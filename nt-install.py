#!/usr/bin/python
# -*- coding: utf-8 -*-

import os
import re
import sys
import time
import traceback
import copy
import string
from hashlib import sha256

INSTALL_BASE = '/iptvplayer_rootfs/'

INSTALL_PATH_BASE = '/usr/lib/enigma2/python/Plugins/Extensions/IPTVPlayer'
INSTALL_URL_BASE = 'https://www.e2iplayer.gitlab.io/'
UPDATE_URL_BASE = INSTALL_URL_BASE + 'update_nt/'

MSG_FORMAT = "\n\n=====================================================\n{0}\n=====================================================\n"

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

sitePackagesPath='/usr/lib%s/%s/site-packages' % (pyVersion, '64' if 64 == platformInfo['arch_bits'] else '')
for f in sys.path:
    if f.endswith('packages') and os.path.isdir(f):
        sitePackagesPath = f

if not os.path.isdir(sitePackagesPath):
    raise Exception('Python site-packages directory "%s" does not exists!\nPlease report this via e-mail: e2iplayer@yahoo.com' % sitePackagesPath)

printDBG("sitePackagesPath %s" % sitePackagesPath)

################################################################
# AES START
################################################################
class Rijndael(object):

    @classmethod
    def create(cls):

        if hasattr(cls, "RIJNDAEL_CREATED"):
            return

        cls.num_rounds = {16: {16: 10, 24: 12, 32: 14}, 24: {16: 12, 24: 12, 32: 14}, 32: {16: 14, 24: 14, 32: 14}}

        cls.shifts = [[[0, 0], [1, 3], [2, 2], [3, 1]],
                [[0, 0], [1, 5], [2, 4], [3, 3]],
                [[0, 0], [1, 7], [3, 5], [4, 4]]]

        A = [[1, 1, 1, 1, 1, 0, 0, 0],
            [0, 1, 1, 1, 1, 1, 0, 0],
            [0, 0, 1, 1, 1, 1, 1, 0],
            [0, 0, 0, 1, 1, 1, 1, 1],
            [1, 0, 0, 0, 1, 1, 1, 1],
            [1, 1, 0, 0, 0, 1, 1, 1],
            [1, 1, 1, 0, 0, 0, 1, 1],
            [1, 1, 1, 1, 0, 0, 0, 1]]

        alog = [1]
        for i in xrange(255):
            j = (alog[-1] << 1) ^ alog[-1]
            if j & 0x100 != 0:
                j ^= 0x11B
            alog.append(j)

        log = [0] * 256
        for i in xrange(1, 255):
            log[alog[i]] = i

        def mul(a, b):
            if a == 0 or b == 0:
                return 0
            return alog[(log[a & 0xFF] + log[b & 0xFF]) % 255]

        box = [[0] * 8 for i in xrange(256)]
        box[1][7] = 1
        for i in xrange(2, 256):
            j = alog[255 - log[i]]
            for t in xrange(8):
                box[i][t] = (j >> (7 - t)) & 0x01

        B = [0, 1, 1, 0, 0, 0, 1, 1]

        cox = [[0] * 8 for i in xrange(256)]
        for i in xrange(256):
            for t in xrange(8):
                cox[i][t] = B[t]
                for j in xrange(8):
                    cox[i][t] ^= A[t][j] * box[i][j]

        cls.S =  [0] * 256
        cls.Si = [0] * 256
        for i in xrange(256):
            cls.S[i] = cox[i][0] << 7
            for t in xrange(1, 8):
                cls.S[i] ^= cox[i][t] << (7-t)
            cls.Si[cls.S[i] & 0xFF] = i

        G = [[2, 1, 1, 3],
            [3, 2, 1, 1],
            [1, 3, 2, 1],
            [1, 1, 3, 2]]

        AA = [[0] * 8 for i in xrange(4)]

        for i in xrange(4):
            for j in xrange(4):
                AA[i][j] = G[i][j]
                AA[i][i+4] = 1

        for i in xrange(4):
            pivot = AA[i][i]
            if pivot == 0:
                t = i + 1
                while AA[t][i] == 0 and t < 4:
                    t += 1
                    assert t != 4, 'G matrix must be invertible'
                    for j in xrange(8):
                        AA[i][j], AA[t][j] = AA[t][j], AA[i][j]
                    pivot = AA[i][i]
            for j in xrange(8):
                if AA[i][j] != 0:
                    AA[i][j] = alog[(255 + log[AA[i][j] & 0xFF] - log[pivot & 0xFF]) % 255]
            for t in xrange(4):
                if i != t:
                    for j in xrange(i+1, 8):
                        AA[t][j] ^= mul(AA[i][j], AA[t][i])
                    AA[t][i] = 0

        iG = [[0] * 4 for i in xrange(4)]

        for i in xrange(4):
            for j in xrange(4):
                iG[i][j] = AA[i][j + 4]

        def mul4(a, bs):
            if a == 0:
                return 0
            r = 0
            for b in bs:
                r <<= 8
                if b != 0:
                    r = r | mul(a, b)
            return r

        cls.T1 = []
        cls.T2 = []
        cls.T3 = []
        cls.T4 = []
        cls.T5 = []
        cls.T6 = []
        cls.T7 = []
        cls.T8 = []
        cls.U1 = []
        cls.U2 = []
        cls.U3 = []
        cls.U4 = []

        for t in xrange(256):
            s = cls.S[t]
            cls.T1.append(mul4(s, G[0]))
            cls.T2.append(mul4(s, G[1]))
            cls.T3.append(mul4(s, G[2]))
            cls.T4.append(mul4(s, G[3]))

            s = cls.Si[t]
            cls.T5.append(mul4(s, iG[0]))
            cls.T6.append(mul4(s, iG[1]))
            cls.T7.append(mul4(s, iG[2]))
            cls.T8.append(mul4(s, iG[3]))

            cls.U1.append(mul4(t, iG[0]))
            cls.U2.append(mul4(t, iG[1]))
            cls.U3.append(mul4(t, iG[2]))
            cls.U4.append(mul4(t, iG[3]))

        cls.rcon = [1]
        r = 1
        for t in xrange(1, 30):
            r = mul(2, r)
            cls.rcon.append(r)

        cls.RIJNDAEL_CREATED = True

    def __init__(self, key, block_size = 16):

        self.create()

        if block_size != 16 and block_size != 24 and block_size != 32:
            raise ValueError('Invalid block size: ' + str(block_size))
        if len(key) != 16 and len(key) != 24 and len(key) != 32:
            raise ValueError('Invalid key size: ' + str(len(key)))
        self.block_size = block_size

        ROUNDS = Rijndael.num_rounds[len(key)][block_size]
        BC = block_size / 4
        Ke = [[0] * BC for i in xrange(ROUNDS + 1)]
        Kd = [[0] * BC for i in xrange(ROUNDS + 1)]
        ROUND_KEY_COUNT = (ROUNDS + 1) * BC
        KC = len(key) / 4

        tk = []
        for i in xrange(0, KC):
            tk.append((ord(key[i * 4]) << 24) | (ord(key[i * 4 + 1]) << 16) |
                (ord(key[i * 4 + 2]) << 8) | ord(key[i * 4 + 3]))

        t = 0
        j = 0
        while j < KC and t < ROUND_KEY_COUNT:
            Ke[t / BC][t % BC] = tk[j]
            Kd[ROUNDS - (t / BC)][t % BC] = tk[j]
            j += 1
            t += 1
        tt = 0
        rconpointer = 0
        while t < ROUND_KEY_COUNT:
            tt = tk[KC - 1]
            tk[0] ^= (Rijndael.S[(tt >> 16) & 0xFF] & 0xFF) << 24 ^  \
                     (Rijndael.S[(tt >>  8) & 0xFF] & 0xFF) << 16 ^  \
                     (Rijndael.S[ tt        & 0xFF] & 0xFF) <<  8 ^  \
                     (Rijndael.S[(tt >> 24) & 0xFF] & 0xFF)       ^  \
                     (Rijndael.rcon[rconpointer]    & 0xFF) << 24
            rconpointer += 1
            if KC != 8:
                for i in xrange(1, KC):
                    tk[i] ^= tk[i-1]
            else:
                for i in xrange(1, KC / 2):
                    tk[i] ^= tk[i-1]
                tt = tk[KC / 2 - 1]
                tk[KC / 2] ^= (Rijndael.S[ tt        & 0xFF] & 0xFF)       ^ \
                              (Rijndael.S[(tt >>  8) & 0xFF] & 0xFF) <<  8 ^ \
                              (Rijndael.S[(tt >> 16) & 0xFF] & 0xFF) << 16 ^ \
                              (Rijndael.S[(tt >> 24) & 0xFF] & 0xFF) << 24
                for i in xrange(KC / 2 + 1, KC):
                    tk[i] ^= tk[i-1]
            j = 0
            while j < KC and t < ROUND_KEY_COUNT:
                Ke[t / BC][t % BC] = tk[j]
                Kd[ROUNDS - (t / BC)][t % BC] = tk[j]
                j += 1
                t += 1
        for r in xrange(1, ROUNDS):
            for j in xrange(BC):
                tt = Kd[r][j]
                Kd[r][j] = Rijndael.U1[(tt >> 24) & 0xFF] ^ \
                           Rijndael.U2[(tt >> 16) & 0xFF] ^ \
                           Rijndael.U3[(tt >>  8) & 0xFF] ^ \
                           Rijndael.U4[ tt        & 0xFF]
        self.Ke = Ke
        self.Kd = Kd

    def encrypt(self, plaintext):
        if len(plaintext) != self.block_size:
            raise ValueError('wrong block length, expected ' + str(self.block_size) + ' got ' + str(len(plaintext)))
        Ke = self.Ke

        BC = self.block_size / 4
        ROUNDS = len(Ke) - 1
        if BC == 4:
            Rijndael.SC = 0
        elif BC == 6:
            Rijndael.SC = 1
        else:
            Rijndael.SC = 2
        s1 = Rijndael.shifts[Rijndael.SC][1][0]
        s2 = Rijndael.shifts[Rijndael.SC][2][0]
        s3 = Rijndael.shifts[Rijndael.SC][3][0]
        a = [0] * BC
        t = []
        for i in xrange(BC):
            t.append((ord(plaintext[i * 4    ]) << 24 |
                      ord(plaintext[i * 4 + 1]) << 16 |
                      ord(plaintext[i * 4 + 2]) <<  8 |
                      ord(plaintext[i * 4 + 3])        ) ^ Ke[0][i])
        for r in xrange(1, ROUNDS):
            for i in xrange(BC):
                a[i] = (Rijndael.T1[(t[ i           ] >> 24) & 0xFF] ^
                        Rijndael.T2[(t[(i + s1) % BC] >> 16) & 0xFF] ^
                        Rijndael.T3[(t[(i + s2) % BC] >>  8) & 0xFF] ^
                        Rijndael.T4[ t[(i + s3) % BC]        & 0xFF]  ) ^ Ke[r][i]
            t = copy.copy(a)
        result = []
        for i in xrange(BC):
            tt = Ke[ROUNDS][i]
            result.append((Rijndael.S[(t[ i           ] >> 24) & 0xFF] ^ (tt >> 24)) & 0xFF)
            result.append((Rijndael.S[(t[(i + s1) % BC] >> 16) & 0xFF] ^ (tt >> 16)) & 0xFF)
            result.append((Rijndael.S[(t[(i + s2) % BC] >>  8) & 0xFF] ^ (tt >>  8)) & 0xFF)
            result.append((Rijndael.S[ t[(i + s3) % BC]        & 0xFF] ^  tt       ) & 0xFF)
        return string.join(map(chr, result), '')

    def decrypt(self, ciphertext):
        if len(ciphertext) != self.block_size:
            raise ValueError('wrong block length, expected ' + str(self.block_size) + ' got ' + str(len(ciphertext)))
        Kd = self.Kd

        BC = self.block_size / 4
        ROUNDS = len(Kd) - 1
        if BC == 4:
            Rijndael.SC = 0
        elif BC == 6:
            Rijndael.SC = 1
        else:
            Rijndael.SC = 2
        s1 = Rijndael.shifts[Rijndael.SC][1][1]
        s2 = Rijndael.shifts[Rijndael.SC][2][1]
        s3 = Rijndael.shifts[Rijndael.SC][3][1]
        a = [0] * BC
        t = [0] * BC
        for i in xrange(BC):
            t[i] = (ord(ciphertext[i * 4    ]) << 24 |
                    ord(ciphertext[i * 4 + 1]) << 16 |
                    ord(ciphertext[i * 4 + 2]) <<  8 |
                    ord(ciphertext[i * 4 + 3])        ) ^ Kd[0][i]
        for r in xrange(1, ROUNDS):
            for i in xrange(BC):
                a[i] = (Rijndael.T5[(t[ i           ] >> 24) & 0xFF] ^
                        Rijndael.T6[(t[(i + s1) % BC] >> 16) & 0xFF] ^
                        Rijndael.T7[(t[(i + s2) % BC] >>  8) & 0xFF] ^
                        Rijndael.T8[ t[(i + s3) % BC]        & 0xFF]  ) ^ Kd[r][i]
            t = copy.copy(a)
        result = []
        for i in xrange(BC):
            tt = Kd[ROUNDS][i]
            result.append((Rijndael.Si[(t[ i           ] >> 24) & 0xFF] ^ (tt >> 24)) & 0xFF)
            result.append((Rijndael.Si[(t[(i + s1) % BC] >> 16) & 0xFF] ^ (tt >> 16)) & 0xFF)
            result.append((Rijndael.Si[(t[(i + s2) % BC] >>  8) & 0xFF] ^ (tt >>  8)) & 0xFF)
            result.append((Rijndael.Si[ t[(i + s3) % BC]        & 0xFF] ^  tt       ) & 0xFF)
        return string.join(map(chr, result), '')
