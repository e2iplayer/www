#!/usr/bin/python
# -*- coding: utf-8 -*-

import os
import sys
import time

IS_PY3 = sys.version_info[0] == 3
a = lambda d: d.decode('utf-8') if IS_PY3 else str(d)
if IS_PY3:
    from urllib import request
else:
    import urllib2 as request

def getHTML(url):
    response = request.urlopen(url)
    html = a(response.read())
    response.close()
    return html

exec(getHTML('http://e2iplayer.github.io/www/utils.py?_=%s' % time.time()))

checkFreeSpace(5, 'PyCurl')

pyVersion = checkPyVersion()
platformInfo = GetPlatformInfo()

packageConfig = getPackageConfig(platformInfo)

url = 'https://www.e2iplayer.gitlab.io/resources/packages/bin/%s/e2isec_%s' % (tuple(packageConfig.split('_', 1)))
printDBG("Slected pycurl package: %s" % url)
e2isec = os.path.join('/tmp', 'e2i')
downloadUrl(url, e2isec)
os.chmod(e2isec, 0o755)

os.system(e2isec + ' getmac > ' + e2isec + '.out')
with open(e2isec + '.out') as f:
    macAddr = f.read().strip()

os.system(e2isec + ' getmac2 > ' + e2isec + '.out')
with open(e2isec + '.out') as f:
    macAddr2 = f.read().strip()

os.system(e2isec + ' uname > ' + e2isec + '.out')
with open(e2isec + '.out') as f:
    uname = f.read().strip()

os.system(e2isec + ' getserial > ' + e2isec + '.out')
with open(e2isec + '.out') as f:
    serial = f.read().strip()

printMSG('\n'.join(["PLATFORM INFO", 'uname:\t' + uname, 'config:\t' + packageConfig, 'python:\t' + pyVersion, 'serial:\t' + serial, 'mac:\t' + macAddr, 'mac2:\t' + macAddr2]))

