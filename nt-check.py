#!/usr/bin/python
# -*- coding: utf-8 -*-

import os
import re
import sys
import time
import traceback
import copy
import string

MSG_FORMAT = "\n\n=====================================================\n{0}\n=====================================================\n"

TMP_DIR = '/tmp/'

def printWRN(txt):
    print(MSG_FORMAT.format(txt))
    
def printMSG(txt):
    print(MSG_FORMAT.format(txt))

def printDBG(txt):
    print(str(txt))

def printERR(txt):
    print(MSG_FORMAT.format('ERROR: ' + txt))
    sys.exit(1)

def printExc(msg=''):
    print("===============================================")
    print("                   EXCEPTION                   ")
    print("===============================================")
    msg = msg + ': \n%s' % traceback.format_exc()
    print(msg)
    print("===============================================")


######################################################################################################################
#                                                    ELF UTILITIES  BEGIN
######################################################################################################################
ELF_MAGIC = '\x7fELF'

#ELFCLASSNONE = 0 # Invalid class
ELFCLASS32 = 1 # 32-bit objects
ELFCLASS64 = 2 # 64-bit objects

EM_386 = 3
EM_860 = 7
EM_X86_64 = 62 # AMD x86-64 architecture

EM_ARM = 40
EM_AARCH64 = 183

EM_MIPS = 8 
EM_SH = 42

ET_NONE = 0 # No file type
ET_REL = 1  # Relocatable file
ET_EXEC = 2 # Executable file
ET_DYN = 3  # Shared object file
ET_CORE = 4 # Core file

# ARM
EF_ARM_EABIMASK = 0XFF000000
EF_ARM_EABI_VER5 = 0x05000000
EF_ARM_ABI_FLOAT_HARD = 0x00000400
EF_ARM_ABI_FLOAT_SOFT = 0x00000200

# MIPS
EF_MIPS_ABI = 0x0000f000
E_MIPS_ABI_O32 = 0x00001000 # The original o32 abi.

EF_MIPS_ARCH = 0xf0000000
E_MIPS_ARCH_32 = 0x50000000 # -mips32
E_MIPS_ARCH_32R2 = 0x70000000 # -mips32r2

# SH4
EF_SH_MACH_MASK = 0x1f
EF_SH4 = 0x9

def ReadStr(stsTable, idx):
    end = stsTable.find('\0', idx)
    if end > -1:
        return stsTable[idx:end]
    return ''

def ReadUint16(tmp, le=True):
    if le: return ord(tmp[1]) << 8 | ord(tmp[0])
    else: return ord(tmp[0]) << 8 | ord(tmp[1])

def ReadUint32(tmp, le=True):
    if le: return ord(tmp[3]) << 24 | ord(tmp[2]) << 16 | ord(tmp[1]) << 8 | ord(tmp[0])
    else: return ord(tmp[0]) << 24 | ord(tmp[1]) << 16 | ord(tmp[2]) << 8 | ord(tmp[3])

def ReadUint64(tmp, le=True):
    if le: return ord(tmp[7]) << 56 | ord(tmp[6]) << 48 | ord(tmp[5]) << 40 | ord(tmp[4]) << 32 | ord(tmp[3]) << 24 | ord(tmp[2]) << 16 | ord(tmp[1]) << 8 | ord(tmp[0])
    else: return ord(tmp[0]) << 56 | ord(tmp[1]) << 48 | ord(tmp[2]) << 40 | ord(tmp[3]) << 32 | ord(tmp[4]) << 24 | ord(tmp[5]) << 16 | ord(tmp[6]) << 8 | ord(tmp[7])

