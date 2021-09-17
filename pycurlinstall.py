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

checkFreeSpace(5, 'PyCurl')

pyVersion = checkPyVersion()
platformInfo = GetPlatformInfo()

packageConfig = '%s_%s' % (pyVersion, getPackageConfig(platformInfo))
installPackage = 'pycurl_%s.tar.gz' % (packageConfig)

printDBG("Slected pycurl package: %s" % installPackage)

sitePackagesPath='/usr/lib%s/%s/site-packages' % (pyVersion, '64' if 64 == platformInfo['arch_bits'] else '')
for f in sys.path:
    if f.endswith('packages') and os.path.isdir(f):
        sitePackagesPath = f

if not os.path.isdir(sitePackagesPath):
    raise Exception('Python site-packages directory "%s" does not exists!\nPlease report this via e-mail: e2iplayer@yahoo.com' % sitePackagesPath)

printDBG("sitePackagesPath %s" % sitePackagesPath)
expectedPyCurlVersion = 20200930 #20210823
acctionNeededBeforeInstall = 'NONE'
systemPyCurlPath = sitePackagesPath + '/pycurl.so'
localPyCurlPath = os.path.join(INSTALL_BASE, 'usr/lib/%s/site-packages/pycurl.so' % (pyVersion))

if os.path.isfile(systemPyCurlPath) and not os.path.islink(systemPyCurlPath):
    ret = os.system('python -c "import sys; import pycurl; test=pycurl.E2IPLAYER_VERSION_NUM == ' + str(expectedPyCurlVersion) + '; sys.exit(0 if test else -1);"')
    if ret == 0:
        # same version but by copy
        acctionNeededBeforeInstall = "REMOVE_FILE"
    else:
        acctionNeededBeforeInstall = "BACKUP_FILE"
elif os.path.islink(systemPyCurlPath):
    # systemPyCurlPath is symbolic link
    linkTarget = os.path.realpath(systemPyCurlPath)
    if linkTarget != os.path.realpath(localPyCurlPath):
        printFatal('Error!!! Your %s is symbolc link to %s!\nThis can not be handled by this installer.\nYou can remove it by hand and try again.\n' % (systemPyCurlPath, linkTarget))
    else:
        acctionNeededBeforeInstall = "REMOVE_SYMBOLIC_LINK"

printDBG("Action needed before install %s" % acctionNeededBeforeInstall)
ret = os.system("mkdir -p %s" % INSTALL_BASE)
if ret not in [None, 0]:
    printFatal('Creating %s failed! Return code: %s' % (INSTALL_BASE, ret))

ret = os.system('rm -f /tmp/%s' % installPackage)
if ret not in [None, 0]:
    printFatal('Removing old downloaded package /tmp/%s failed! Return code: %s' % (installPackage, ret))

url = "https://www.e2iplayer.gitlab.io/resources/packages/pycurl/%s" % installPackage
out = '/tmp/' + installPackage

if not downloadUrl(url, out):
    printFatal('Download package %s failed!' % url)

msg = 'Package %s ready to install.\nDo you want to proceed?' % installPackage
if ask(msg):
    # remove old version
    os.system('rm -rf %s/lib/libcurl.so*' % INSTALL_BASE)
    os.system('rm -rf %s/lib/libwolfssl.so*' % INSTALL_BASE)

    ret = os.system("mkdir -p %s && tar -xvf /tmp/%s -C %s " % (INSTALL_BASE, installPackage, INSTALL_BASE))
    if ret not in [None, 0]:
        printFatal('PyCurl unpack archive failed with return code: %s' % (ret))

    os.system('rm -f /tmp/%s' % installPackage)

    if acctionNeededBeforeInstall in ['REMOVE_FILE', 'REMOVE_SYMBOLIC_LINK']:
        os.unlink(systemPyCurlPath)
    elif acctionNeededBeforeInstall == 'BACKUP_FILE':
        backup = '%s_backup_%s' % (systemPyCurlPath, str(time.time()))
        os.rename(systemPyCurlPath, backup)

    # create symlink
    os.symlink(localPyCurlPath, systemPyCurlPath)

    # check if pycurl is working
    import pycurl
    if pycurl.E2IPLAYER_VERSION_NUM >= expectedPyCurlVersion:
        printMSG('Done. PyCurl version "%s" installed correctly.\nPlease remember to restart your Enigma2.' % (pycurl.E2IPLAYER_VERSION_NUM))
    else:
        printFatal('Installed PyCurl is NOT working correctly! It report diffrent version "%s" then expected "%s"' % (pycurl.E2IPLAYER_VERSION_NUM, expectedPyCurlVersion))

