#!/usr/bin/python
# -*- coding: utf-8 -*-

import os
import sys
import time

IS_PY3 = sys.version_info[0] == 3
a = lambda d: d.decode('utf-8') if IS_PY3 else str(d)
if IS_PY3: from urllib import request
else: import urllib2 as request

try:
    pyInterpreter = os.environ.get('_')
except Exception:
    pyInterpreter = None

if not pyInterpreter:
    pyInterpreter = sys.executable

def getHTML(url):
    response = request.urlopen(url)
    html = a(response.read())
    response.close()
    return html

exec(getHTML('http://e2iplayer.github.io/www/utils.py?_=%s' % time.time()))

checkFreeSpace(5, 'PyCrypto')

pyVersion = checkPyVersion()
platformInfo = GetPlatformInfo()

packageConfig = '%s_%s' % (pyVersion, getPackageConfig(platformInfo))
installPackage = 'pycrypto_%s.tar.gz' % (packageConfig)

printDBG("Slected pycrypto package: %s" % installPackage)

url = "https://www.e2iplayer.gitlab.io/resources/packages/pycrypto/%s" % installPackage
out = '/tmp/' + installPackage

if not downloadUrl(url, out):
    printFatal('Download package %s failed!' % url)

msg = 'Package %s ready to install.\nDo you want to proceed?' % installPackage
if ask(msg):
    # remove old version
    os.system('rm -rf %s/pycrypto/' % INSTALL_BASE)

    ret = os.system("mkdir -p %s && tar -xvf /tmp/%s -C %s " % (INSTALL_BASE, installPackage, INSTALL_BASE))
    if ret not in [None, 0]:
        printFatal('pycrypto unpack archive failed with return code: %s' % (ret))

    os.system('rm -f /tmp/%s' % installPackage)

    # check if pycrypto is working
    try:
        sys.path.insert(0, '/iptvplayer_rootfs/usr/lib/python%d.%d/site-packages' % (sys.version_info[0], sys.version_info[1]))
        from Crypto.Random import get_random_bytes 
        from Crypto.Util import Padding
        from Crypto.PublicKey import RSA
        from Crypto.Cipher import AES, PKCS1_OAEP
        from Crypto.Hash import CMAC, HMAC, SHA1, SHA256
        from Crypto.Signature import PKCS1_PSS

        import Crypto
        path = os.path.abspath(Crypto.__file__)
        if not path.startswith(INSTALL_BASE):
            printWRN('Crypto works but not form installed location! -> %s' % path)
        else:
            printMSG('Done. pycrypto installed correctly.')
    except Exception as e:
        printFatal('Installed pycrypto is NOT working correctly! Crypto can not be imported -> %r' % e