def ReadElfHeader(file):
    ehdr = {}

    tmp = file.read(4)
    if ELF_MAGIC != tmp:
        raise Exception('Wrong magic [%r]!' % tmp)

    tmp = ord(file.read(1))
    if tmp not in (ELFCLASS32, ELFCLASS64):
        raise Exception('Wrong elf class [%r]!' % tmp)

    ehdr['class_bits'] = 32 if tmp == ELFCLASS32 else 64

    # e_ident
    tmp = file.read(11)

    # e_type 
    tmp = ReadUint16(file.read(2))
    if tmp not in (ET_EXEC, ET_DYN):
        raise Exception('Wrong type [%r]!' % tmp)
    ehdr['e_type'] = tmp

    # e_machine
    tmp = ReadUint16(file.read(2))
    ehdr['e_machine'] = tmp

    # e_version
    ehdr['e_version'] = ReadUint32(file.read(4))

    archSize = ehdr['class_bits'] / 8
    archRead = ReadUint32 if archSize == 4 else ReadUint64

    ehdr['e_entry'] = archRead(file.read(archSize))
    ehdr['e_phoff'] = archRead(file.read(archSize))
    ehdr['e_shoff'] = archRead(file.read(archSize)) # e_shoff - Start of section headers

    ehdr['e_flags']     = ReadUint32(file.read(4))
    ehdr['e_ehsize']    = ReadUint16(file.read(2))
    ehdr['e_phentsize'] = ReadUint16(file.read(2))
    ehdr['e_phnum']     = ReadUint16(file.read(2))
    ehdr['e_shentsize'] = ReadUint16(file.read(2)) # e_shentsize - Size of section headers
    ehdr['e_shnum']     = ReadUint16(file.read(2)) # e_shnum -  Number of section headers
    ehdr['e_shstrndx']  = ReadUint16(file.read(2)) # e_shstrndx - Section header string table index

    return ehdr

def ReadElfSectionHeader(file, ehdr):
    shdrTab = []
    archSize = ehdr['class_bits'] / 8
    archRead = ReadUint32 if archSize == 4 else ReadUint64

    for idx in range(ehdr['e_shnum']):
        offset = ehdr['e_shoff'] + idx * ehdr['e_shentsize']
        file.seek(offset)

        shdr = {}
        shdr['sh_name']   = ReadUint32(file.read(4)) # Section name, index in string tbl
        shdr['sh_type']   = ReadUint32(file.read(4)) # Type of section
        shdr['sh_flags']  = archRead(file.read(archSize)) # Miscellaneous section attributes
        shdr['sh_addr']   = archRead(file.read(archSize)) # Section virtual addr at execution
        shdr['sh_offset'] = archRead(file.read(archSize)) # Section file offset
        shdr['sh_size']   = archRead(file.read(archSize)) # Size of section in bytes
        shdr['sh_link']   = ReadUint32(file.read(4)) # Index of another section
        shdr['sh_info']   = ReadUint32(file.read(4)) # Additional section information 
        shdr['sh_addralign'] = archRead(file.read(archSize)) # Section alignment
        shdr['sh_entsize']   = archRead(file.read(archSize)) # Entry size if section holds table
        shdrTab.append(shdr)

    shdr = shdrTab[ehdr['e_shstrndx']]
    file.seek(shdr['sh_offset'])
    data = file.read(shdr['sh_size'])
    for shdr in shdrTab:
        idx = shdr['sh_name']
        if idx >= len(data): shdr['sh_name'] = "<no-name>"
        shdr['sh_name'] = ReadStr(data, idx)

    return shdrTab

def GetElfDynamic(file, shdrTab, archSize):
    SHT_STRTAB = 3
    SHT_DYNAMIC = 6
    DT_NULL = 0
    DT_NEEDED = 1
    DT_RPATH = 15
    DT_RUNPATH = 29
    archRead = ReadUint32 if archSize == 4 else ReadUint64

    strTab = ''
    dynEntries = []
    for shdr in shdrTab:
        if shdr['sh_type'] == SHT_DYNAMIC:
            file.seek(shdr['sh_offset'])
            for idx in range(shdr['sh_entsize']):
                d_tag = archRead(file.read(archSize))
                #printDBG('d_tag: %s' % d_tag)
                if d_tag == DT_NULL:
                    break
                elif d_tag in (DT_NEEDED, DT_RPATH, DT_RUNPATH):
                    dynEntries.append((d_tag, archRead(file.read(archSize))))
        elif shdr['sh_type'] == SHT_STRTAB and '.dynstr' == shdr['sh_name']:
            file.seek(shdr['sh_offset'])
            strTab = file.read(shdr['sh_size'])

    ret = {'needed':[]}
    for item in dynEntries:
        name = ReadStr(strTab, item[1])
        if name:
            if item[0] == DT_NEEDED:
                ret['needed'].append(name)
            elif item[0] == DT_RPATH:
                ret['rpath'] = name
            elif item[0] == DT_RUNPATH:
                ret['runpath'] = name
    return ret

