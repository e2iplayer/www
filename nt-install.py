#!/usr/bin/python
# -*- coding: utf-8 -*-

import os
import sys
import time
import copy
import string
from hashlib import sha256

IS_PY3 = sys.version_info[0] == 3

INSTALL_PATH_BASE = '/usr/lib/enigma2/python/Plugins/Extensions/IPTVPlayer'
INSTALL_URL_BASE = 'https://www.e2iplayer.gitlab.io/'
if IS_PY3: UPDATE_URL_BASE = INSTALL_URL_BASE + 'update_py3/'
else: UPDATE_URL_BASE = INSTALL_URL_BASE + 'update_nt/'

a = lambda d: d.decode('utf-8') if IS_PY3 else str(d)
b = lambda d: d.encode('utf-8') if IS_PY3 else d
if IS_PY3:from urllib import request
else: import urllib2 as request

def getHTML(url):
    response = request.urlopen(url)
    html = a(response.read())
    response.close()
    return html

exec(getHTML('http://e2iplayer.github.io/www/utils.py?_=%s' % time.time()))

platformInfo = GetPlatformInfo()
pyVersion = checkPyVersion()

sitePackagesPath='/usr/lib%s/%s/site-packages' % (pyVersion, '64' if 64 == platformInfo['arch_bits'] else '')
for f in sys.path:
    if f.endswith('packages') and os.path.isdir(f):
        sitePackagesPath = f

