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

checkFreeSpace(12, 'OpenSSL')

installPackage = 'openssl_%s.tar.gz' % getPackageConfig()
printDBG("Slected package: %s" % installPackage)

def HasOpenssl():
    hasOpenssl = False
    try:
        file = os.popen(INSTALL_BASE + 'usr/bin/openssl version')
        data = file.read()
        ret = file.close()
        if ret in [0, None]:
            hasOpenssl = True
    except Exception as e:
        printDBG(e)
    return hasOpenssl

if HasOpenssl():
    msg = 'Old openssl installation has been detected in "%s"\nDo you want to remove it?' % INSTALL_BASE
    if ask(msg):
        ret = os.system("cd %s/usr/bin && rm -f openssl c_rehash && cd %s/usr/lib/ && rm -f libssl* libcrypto*" % (INSTALL_BASE, INSTALL_BASE))
        if ret not in [None, 0]:
            printWRN("Cleanup of the old openssl installation failed! Return code: %s" % ret)

ret = os.system("mkdir -p %s" % INSTALL_BASE)
if ret not in [None, 0]:
    printFatal('Creating %s failed! Return code: %s' % (INSTALL_BASE, ret))

url = "http://e2iplayer.github.io/www/openssl/" + installPackage
out = "/tmp/" + installPackage
if not downloadUrl(url, out):
    printFatal('Download package %s failed!' % url)

msg = 'Package %s ready to install.\nDo you want to proceed?' % installPackage 
answer = ask(msg)
if answer:
    ret = os.system("mkdir -p %s && tar -xvf /tmp/%s -C %s " % (INSTALL_BASE, installPackage , INSTALL_BASE))

os.system('rm -f /tmp/%s' % installPackage )

if not answer:
    printMSG('Installation cancelled.')
    sys.exit(1)

if ret not in [None, 0]:
    printFatal('Openssl installation failed with return code: %s' % (ret))

if answer:
    if HasOpenssl():
        wgetPaths = [INSTALL_BASE + 'usr/bin/wget']
        if os.path.isfile(wgetPaths[0]):
            msg = 'Old wget binary detected. You should remove it. After restart E2iPlayer will install new one.\nDo you want to proceed?'
            if ask(msg):
                os.system('rm -f %s' % wgetPaths[0])
        os.system('sync')
        printMSG("Done.\nPlease remember to restart your Enigma2.")
    else:
        printFatal('Installed openssl is NOT working correctly!')