def GetElfAttributes(file, shdrTab, attribsId):
    SHT_ARM_ATTRIBUTES = 0x70000003
    SHT_GNU_ATTRIBUTES=0x6ffffff5
    SHT_MIPS_ABIFLAGS=0x7000002a
    Tag_GNU_MIPS_ABI_FP=4

    if attribsId not in ('aeabi', 'gnu'):
        raise Exception('No supported attribs id: %s' % attribsId)

    def _readLeb128(data, start, end):
        result = 0
        numRead = 0
        shift = 0
        byte = 0

        while start < end:
            byte = ord(data[start])
            numRead += 1

            result |= (byte & 0x7f) << shift

            shift += 7
            if byte < 0x80:
                break
        return numRead, result

    attribs = {}

    for shrd in shdrTab:
        if shrd['sh_type'] in (SHT_GNU_ATTRIBUTES, SHT_ARM_ATTRIBUTES):
            file.seek(shrd['sh_offset'])
            contents = file.read(shrd['sh_size'])
            p = 0
            if contents.startswith('A'):
                p += 1
                sectionLen = shrd['sh_size'] -1
                while sectionLen > 0:
                    attrLen = ReadUint32(contents[p:])
                    p += 4

                    if attrLen > sectionLen:
                        attrLen = sectionLen
                    elif attrLen < 5:
                        break
                    sectionLen -= attrLen
                    attrLen -= 4
                    attrName = ReadStr(contents, p)

                    p += len(attrName) + 1
                    attrLen -= len(attrName) + 1

                    while attrLen > 0 and p < len(contents):
                        if attrLen < 6:
                            sectionLen = 0
                            break
                        tag = ord(contents[p])
                        p += 1
                        size = ReadUint32(contents[p:])
                        if size > attrLen:
                            size = attrLen
                        if size < 6:
                            sectionLen = 0
                            break

                        attrLen -= size
                        end = p + size - 1
                        p += 4

                        if tag == 1 and attrName == "gnu" and attribsId == "gnu": #File Attributes
                            while p < end:
                                # display_gnu_attribute
                                  numRead, tag = _readLeb128(contents, p, end)
                                  p += numRead
                                  if tag == Tag_GNU_MIPS_ABI_FP: 
                                    numRead, val = _readLeb128(contents, p, end)
                                    p += numRead
                                    attribs['GNU_MIPS_ABI_FP'] = val # # Val_GNU_MIPS_ABI_FP_ANY=0, VFP_DOUBLE=1, VFP_SINGLE=2, VFP_SOFT=3, VFP_OLD_64=4, VFP_XX=5, VFP_64=6,VFP_64A=7, VFP_NAN2008=8
                                    break
                        elif tag == 1 and attrName == "aeabi" and attribsId == "aeabi": #File Attributes
                            while p < end:
                                numRead, tag = _readLeb128(contents, p, end)
                                p += numRead
                                if tag in (4, 5, 67):
                                    strVal = ReadStr(contents, p)
                                    p += len(strVal) + 1
                                    printDBG('[1] tag [%s] %s' % (tag, strVal))
                                    if tag == 4: # Tag_CPU_raw_name
                                        attribs['CPU_raw_name'] = strVal
                                    elif tag == 5: # Tag_CPU_name
                                        attribs['CPU_name'] = strVal
                                elif tag in (7, 24, 25, 32, 65, 6,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,26,27,28,29,30,31,34,36,38,42,44,46,66,68,70,):
                                    numRead, val = _readLeb128(contents, p, end)
                                    p += numRead
                                    printDBG('[2] tag [%d] %s' % (tag, val))
                                else:
                                    raise Exception('Unknown tag %s!' % tag)

                                if tag == 10: # Tag_FP_arch["No","VFPv1","VFPv2","VFPv3","VFPv3-D16","VFPv4","VFPv4-D16","FP for ARMv8","FPv5/FP-D16 for ARMv8"]
                                    attribs['FP_arch'] = val
                                elif tag == 28: # Tag_ABI_VFP_args["AAPCS","VFP registers","custom","compatible"]
                                    attribs['ABI_VFP_args'] = val
                                elif tag == 6: # Tag_CPU_arch["Pre-v4","v4","v4T","v5T","v5TE","v5TEJ","v6","v6KZ","v6T2","v6K","v7","v6-M","v6S-M","v7E-M","v8","v8-R","v8-M.baseline","v8-M.mainline"]
                                    attribs['CPU_arch'] = val
                                elif tag == 9: # Tag_THUMB_ISA_use["No","Thumb-1","Thumb-2","Yes"]
                                    attribs['THUMB_ISA_use'] = val
                                elif tag in (8,11,12,13,14,15,16,17,18,19,20,21,22,23,26,27,29,30,31,34,36,38,42,44,46,66,68,70,):
                                    # val is index of array
                                    pass
                                elif tag == 65:
                                    if val == 6:
                                        numRead, val = _readLeb128(contents, p, end)
                                        p += numRead
                                    else:
                                        strVal = ReadStr(contents, p)
                                        p += len(strVal) + 1
                                elif tag == 32:
                                    if p < end - 1:
                                        strVal = ReadStr(contents, p)
                                        p += len(strVal) + 1
                                    else:
                                        p = end
                        elif p < end:
                            p = end
                        else:
                            attrLen = 0
            elif sh_type == SHT_MIPS_ABIFLAGS:
                attribs['HAS_MIPS_ABI_FLAGS'] = True
    return attribs

