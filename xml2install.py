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

checkFreeSpace(2, 'libxml2')

LOCAL_PACKAGE_CONFIG = getPackageConfig()

installPackage = 'libxml2_%s.tar.gz' % LOCAL_PACKAGE_CONFIG
printDBG("Slected package: %s" % installPackage)

# check if installation is needed
LOCAL_EXISTS = False
for loc in ('/usr/lib/', '/lib/'):
    loc += 'libxml2.so.2'
    if not os.path.exists(loc):
        continue
    if 'libxml2.so.2.' in os.readlink(loc):
        LOCAL_EXISTS = True
        break

if LOCAL_EXISTS:
    printMSG("Done.\nlibxml2.so.2 already exists")
    if os.environ.get("FORCE_LIBXML2_INSTALL") != '1':
        sys.exit(0)

if '_aarch64_' in LOCAL_PACKAGE_CONFIG:
    LOCAL_LIB_NAME = 'libxml2.so.2.9.4'
else:
    LOCAL_LIB_NAME = 'libxml2.so.2.9.0'

def HasPackage():
    return os.path.isfile(INSTALL_BASE + 'usr/lib/' + LOCAL_LIB_NAME)

if HasPackage():
    msg = 'Old libxml installation has been detected in "%s"\nDo you want to remove it?' % INSTALL_BASE
    if ask(msg):
        ret = os.system("cd %s/usr/lib/ && rm -f libxml*" % (INSTALL_BASE,))
        if ret not in [None, 0]:
            printWRN("Cleanup of the old libxml installation failed! Return code: %s" % ret)

ret = os.system("mkdir -p %s" % INSTALL_BASE)
if ret not in [None, 0]:
    printFatal('Creating %s failed! Return code: %s' % (INSTALL_BASE, ret))

url = "https://www.e2iplayer.gitlab.io/resources/packages/libxml2/" + installPackage
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
    printFatal('libxml installation failed with return code: %s' % (ret))

if answer:
    if HasPackage():
        os.system('sync')
        printMSG("Done.")
    else:
        printFatal('libxml2 not found!')
