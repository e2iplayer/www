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

checkFreeSpace(12, 'ffmpeg')

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
    printFatal('OpenSSL is to old, please install new one!')

packageConfig = '%s_openssl%s' % (getPackageConfig(platformInfo), installSSLVer)
installPackage = 'ffmpeg_%s.tar.gz' % (packageConfig)

printDBG("Slected ffmpeg package: %s" % installPackage)

def HasFFmpeg():
    hasFFmpeg = False
    try:
        file = os.popen(INSTALL_BASE + 'usr/bin/ffmpeg -version')
        data = file.read()
        ret = file.close()
        if ret in [0, None]:
            hasFFmpeg = True
    except Exception as e:
        printDBG(e)
    return hasFFmpeg

if HasFFmpeg():
    msg = 'Old ffmpeg installation has been detected in "%s"\nDo you want to remove it?' % INSTALL_BASE
    if ask(msg):
        ret = os.system("rm -f %s/usr/bin/ffmpeg && cd %s/usr/lib/ && rm -f libavcodec.so* libavdevice.so* libavfilter.so* libavformat.so* libavutil.so* libswresample.so* libswscale.so*" % (INSTALL_BASE, INSTALL_BASE))
        if ret not in [None, 0]:
            printWRN("Cleanup of the old ffmpeg installation failed! Return code: %s" % ret)

ret = os.system("mkdir -p %s" % INSTALL_BASE)
if ret not in [None, 0]:
    printFatal('Creating %s failed! Return code: %s' % (INSTALL_BASE, ret))

url = "https://www.e2iplayer.gitlab.io/resources/packages/ffmpeg/" + installPackage
out = '/tmp/' + installPackage

if not downloadUrl(url, out):
    printFatal('Download package %s failed!' % url)

msg = 'Package %s ready to install.\nDo you want to proceed?' % installPackage
answer = ask(msg)

if answer:
    ret = os.system("mkdir -p %s && tar -xvf /tmp/%s -C %s " % (INSTALL_BASE, installPackage, INSTALL_BASE))

os.system('rm -f /tmp/%s' % installPackage)

if not answer:
    printMSG('Installation cancelled.')
    sys.exit(1)

if ret not in [None, 0]:
    printFatal('FFmpeg installation failed with return code: %s' % (ret))

if answer:
    if HasFFmpeg():
        exteplayer3Paths = [INSTALL_BASE + 'usr/bin/exteplayer3']
        exteplayer3Detected = False
        if os.path.isfile(exteplayer3Paths[0]):
            msg = 'Old exteplayer3 binary detected. You should remove it. After restart E2iPlayer will install new one.\nDo you want to proceed?'
            if ask(msg):
                os.system('rm -f %s' % exteplayer3Paths[0])
        os.system('sync')
        printMSG("Done.\nPlease remember to restart your Enigma2.")
    else:
        printFatal('Installed ffmpeg is NOT working correctly!')