def GetMappedFiles(pid=None):
    libs = []
    try:
        if pid == None:
            pid = os.getpid()

        with open('/proc/%s/maps' % pid, "r") as file:
            line = file.readline()
            while line:
                line = line[line.rfind(' ')+1:-1]
                if line.startswith('/') and line not in libs:
                    libs.append(line)
                line = file.readline()
    except Exception:
        printExc()
    return libs

def GetCurrentExec():
    return sys.executable

def GetPlatformInfo():
    # we need to select one of the platform
    info = {}
    try:
        libcPath = ''
        ldPath = ''
        tmp = GetMappedFiles()
        for it in tmp:
            t = it.rsplit('/', 1)[-1]
            if t.startswith('libc-'):
                libcPath = it
            if t.startswith('ld-'):
                ldPath = it
        info['libc_path'] = libcPath
        info['ld_path'] = ldPath

        glibcVersion = re.search("libc\-([0-9]+)\.([0-9]+)\.", info['libc_path'])
        glibcVersion = int(glibcVersion.group(1)) * 100 + int(glibcVersion.group(2))
        info['libc_ver'] = glibcVersion

        with open(libcPath, "rb") as file:
            ehdr = ReadElfHeader(file)
            info['arch_bits'] =  ehdr['class_bits']
            shdrTab = ReadElfSectionHeader(file, ehdr)
            if EM_ARM == ehdr['e_machine']:
                fattribs = GetElfAttributes(file, shdrTab, 'aeabi')
                if (ehdr['e_flags'] & EF_ARM_EABI_VER5) != EF_ARM_EABI_VER5:
                    raise Exception('ARM unsupported EABI [%r]!' % (ehdr['e_flags'] & EF_ARM_EABIMASK))

                if (ehdr['e_flags'] & EF_ARM_ABI_FLOAT_HARD) == EF_ARM_ABI_FLOAT_HARD:
                    fputype = 'hard'
                elif (ehdr['e_flags'] & EF_ARM_ABI_FLOAT_SOFT) == EF_ARM_ABI_FLOAT_SOFT:
                    fputype = 'softfp' if 0 != fattribs.get('FP_arch', 0) else 'soft'
                else:
                    raise Exception('Unknown ARM FPU ABI [%r]!' % ehdr['e_flags'])

                info['fpu_type'] = fputype
                if fputype == 'hard':
                    info['platform'] = 'armv7'
                elif fputype == 'soft':
                    info['platform'] = 'armv5t'
                else:
                    # this is not optimal but we will use soft_fpu binaries in such situation
                    info['platform'] = 'armv5t'
            elif EM_AARCH64 == ehdr['e_machine']:
                info['platform'] = 'aarch64'
                info['fpu_type'] = 'hard'
            elif EM_MIPS == ehdr['e_machine']:
                fattribs = GetElfAttributes(file, shdrTab, 'gnu')
                if (ehdr['e_flags'] & EF_MIPS_ABI) != E_MIPS_ABI_O32:
                    raise Exception('Not supported MIPS ABI [%r]!' % (ehdr['e_flags'] & EF_MIPS_ABI))
                if (ehdr['e_flags'] & EF_MIPS_ARCH) not in (E_MIPS_ARCH_32, E_MIPS_ARCH_32R2): # binary compiled for mips32 should works on mips32r2
                    raise Exception('Not supported MIPS ARCH [%r]!' % (ehdr['e_flags'] & EF_MIPS_ARCH))
                info['platform'] = 'mipsel'
                abiFP = fattribs.get('GNU_MIPS_ABI_FP', -1)
                if abiFP == 3: fputype = 'soft'
                elif abiFP not in (-1, 0): fputype = 'hard'
                else:
                    printDBG('GNU_MIPS_ABI_FP not available try to guess based on /proc/cpuinfo')
                    with open('/proc/cpuinfo', 'r') as f:
                        data = f.read().strip().upper()
                    fputype = 'hard' if ' FPU ' in data or info['libc_ver'] < 220 else 'soft'
                info['fpu_type'] = fputype
            elif EM_SH == ehdr['e_machine']:
                if (ehdr['e_flags'] & EF_SH_MACH_MASK) != EF_SH4:
                    raise Exception('Not supported SH ARCH [%r]!' % (ehdr['e_flags'] & EF_SH_MACH_MASK))
                info['platform'] = 'sh4'
                info['fpu_type'] = 'hard'
            elif EM_386 == ehdr['e_machine']:
                info['platform'] = 'i686'
                info['fpu_type'] = 'hard'
            else:
                raise Exception("Not supported architecture: %r" % ehdr['e_machine'])
    except Exception:
        printExc()
    return info
