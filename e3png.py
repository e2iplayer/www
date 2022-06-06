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

checkFreeSpace(2, 'extEplayer3 PNG plugin')

platformInfo = GetPlatformInfo()

ret = os.system("mkdir -p %s" % INSTALL_BASE)
if ret not in [None, 0]:
    printFatal('Creating %s failed! Return code: %s' % (INSTALL_BASE, ret))

packageConfig = getPackageConfig(platformInfo)
installFile = "exteplayer3png.so"

installPackage = '%s/exteplayer3png_%s.so' % (tuple(packageConfig.split('_', 1)))
url = 'https://www.e2iplayer.gitlab.io/resources/packages/bin/' + installPackage
out = os.path.join('/tmp', installFile)

printDBG("Slected exteplayer3png package: %s" % url)

if not downloadUrl(url, out):
    printFatal('Download package %s failed!' % url)

msg = 'Package %s ready to install.\nDo you want to proceed?' % installFile
answer = ask(msg)
if answer:
    installPath = '%s/lib/%s' % (INSTALL_BASE, installFile)
    ret = os.system("mkdir -p %s/lib && cp %s %s && chmod 777 %s " % (INSTALL_BASE, out, installPath, installPath))

os.system('rm -f /%s' % out)

if not answer:
    printMSG('Installation cancelled.')
    sys.exit(1)

if ret not in [None, 0]:
    raise Exception('%s installation failed with return code: %s' % (installFile, ret))

def HasPackage():
    hasPackage = False
    try:
        file = os.popen(installPath)
        data = file.read()
        printDBG('Test output data: %s' % data)
        ret = file.close()
        printDBG('Test return code: %d' % ret)
        if ret in [0, None]:
            hasPackage = True
    except Exception as e:
        printDBG(e)
    return hasPackage

if HasPackage():
    os.system('sync')
    printMSG("Done.\nReady to use.")
else:
    printFatal('Installed %s is NOT working correctly!' % installPackage)