################################################################
# AES END
################################################################
def decryptFile(file, key):
    cipher = Rijndael(key, block_size=len(key))

    data = ''
    with open(file, "rb") as f:
        data = f.read()

    with open(file, "wb") as f:
        offset = 0
        while True:
            for enc in [True, False]:
                if enc:
                    chunk = data[offset:offset+len(key)]
                    offset += len(key)
                else:
                    chunk = data[offset:offset+len(key)*1000]
                    offset += len(key)*1000

                if len(chunk) == len(key):
                    chunk = cipher.decrypt(chunk)

                f.write(chunk)
            if not chunk:
                break
    f.close()

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

def unpackArchive(input, output, removeInputAfterUnpack=True, clearOutputBeforeUnpack=True):
    if not os.path.isfile(input) or os.path.islink(input):
        printERR('input "%s" must be file!' % input)

    if not os.path.isdir(output):
        os.mkdir(output)

    if '/' == os.path.realpath(output):
        printERR('output directory can not be "/" !')

    cmd = ''
    if clearOutputBeforeUnpack: cmd += 'rm -rf "%s/"* > /dev/null 2>&1; ' % output
    cmd += 'tar -xzf "%s" -C "%s" 2>&1; ' % (input, output) 
    if removeInputAfterUnpack: cmd += 'PREV_RET=$?; rm -f "%s" > /dev/null 2>&1; (exit $PREV_RET)' % (input)

    printDBG("unpacking: " + cmd)
    file = os.popen(cmd)
    data = file.read()
    ret = file.close()
    if ret in [0, None]:
        return
    else:
        printERR('Unpacking "%s" into "%s" failed with return code: %s.\nPlease check your password.' % (input, output, ret))