######################################################################################################################
#                                                    ELF UTILITIES  END
######################################################################################################################

platformInfo = GetPlatformInfo()
e2iPlatform = platformInfo['platform']
glibcVer = platformInfo['libc_ver']
fpuType = platformInfo['fpu_type']

pyVersion = 'python%s.%s' % (sys.version_info[0], sys.version_info[1])
if pyVersion not in ['python2.7', 'python2.6']:
    raise Exception('Your python version "%s" is not supported!' % pyVersion)

if e2iPlatform in ['sh4', 'mipsel'] and glibcVer < 220:
    installOld = 'old_'
else:
    installOld = ''

installFPU = 'fpu_%s' % fpuType

def downloadWithWget(url, output):
    if os.path.exists(output):
        if not os.path.isfile(output) or os.path.islink(output):
            printERR("%s exists - please remove it to be able to install!" % output)
        os.unlink(output)

    for cmd in ['/iptvplayer_rootfs/usr/bin/wget --no-check-certificate', 'wget', 'wget --no-check-certificate', 'fullwget --no-check-certificate']:
        try:
            file = os.popen(cmd + ' "%s" -O "%s" ' % (url, output))
            data = file.read()
            ret = file.close()
            if ret in [0, None]:
                return
            else:
                printWRN("Download using %s failed with return code: %s" % (url, ret))
        except Exception,e:
            printDBG(e)

    printERR('Download %s failed!' % url)

try:
    sys.stdin = open('/dev/tty')
except Exception:
    pass


packageConfig = '_%sfpu_%s' % (installOld, fpuType)
url = 'https://www.e2iplayer.gitlab.io/resources/packages/bin/%s/e2isec%s' % (e2iPlatform, packageConfig)
e2isec = os.path.join(TMP_DIR, 'e2i')
downloadWithWget(url, e2isec)
os.chmod(e2isec, 0755)

os.system(e2isec + ' getmac > ' + e2isec + '.out')
with open(e2isec + '.out') as f:
    macAddr = f.read()

os.system(e2isec + ' getmac2 > ' + e2isec + '.out')
with open(e2isec + '.out') as f:
    macAddr2 = f.read()

os.system(e2isec + ' uname > ' + e2isec + '.out')
with open(e2isec + '.out') as f:
    uname = f.read()

printMSG('\n'.join(["PLATFORM INFO", 'uname:\t' + uname, 'config:\t' + e2iPlatform + packageConfig, 'python:\t' + pyVersion, 'mac:\t' + macAddr, 'mac2:\t' + macAddr2]))

