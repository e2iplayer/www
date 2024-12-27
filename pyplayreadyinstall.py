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

checkFreeSpace(5, 'pyplayready')

pyVersion = checkPyVersion()
installPackage = 'pyplayready_%s.tar.gz' % (pyVersion)

printDBG("Slected pyplayready package: %s" % installPackage)

url = "https://www.e2iplayer.gitlab.io/resources/packages/pyplayready/%s" % installPackage
out = '/tmp/' + installPackage

if not downloadUrl(url, out):
    printFatal('Download package %s failed!' % url)

msg = 'Package %s ready to install.\nDo you want to proceed?' % installPackage
if ask(msg):
    # remove old version
    removeCmdBase = 'rm -rf %s/usr/lib/%s/site-packages/' % (INSTALL_BASE, pyVersion)
    for item in ('pyplayready', 'construct', 'ecpy'):
        os.system(removeCmdBase + item)

    ret = os.system("mkdir -p %s && tar -xvf /tmp/%s -C %s " % (INSTALL_BASE, installPackage, INSTALL_BASE))
    if ret not in [None, 0]:
        printFatal('pyplayready unpack archive failed with return code: %s' % (ret))

    os.system('rm -f /tmp/%s' % installPackage)

    # check if pyplayready is working
    try:
        sys.path.insert(0, '%s/usr/lib/%s/site-packages' % (INSTALL_BASE, pyVersion))
        from pyplayready.cdm import Cdm
        from pyplayready.device import Device
        from pyplayready.pssh import PSSH

        import pyplayready
        path = os.path.abspath(pyplayready.__file__)
        if not path.startswith(INSTALL_BASE):
            printWRN('pyplayready works but not form installed location! -> %s' % path)
        else:
            printMSG('Done. pyplayready installed correctly.')
    except Exception as e:
        printFatal('Installed pyplayready is NOT working correctly! pyplayready can not be imported -> %r' % e)

