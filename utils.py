#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys
import os
import re
import traceback
import time

INSTALL_BASE = '/iptvplayer_rootfs/'
DEBUG = os.environ.get('E2I_DEBUG')

IS_PY3 = sys.version_info[0] == 3
if IS_PY3:
    _ord=ord
    def ord(c):
        if type(c) is int: return int(c)
        else: return int(_ord(c))
    raw_input=input
    xrange=range

MSG_FORMAT = "\n\n=====================================================\n{0}\n=====================================================\n"

try:
    sys.stdin = open('/dev/tty')
except Exception:
    pass

class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def ask(msg):
    answer = ''
    msg = MSG_FORMAT.format(msg) + '\n'
    while answer not in ['Y', 'N']:
        if not msg:
            print('\033[1K')
        answer = raw_input(msg + ("%sY%s/%sN%s: " % (bcolors.OKGREEN, bcolors.ENDC, bcolors.WARNING, bcolors.ENDC))).strip().upper()
        msg = ''
    return answer == 'Y'

def printColor(txt, color=None):
    print(color + txt + bcolors.ENDC)

def printWRN(txt, format=None):
    if None == format: format = MSG_FORMAT
    print(format.format(bcolors.WARNING + txt + bcolors.ENDC))

def printMSG(txt, color=None):
    if color == None:
        color = bcolors.OKGREEN
        if ' cancel' in txt or ' skip' in txt:
            color = bcolors.WARNING
    print(MSG_FORMAT.format(color + txt + bcolors.ENDC))

def printDBG(txt):
    if DEBUG:
        print(str(txt))

def printExc(msg=''):
    printColor("===============================================", bcolors.FAIL)
    printColor("                   EXCEPTION                   ", bcolors.FAIL)
    printColor("===============================================", bcolors.FAIL)
    msg = msg + ': \n%s' % traceback.format_exc()
    print(msg)
    printColor("===============================================", bcolors.FAIL)

def printFatal(msg='', errorCode=-1):
    printColor("===============================================", bcolors.FAIL)
    printColor("                     FATAL                     ", bcolors.FAIL)
    printColor("===============================================", bcolors.FAIL)
    print(msg)
    printColor("===============================================", bcolors.FAIL)
    sys.exit(errorCode)

######################################################################################################################
#                                                    ELF UTILITIES  BEGIN
######################################################################################################################
ELF_MAGIC = b'\x7fELF'

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
    end = stsTable.find(b'\0', idx)
    if end > -1:
        return a(stsTable[idx:end])
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

    archSize = ehdr['class_bits'] // 8
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
    archSize = ehdr['class_bits'] // 8
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
            if contents.startswith(b'A'):
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
        libsPaths = []
        libcPath = ''
        libcPathAlt = ''
        ldPath = ''
        tmp = GetMappedFiles()
        for it in tmp:
            p, t = it.rsplit('/', 1)
            if t.startswith('libc-'):
                libcPath = it
            if t.startswith('libc.'):
                libcPathAlt = it
            if t.startswith('ld-'):
                ldPath = it
            if '/lib' in p and t.startswith('lib') and p not in libsPaths:
                libsPaths.append(p)
        info['libc_path'] = libcPath if libcPath else libcPathAlt
        info['ld_path'] = ldPath
        info['libs_paths'] = libsPaths

        glibcVersion = re.search("libc\-([0-9]+)\.([0-9]+)\.", info['libc_path'])
        if not glibcVersion:
            file = os.popen('%s --version 2>&1' % info['libc_path'])
            glibcVersion = file.read()
            file.close()
            glibcVersion = re.search("stable release version ([0-9]+)\.([0-9]+)", glibcVersion)

        glibcVersion = int(glibcVersion.group(1)) * 100 + int(glibcVersion.group(2))
        info['libc_ver'] = glibcVersion

        with open(info['libc_path'], "rb") as file:
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
    except Exception as e:
        printExc(str(e))
    return info
######################################################################################################################
#                                                    ELF UTILITIES  END
######################################################################################################################

def getLibsPaths(installBase, platformInfo):
    libsPaths = [installBase + 'usr/lib/', installBase + 'lib/']
    if 64 == platformInfo['arch_bits']:
        libsPaths.extend(['/lib64/', '/usr/lib64/'])
    else:
        libsPaths.extend(['/lib/', '/usr/lib/'])

    for item in platformInfo.get('libs_paths', []):
        if item not in libsPaths:
            libsPaths.append(item + '/')
    return libsPaths

def getPackageConfig(platformInfo=None):
    if platformInfo == None:
        platformInfo = GetPlatformInfo()

    e2iPlatform = platformInfo['platform']
    glibcVer = platformInfo['libc_ver']
    fpuType = platformInfo['fpu_type']

    if e2iPlatform in ['sh4', 'mipsel'] and glibcVer < 220:
        installOld = 'old_'
    else:
        installOld = ''

    installFPU = 'fpu_%s' % fpuType

    return '%s_%s%s' % (e2iPlatform, installOld, installFPU)

def checkPyVersion():
    pyVersion = 'python%s.%s' % (sys.version_info[0], sys.version_info[1])
    if pyVersion not in ['python2.7', 'python2.6', 'python3.8', 'python3.9']:
        printFatal('Your python version "%s" is not supported!' % pyVersion)
    return pyVersion