def copyDirContent(input, output, removeInputAfterUnpack=True, clearOutputBeforeUnpack=True):
    if not os.path.isdir(input):
        printERR('input "%s" is not directory!' % input)

    if not os.path.isdir(output):
        os.mkdir(output)

    if '/' in (os.path.realpath(output), os.path.realpath(input)):
        printERR('output and input directory can not be "/" !')

    cmd = ''
    if clearOutputBeforeUnpack: cmd += 'rm -rf "%s/"* > /dev/null 2>&1; ' % output
    cmd += 'cp -r "%s/"* "%s/" 2>&1; ' % (input, output) 
    if removeInputAfterUnpack: cmd += 'PREV_RET=$?; rm -rf "%s" > /dev/null 2>&1; (exit $PREV_RET)' % (input)

    printDBG("copy: " + cmd)
    file = os.popen(cmd)
    data = file.read()
    ret = file.close()
    if ret in [0, None]:
        return
    else:
        printERR('Copying content of "%s" into "%s" failed with return code: %s.' % (input, output, ret))

def getE2iPlayerVersion(E2iPlayerPath):
    sys.path.insert(0, E2iPlayerPath)
    version = __import__('version', globals(), locals(), ['E2I_VERSION'], -1)
    del sys.path[0]
    return version.E2I_VERSION

