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

installFile = "device"
url = 'https://edrm2.gitlab.io/download/' + installFile
out = '/tmp/' + installFile

if not downloadUrl(url, out):
    printFatal('Download package %s failed!' % url)

finallLocation = INSTALL_BASE + '/' + installFile
if os.path.isfile(sitePackagesPath):
    msg = "DRM certificate: '%s' already exists. Do you want to overwrite it?" % finallLocation
    if not ask(msg):
        printMSG('Installation cancelled.')

msg = 'DRM certificate ready to install.\nDo you want to proceed?'
if ask(msg):
    # remove old version
    try:
        with open(out, 'wb') as i:
            with open(finallLocation, 'wb') as o:
                o.write(i.read())
        printMSG('Done. pywidevine installed correctly.')
    except Exception as e:
        printFatal('Instalation failed!\n%r' % e)
    finally:
        os.remove(out)