def downloadUrl(url, out):
    printMSG('Downloading "%s" please wait.' % url.split('?', 1)[0], '{}', bcolors.UNDERLINE)

    wget = INSTALL_BASE + 'usr/bin/wget'
    listToCheck = ['wget --no-check-certificate', 'wget']
    if os.path.isfile(wget):
        listToCheck.insert(0, '%s --no-check-certificate' % wget)

    if '?' not in url:
        url += '?_=' + str(time.time())

    errorMsg = []
    errorCodes = []
    wget = ''
    for cmd in listToCheck:
        try:
            tmpCmd = cmd + ' "%s" -O "%s" ' % (url, out)
            if not DEBUG:
                tmpCmd += " 2>&1"
            printDBG('Try: ' + tmpCmd)
            file = os.popen(tmpCmd)
            data = file.read()
            ret = file.close()
            if ret in [0, None]:
                wget = cmd
                break
            elif not DEBUG:
                if '--no-check-certificate' in data:
                    continue
                ret = str(ret)
                if ret not in errorCodes:
                    errorCodes.append(ret)
                else:
                    continue

                data = data.split('\n')
                for it in reversed(data):
                    if not it.strip():
                        continue
                    if it not in errorMsg:
                        errorMsg.append(it)
                    break
            else:
                printDBG("Download using %s failed with return code: %s" % (cmd, ret))

        except Exception as e:
            printDBG(e)
    if errorCodes:
        printWRN("Download failed %s: %s" % (', '.join(errorCodes), ', '.join(errorMsg)), '{}')
    return wget

def checkFreeSpace(requiredFreeSpaceMB, packageName, allowForce=True, ):
    # check free size in the rootfs
    if packageName == 'E2iPlayer':
        s = os.statvfs(INSTALL_PATH_BASE) if os.path.isdir(INSTALL_PATH_BASE) else os.statvfs(INSTALL_PATH_BASE.rsplit('/', 1)[0])
    else:
        s = os.statvfs(INSTALL_BASE) if os.path.isdir(INSTALL_BASE) else os.statvfs("/")

    freeSpaceMB = s.f_bfree * s.f_frsize // (1024*1024) # in KB
    availSpaceMB = s.f_bavail * s.f_frsize // (1024*1024) # in KB

    printDBG("Free space %s MB in rootfs" % (availSpaceMB))
    if availSpaceMB < requiredFreeSpaceMB:
        msg = "Not enough disk space for installing %s!\nAt least %s MB is required.\nYou have %s MB free space in the rootfs.\nDo you want to continue anyway?" % (packageName, requiredFreeSpaceMB, availSpaceMB)
        if not allowForce or not ask(msg):
            printFatal("Not enough disk space for installing %s!\nAt least %s MB is required." % (packageName, requiredFreeSpaceMB))

def GetOpenSSLVer(platformInfo):
    libsPaths = getLibsPaths(INSTALL_BASE, platformInfo)

    libsslPath = ''
    libcryptoPath = ''
    e2iOpenSSLVer = ''
    for ver in ['1.1', '1.0.2', '1.0.0', '0.9.8']:
        libsslExist = False
        libcryptoExist = False
        libsslPath = ''
        libcryptoPath = ''
        for path in libsPaths:
            filePath = path + 'libssl.so.' + ver
            if os.path.isfile(filePath) and not os.path.islink(filePath):
                libsslExist = True
                libsslPath = path

            filePath = path + 'libcrypto.so.' + ver
            if os.path.isfile(filePath) and not os.path.islink(filePath):
                libcryptoExist = True
                libcryptoPath = path

            if libsslExist and libcryptoExist:
                e2iOpenSSLVer = ver
                break

        if e2iOpenSSLVer:
            break

    printDBG("OpenSSL SONAME VERSION [%s]" % e2iOpenSSLVer)
    if e2iOpenSSLVer == '1.0.2':
        linksTab = []
        symlinksText = []
        if not os.path.isfile(libsslPath + 'libssl.so.' + e2iOpenSSLVer):
            linksTab.append((libsslPath + 'libssl.so.' + e2iOpenSSLVer, libsslPath + 'libssl.so.1.0.0'))
            symlinksText.append('%s -> %s' % linksTab[-1])

        if not os.path.isfile(libcryptoPath + 'libcrypto.so.' + e2iOpenSSLVer):
            linksTab.append((libcryptoPath + 'libcrypto.so.' + e2iOpenSSLVer, libcryptoPath + 'libcrypto.so.1.0.0'))
            symlinksText.append('%s -> %s' % linksTab[-1])

        if len(linksTab):
            msg = "OpenSSL in your image has different library names then these used by E2iPlayer.\nThere is need to create following symlinks:\n%s\nto be able to install binary components from E2iPlayer server.\nDo you want to proceed?" % ('\n'.join(symlinksText))
            answer = ''
            while answer not in ['Y', 'N']:
                answer = raw_input(MSG_FORMAT.format(msg) + "\nY/N: ").strip().upper()
                msg = ''

            if answer == 'Y':
                for item in linksTab:
                    os.symlink(item[0], item[1])
            else:
                printFatal('Your OpenSSL version is not supported!')
    elif e2iOpenSSLVer == '1.0.0':
        with open(libsslPath + 'libssl.so.' + e2iOpenSSLVer, "rb") as file:
            if b'OPENSSL_1.0.2' in file.read():
                e2iOpenSSLVer = '1.0.2'

    printDBG("OpenSSL VERSION [%s]" % e2iOpenSSLVer)
    return e2iOpenSSLVer

if not DEBUG:
    os.system('clear')
