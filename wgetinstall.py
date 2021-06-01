#!/usr/bin/python
# -*- coding: utf-8 -*-

import os
import sys
import time

IS_PY3 = sys.version_info[0] == 3
a = lambda d: d.decode('utf-8') if IS_PY3 else str(d)
if IS_PY3: from urllib import request
else: import urllib2 as request

def getHTML(url):
    response = request.urlopen(url)
    html = a(response.read())
    response.close()
    return html

exec(getHTML('http://e2iplayer.github.io/www/utils.py?_=%s' % time.time()))

checkFreeSpace(2, 'wget')

platformInfo = GetPlatformInfo()
e2iOpenSSLVer = GetOpenSSLVer(platformInfo)
if not e2iOpenSSLVer:
    printFatal('Problem with OpenSSL version detection!')

if e2iOpenSSLVer in ('1.0.0', '1.0.2'):
    installSSLVer = '1.0.2'
elif e2iOpenSSLVer == '1.1':
    installSSLVer = '1.1.1'
else:
    installSSLVer = '0.9.8'

ret = os.system("mkdir -p %s" % INSTALL_BASE)
if ret not in [None, 0]:
    printFatal('Creating %s failed! Return code: %s' % (INSTALL_BASE, ret))

packageConfig = getPackageConfig(platformInfo)

config = '%s/wget_%s_openssl%s' % (tuple(packageConfig.split('_', 1) + [installSSLVer]))
url = 'http://e2iplayer.github.io/www/wget/' + config + '?_=' + str(time.time())
out = '/tmp/e2i_wget'
printDBG("Slected wget package: %s" % url)

if True or not downloadUrl(url, out):
    printWRN('Download package %s failed!' % url)
    response = request.urlopen(url)
    with open(out, "wb") as f:
        f.write(response.read())
    response.close()

installDir = INSTALL_BASE + 'usr/bin/'
cmds = ["chmod 0755 %s" % out]
cmds += ["%s --version" % out]
cmds += ["rm -rf %s" % (installDir + 'wget')]
cmds += ["mkdir -p %s" % installDir]
cmds += ["cp %s %s" % (out, installDir + 'wget')]
ret = os.system(' && '.join(cmds))
os.system('rm -rf %s' % out)

if ret == 0:
    os.system('sync')
    printMSG("wget: %s ready" % config)
else:
    printFatal("Something goes wrong!")