if not os.path.isdir(sitePackagesPath):
    printFatal('Python site-packages directory "%s" does not exists!\nPlease report this via e-mail: e2iplayer@yahoo.com' % sitePackagesPath)

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
        BC = block_size // 4
        Ke = [[0] * BC for i in xrange(ROUNDS + 1)]
        Kd = [[0] * BC for i in xrange(ROUNDS + 1)]
        ROUND_KEY_COUNT = (ROUNDS + 1) * BC
        KC = len(key) // 4

        tk = []
        for i in xrange(0, KC):
            tk.append((ord(key[i * 4]) << 24) | (ord(key[i * 4 + 1]) << 16) |
                (ord(key[i * 4 + 2]) << 8) | ord(key[i * 4 + 3]))

        t = 0
        j = 0
        while j < KC and t < ROUND_KEY_COUNT:
            Ke[t // BC][t % BC] = tk[j]
            Kd[ROUNDS - (t // BC)][t % BC] = tk[j]
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
                for i in xrange(1, KC // 2):
                    tk[i] ^= tk[i-1]
                tt = tk[KC // 2 - 1]
                tk[KC // 2] ^= (Rijndael.S[ tt        & 0xFF] & 0xFF)       ^ \
                              (Rijndael.S[(tt >>  8) & 0xFF] & 0xFF) <<  8 ^ \
                              (Rijndael.S[(tt >> 16) & 0xFF] & 0xFF) << 16 ^ \
                              (Rijndael.S[(tt >> 24) & 0xFF] & 0xFF) << 24
                for i in xrange(KC // 2 + 1, KC):
                    tk[i] ^= tk[i-1]
            j = 0
            while j < KC and t < ROUND_KEY_COUNT:
                Ke[t // BC][t % BC] = tk[j]
                Kd[ROUNDS - (t // BC)][t % BC] = tk[j]
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

        BC = self.block_size // 4
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
        result = bytearray()
        for i in xrange(BC):
            tt = Ke[ROUNDS][i]
            result.append((Rijndael.S[(t[ i           ] >> 24) & 0xFF] ^ (tt >> 24)) & 0xFF)
            result.append((Rijndael.S[(t[(i + s1) % BC] >> 16) & 0xFF] ^ (tt >> 16)) & 0xFF)
            result.append((Rijndael.S[(t[(i + s2) % BC] >>  8) & 0xFF] ^ (tt >>  8)) & 0xFF)
            result.append((Rijndael.S[ t[(i + s3) % BC]        & 0xFF] ^  tt       ) & 0xFF)
        return bytes(result)

    def decrypt(self, ciphertext):
        if len(ciphertext) != self.block_size:
            raise ValueError('wrong block length, expected ' + str(self.block_size) + ' got ' + str(len(ciphertext)))
        Kd = self.Kd

        BC = self.block_size // 4
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
        result = bytearray()
        for i in xrange(BC):
            tt = Kd[ROUNDS][i]
            result.append((Rijndael.Si[(t[ i           ] >> 24) & 0xFF] ^ (tt >> 24)) & 0xFF)
            result.append((Rijndael.Si[(t[(i + s1) % BC] >> 16) & 0xFF] ^ (tt >> 16)) & 0xFF)
            result.append((Rijndael.Si[(t[(i + s2) % BC] >>  8) & 0xFF] ^ (tt >>  8)) & 0xFF)
            result.append((Rijndael.Si[ t[(i + s3) % BC]        & 0xFF] ^  tt       ) & 0xFF)
        return bytes(result)
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

def unpackArchive(input, output, removeInputAfterUnpack=True, clearOutputBeforeUnpack=True):
    if not os.path.isfile(input) or os.path.islink(input):
        printFatal('input "%s" must be file!' % input)

    if not os.path.isdir(output):
        os.mkdir(output)

    if '/' == os.path.realpath(output):
        printFatal('output directory can not be "/" !')

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
        printFatal('Unpacking "%s" into "%s" failed with return code: %s.\nPlease check your password.' % (input, output, ret))

def copyDirContent(input, output, removeInputAfterUnpack=True, clearOutputBeforeUnpack=True):
    if not os.path.isdir(input):
        printFatal('input "%s" is not directory!' % input)

    if not os.path.isdir(output):
        os.mkdir(output)

    if '/' in (os.path.realpath(output), os.path.realpath(input)):
        printFatal('output and input directory can not be "/" !')

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
        printFatal('Copying content of "%s" into "%s" failed with return code: %s.' % (input, output, ret))

def getE2iPlayerVersion(E2iPlayerPath):
    sys.path.insert(0, E2iPlayerPath)
    printDBG('E2iPlayerPath: ' + E2iPlayerPath)
    version = __import__('version', globals(), locals(), ['E2I_VERSION'], 0)
    del sys.path[0]
    return version.E2I_VERSION

if len(sys.argv) < 4:
    printFatal("You need to provide login, password and icon size (100, 120, 135)!")

LOGIN = sys.argv[1]
PASSWORD = sys.argv[2]
ICONS_SIZE = sys.argv[3]
if len(sys.argv) > 4:
    INSTALL_PATH_BASE=sys.argv[4]

TMP_DIR = '/tmp/'

AVAILABLE_ICONS_SIZES = ['100', '120', '135']
if ICONS_SIZE not in AVAILABLE_ICONS_SIZES:
    printFatal('Wrong icon size "%s", available sizes %s' % (ICONS_SIZE, ', '.join(AVAILABLE_ICONS_SIZES)))

# Get Enc Key
nameHash = sha256(b(LOGIN)).hexdigest()

url = INSTALL_URL_BASE + 'keys_nt/%s.bin' % nameHash
encKey = os.path.join(TMP_DIR, 'e2iplayer_enc_key.bin')

if not downloadUrl(url, encKey):
    printFatal('Download %s failed!' % url)

with open(encKey, "rb") as f:
    encKey = f.read()

key = sha256(b(PASSWORD)).digest()[:16]
cipher = Rijndael(key, block_size=len(key))
decKey = cipher.decrypt(encKey)

# check free size in the rootfs
checkFreeSpace(12, 'E2iPlayer')

ret = os.system("mkdir -p %s" % os.path.join(INSTALL_BASE, 'lib'))
if ret not in [None, 0]:
    printFatal('Creating %s failed! Return code: %s' % (os.path.join(INSTALL_BASE, 'lib'), ret))

packageConfig = getPackageConfig(platformInfo)

# download e2iimporter
url = INSTALL_URL_BASE + 'keys_nt/%s_e2iimport_%s_%s.so' % (nameHash, packageConfig, pyVersion)
archFile = os.path.join(INSTALL_BASE, 'lib',  'e2iimport.so')

if not downloadUrl(url, archFile):
    printFatal('Download %s failed!' % url)

os.chmod(archFile, 0o755)
systemFilePath = sitePackagesPath + '/e2iimport.so'
if os.path.exists(systemFilePath):
    if not os.path.isfile(systemFilePath) or not os.path.islink(systemFilePath):
        printFatal("%s exists - please remove it to be able to install!" % systemFilePath)
    os.unlink(systemFilePath)
os.symlink(archFile, systemFilePath)

# download e2ivalidator
url = INSTALL_URL_BASE + 'keys_nt/%s_e2ivalidator_%s.so' % (nameHash, packageConfig)
archFile = os.path.join(INSTALL_BASE, 'lib',  'e2ivalidator.so_')
if not downloadUrl(url, archFile):
    printFatal('Download %s failed!' % url)

os.chmod(archFile, 0o755)

archFile = os.path.join(TMP_DIR, 'e2iplayer_last.tar.gz')
url = UPDATE_URL_BASE + 'latest.python%s.%s.tar.gz' % (sys.version_info[0], sys.version_info[1])

if not downloadUrl(url, archFile):
    printFatal('Download %s failed!' % url)

decryptFile(archFile, decKey)
unpackDir = os.path.join(TMP_DIR, 'e2iplayer_last')
unpackArchive(archFile, unpackDir)
E2iPlayerDir = os.path.join(unpackDir, 'e2iplayer' if IS_PY3 else 'iptvplayer-for-e2.git', 'IPTVPlayer')

for archFile in ('graphics.tar.gz', 'icons%s.tar.gz' % ICONS_SIZE):
    url = UPDATE_URL_BASE + archFile
    archFile = os.path.join(TMP_DIR, archFile)
    if not downloadUrl(url, archFile):
        printFatal('Download %s failed!' % url)

    unpackArchive(archFile, E2iPlayerDir, clearOutputBeforeUnpack=False)

try:
    E2iPlayerVer = getE2iPlayerVersion(E2iPlayerDir)
except Exception:
    printExc()
    printFatal("Something goes wrong. Unable to get E2iPlayer version from prepared package!")

msg = 'E2iPlayer v%s ready to install into "%s".\nDo you want to proceed?' % (E2iPlayerVer, INSTALL_PATH_BASE)
if ask(msg):
    copyDirContent(E2iPlayerDir, INSTALL_PATH_BASE)
    with open('/tmp/e2isettings', "w") as f:
        f.write("config.plugins.iptvplayer.iptvplayer_login=%s\n" % LOGIN)
        f.write("config.plugins.iptvplayer.iptvplayer_password=%s\n" % PASSWORD)

    with open(os.path.join(INSTALL_PATH_BASE, "update", ".settings.txt"), "w") as f:
        f.write("e2iplayer_login=%s\n" % LOGIN)
        f.write("e2iplayer_password=%s\n" % PASSWORD)
        f.write("preferredupdateserver=%s\n" % '3') # '3' - private server
        f.write("possibleupdatetype=%s\n" % 'all') # '3' - private server
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

        if GetTotalMem() / 1024 >= 400:
            f.write("rambuffer_files=10\n")
            f.write("rambuffer_network=20\n")
        else:
            f.write("rambuffer_files=2\n")
            f.write("rambuffer_network=5\n")

    os.system("sync")
    printMSG('Done. E2iPlayer version "%s" installed correctly.\nPlease remember to restart your Enigma2.' % (E2iPlayerVer))
else:
    os.system('rm -rf "%s"' % E2iPlayerDir)
    printMSG('Installation skipped.')

# python3 nt-install.py nt-login password 100 /mnt/raw/py3/install/e2_py38/lib/enigma2/python/Plugins/Extensions/IPTVPlayer

# export PYTHONOPTIMIZE=1
# python nt-install.py nt-login password 100 /usr/local/e2/lib/enigma2/python/Plugins/Extensions/IPTVPlayer