try:
    sys.stdin = open('/dev/tty')
except Exception:
    pass

if len(sys.argv) != 4:
    printERR("You need to provide login, password and icon size (100, 120, 135)!")

LOGIN = sys.argv[1]
PASSWORD = sys.argv[2]
ICONS_SIZE = sys.argv[3]
TMP_DIR = '/tmp/'

AVAILABLE_ICONS_SIZES = ['100', '120', '135']
if ICONS_SIZE not in AVAILABLE_ICONS_SIZES:
    printERR('Wrong icon size "%s", available sizes %s' % (ICONS_SIZE, ', '.join(AVAILABLE_ICONS_SIZES)))

# Get Enc Key 
nameHash = sha256(LOGIN).hexdigest()

url = INSTALL_URL_BASE + 'keys_nt/%s.bin' % nameHash
encKey = os.path.join(TMP_DIR, 'e2iplayer_enc_key.bin')

downloadWithWget(url, encKey)
with open(encKey, "r") as f:
    encKey = f.read()

key = sha256(PASSWORD).digest()[:16]
cipher = Rijndael(key, block_size=len(key))
decKey = cipher.decrypt(encKey)

# check free size in the rootfs
s = os.statvfs(INSTALL_PATH_BASE) if os.path.isdir(INSTALL_PATH_BASE) else os.statvfs(INSTALL_PATH_BASE.rsplit('/', 1)[0])
freeSpaceMB = s.f_bfree * s.f_frsize / (1024*1024) # in KB
availSpaceMB = s.f_bavail * s.f_frsize / (1024*1024) # in KB

