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

checkFreeSpace(5, 'pywidevine')

pyVersion = checkPyVersion()
installPackage = 'pywidevine_%s.tar.gz' % (pyVersion)

printDBG("Slected pywidevine package: %s" % installPackage)

url = "https://www.e2iplayer.gitlab.io/resources/packages/pywidevine/%s" % installPackage
out = '/tmp/' + installPackage

if not downloadUrl(url, out):
    printFatal('Download package %s failed!' % url)

msg = 'Package %s ready to install.\nDo you want to proceed?' % installPackage
if ask(msg):
    # remove old version
    removeCmdBase = 'rm -rf %s/usr/lib/%s/site-packages/' % (INSTALL_BASE, pyVersion)
    for item in ('pywidevine', 'google'):
        os.system(removeCmdBase + item)

    os.system('rm -rf %s/pywidevine/' % INSTALL_BASE)

    ret = os.system("mkdir -p %s && tar -xvf /tmp/%s -C %s " % (INSTALL_BASE, installPackage, INSTALL_BASE))
    if ret not in [None, 0]:
        printFatal('pywidevine unpack archive failed with return code: %s' % (ret))

    os.system('rm -f /tmp/%s' % installPackage)

    # check if pywidevine is working
    try:
        sys.path.insert(0, '%s/usr/lib/%s/site-packages' % (INSTALL_BASE, pyVersion))
        from pywidevine.cdm import Cdm
        from pywidevine.device import Device
        from pywidevine.pssh import PSSH

        import pywidevine
        path = os.path.abspath(pywidevine.__file__)
        if not path.startswith(INSTALL_BASE):
            printWRN('pywidevine works but not form installed location! -> %s' % path)
        else:
            printMSG('Done. pywidevine installed correctly.')
    except Exception as e:
        printFatal('Installed pywidevine is NOT working correctly! pywidevine can not be imported -> %r' % e)

