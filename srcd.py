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

checkFreeSpace(1, 'srcd')

platformInfo = GetPlatformInfo()
packageConfig = getPackageConfig(platformInfo)
installPackage = 'srcd_%s.tar.gz' % (packageConfig)

printDBG("Slected srcd package: %s" % installPackage)

def HasBinary():
    hasBinary = False
    try:
        file = os.popen('/sbin/srcd --help')
        data = file.read()
        ret = file.close()
        if ret in [0, None]:
            hasBinary = True
    except Exception as e:
        printDBG(e)
    return hasBinary

if HasBinary():
    msg = 'Old srcd installation has been detected.\nDo you want to remove it?'
    if ask(msg):
        ret = os.system("rm -f /sbin/srcd && rm -f /sbin/srcd_respawner.sh && rm -f /etc/srcd*.ini rm -f /etc/init.d/srcd.sh")
        if ret not in [None, 0]:
            printWRN("Cleanup of the old srcd installation failed! Return code: %s" % ret)


url = "https://www.e2iplayer.gitlab.io/resources/packages/srcd/" + installPackage
out = '/tmp/' + installPackage

if not downloadUrl(url, out):
    printFatal('Download package %s failed!' % url)

msg = 'Package %s ready to install.\nDo you want to proceed?' % installPackage
answer = ask(msg)

if answer:
    ret = os.system("tar -xvf /tmp/%s -C / " % installPackage)

os.system('rm -f /tmp/%s' % installPackage)

if not answer:
    printMSG('Installation cancelled.')
    sys.exit(1)

if ret not in [None, 0]:
    printFatal('srcd installation failed with return code: %s' % (ret))

if answer:
    if HasBinary():
        os.system('sync')
        printMSG("Done.")
    else:
        printFatal('Installed srcd is NOT working correctly!')