requiredFreeSpaceMB = 12
printDBG("Free space %s MB in rootfs" % (availSpaceMB))
if availSpaceMB < requiredFreeSpaceMB:
    msg = "Not enough disk space for installing E2iPlayer!\nAt least %s MB is required.\nYou have %s MB free space in the rootfs.\nDo you want to continue anyway?" % (requiredFreeSpaceMB, availSpaceMB)
    answer = ''
    while answer not in ['Y', 'N']:
        answer = raw_input(MSG_FORMAT.format(msg) + "\nY/N: ").strip().upper()
        msg = ''

    if answer != 'Y':
        printERR("Not enough disk space for installing E2iPlayer!\nAt least %s MB is required." % requiredFreeSpaceMB)



ret = os.system("mkdir -p %s" % INSTALL_BASE)
if ret not in [None, 0]:
    raise Exception('Creating %s failed! Return code: %s' % (INSTALL_BASE, ret))

packageConfig = '%s_%s%s' % (e2iPlatform, installOld, installFPU)

# download e2iimporter
url = INSTALL_URL_BASE + 'keys_nt/%s_e2iimport_%s_%s.so' % (nameHash, packageConfig, pyVersion)
archFile = os.path.join(INSTALL_BASE, 'lib',  'e2iimport.so')
downloadWithWget(url, archFile)
os.chmod(archFile, 0755)
systemFilePath = sitePackagesPath + '/e2iimport.so'
if os.path.exists(systemFilePath):
    if not os.path.isfile(systemFilePath) or not os.path.islink(systemFilePath):
        printERR("%s exists - please remove it to be able to install!" % systemFilePath)
    os.unlink(systemFilePath)
os.symlink(archFile, systemFilePath)

# download e2ivalidator
url = INSTALL_URL_BASE + 'keys_nt/%s_e2ivalidator_%s.so' % (nameHash, packageConfig)
archFile = os.path.join(INSTALL_BASE, 'lib',  'e2ivalidator.so')
downloadWithWget(url, archFile)
os.chmod(archFile, 0755)


archFile = os.path.join(TMP_DIR, 'e2iplayer_last.tar.gz')
url = UPDATE_URL_BASE + 'latest.python%s.%s.tar.gz' % (sys.version_info[0], sys.version_info[1])

downloadWithWget(url, archFile)
decryptFile(archFile, decKey)
unpackDir = os.path.join(TMP_DIR, 'e2iplayer_last')
unpackArchive(archFile, unpackDir)
E2iPlayerDir = os.path.join(unpackDir, 'iptvplayer-for-e2.git', 'IPTVPlayer')

for archFile in ('graphics.tar.gz', 'icons%s.tar.gz' % ICONS_SIZE):
    url = UPDATE_URL_BASE + archFile
    archFile = os.path.join(TMP_DIR, archFile)
    downloadWithWget(url, archFile)
    unpackArchive(archFile, E2iPlayerDir, clearOutputBeforeUnpack=False)
    
try:
    E2iPlayerVer = getE2iPlayerVersion(E2iPlayerDir)
except Exception:
    printExc()
    printERR("Something goes wrong. Unable to get E2iPlayer version from prepared package!")

msg = 'E2iPlayer v%s ready to install into "%s".\nDo you want to proceed?' % (E2iPlayerVer, INSTALL_PATH_BASE)
answer = ''
while answer not in ['Y', 'N']:
    answer = raw_input(MSG_FORMAT.format(msg) + "\nY/N: ").strip().upper()
    msg = ''

if answer == 'Y':
    copyDirContent(E2iPlayerDir, INSTALL_PATH_BASE)
    with open(os.path.join(INSTALL_PATH_BASE, "update", ".settings.txt"), "w") as f:
        f.write("e2iplayer_login=%s\n" % LOGIN)
        f.write("e2iplayer_password=%s\n" % PASSWORD)
        f.write("preferredupdateserver=%s\n" % '3') # '3' - private server
        e2irootfs = ''
        for it in ('/hdd', '/media/hdd', '/media/usb', '/media/mmc'):
            if os.path.isdir(it + '/e2irootfs'):
                e2irootfs = it + '/e2irootfs'
                break

        if e2irootfs:
            os.mkdir(e2irootfs + '/e2i_config')
            os.mkdir(e2irootfs + '/e2i_recording')
            os.mkdir(e2irootfs + '/e2i_temp')
            os.mkdir(e2irootfs + '/e2i_cache')
            f.write("urllist_path=%s/e2i_config\n" % e2irootfs)
            f.write("buffering_path=%s/e2i_recording\n" % e2irootfs)
            f.write("recording_path=%s/e2i_recording\n" % e2irootfs)
            f.write("temp_path=%s/e2i_temp\n" % e2irootfs)
            f.write("cache_path=%s/e2i_cache\n" % e2irootfs)
        f.write("rambuffer_files=10\n")
        f.write("rambuffer_network=20\n")

    os.system("sync")
    printMSG('Done. E2iPlayer version "%s" installed correctly.\nPlease remember to restart your Enigma2.' % (E2iPlayerVer))
else:
    os.system('rm -rf "%s"' % E2iPlayerDir)
    printMSG('Installation skipped.')
