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

checkFreeSpace(5, 'PyCurl')

pyVersion = checkPyVersion()
platformInfo = GetPlatformInfo()

packageConfig = '%s_%s' % (pyVersion, getPackageConfig(platformInfo))
installPackage = 'e2ipycurl_%s.tar.gz' % (packageConfig)

printDBG("Slected pycurl package: %s" % installPackage)

sitePackagesPath='/usr/lib%s/%s/site-packages' % (pyVersion, '64' if 64 == platformInfo['arch_bits'] else '')
for f in sys.path:
    if f.endswith('packages') and os.path.isdir(f):
        sitePackagesPath = f

if not os.path.isdir(sitePackagesPath):
    raise Exception('Python site-packages directory "%s" does not exists!\nPlease report this via e-mail: e2iplayer@yahoo.com' % sitePackagesPath)

printDBG("sitePackagesPath %s" % sitePackagesPath)
expectedPyCurlVersion = 20250116
acctionNeededBeforeInstall = 'NONE'
localE2iPyCurlPath = os.path.join(INSTALL_BASE, 'usr/lib/%s/site-packages/e2ipycurl.so' % (pyVersion))
systemE2iPyCurlPath = ''
try:
    fd = os.popen(pyInterpreter + ' -c "import os; import e2ipycurl; print(os.path.abspath(e2ipycurl.__file__))"')
    systemE2iPyCurlPath = fd.read().strip()
    fd.close()
except Exception as e:
    printExc(str(e))

if not systemE2iPyCurlPath.startswith('/'):
    systemE2iPyCurlPath = sitePackagesPath + '/e2ipycurl.so'

# real PyCurl
systemPyCurlPath = ''
try:
    fd = os.popen(pyInterpreter + ' -c "import os; import pycurl; print(os.path.abspath(pycurl.__file__))"')
    systemPyCurlPath = fd.read().strip()
    fd.close()
except Exception as e:
    printExc(str(e))

if os.path.isfile(systemE2iPyCurlPath) and not os.path.islink(systemE2iPyCurlPath):
    ret = os.system(pyInterpreter + ' -c "import sys; import e2ipycurl; test=e2ipycurl.E2IPLAYER_VERSION_NUM == ' + str(expectedPyCurlVersion) + '; sys.exit(0 if test else -1);"')
    if ret == 0:
        # same version but by copy
        acctionNeededBeforeInstall = "REMOVE_FILE"
    else:
        acctionNeededBeforeInstall = "BACKUP_FILE"
elif os.path.islink(systemE2iPyCurlPath):
    # systemE2iPyCurlPath is symbolic link
    linkTarget = os.path.realpath(systemE2iPyCurlPath)
    if linkTarget != os.path.realpath(localE2iPyCurlPath):
        printFatal('Error!!! Your %s is symbolc link to %s!\nThis can not be handled by this installer.\nYou can remove it by hand and try again.\n' % (systemE2iPyCurlPath, linkTarget))
    else:
        acctionNeededBeforeInstall = "REMOVE_SYMBOLIC_LINK"

printDBG("Action needed before install %s" % acctionNeededBeforeInstall)
ret = os.system("mkdir -p %s" % INSTALL_BASE)
if ret not in [None, 0]:
    printFatal('Creating %s failed! Return code: %s' % (INSTALL_BASE, ret))

ret = os.system('rm -f /tmp/%s' % installPackage)
if ret not in [None, 0]:
    printFatal('Removing old downloaded package /tmp/%s failed! Return code: %s' % (installPackage, ret))

url = "https://www.e2iplayer.gitlab.io/resources/packages/e2ipycurl/%s" % installPackage
out = '/tmp/' + installPackage

if not downloadUrl(url, out):
    printFatal('Download package %s failed!' % url)

msg = 'Package %s ready to install.\nDo you want to proceed?' % installPackage
if ask(msg):
    # restore previously moved pycurl
    if os.path.islink(systemPyCurlPath):
        linkTarget = os.path.realpath(systemPyCurlPath)
        localPyCurlPath = localE2iPyCurlPath.replace('/e2ipycurl.so', '/pycurl.so')
        if linkTarget == os.path.realpath(localPyCurlPath):
            maxTimestamp = 0
            toRestore = ''
            directory = os.path.dirname(systemPyCurlPath)
            for f in os.listdir(directory):
                if 'pycurl.so_backup_' not in f: continue
                timestamp = float(f.split('_', 1)[-1])
                if timestamp > maxTimestamp:
                    maxTimestamp = timestamp
                    toRestore = os.path.join(directory, f)

            os.unlink(systemPyCurlPath)
            if toRestore:
                os.rename(toRestore, systemPyCurlPath)

    # remove old version
    os.system('(cd %s/lib/; rm -f libcurl.so* libwolfssl.so* libnghttp2.so* libbrotlidec.so* libbrotlicommon.so*)' % INSTALL_BASE)

    ret = os.system("mkdir -p %s && tar -xvf /tmp/%s -C %s " % (INSTALL_BASE, installPackage, INSTALL_BASE))
    if ret not in [None, 0]:
        printFatal('PyCurl unpack archive failed with return code: %s' % (ret))

    os.system('rm -f /tmp/%s' % installPackage)

    if acctionNeededBeforeInstall in ['REMOVE_FILE', 'REMOVE_SYMBOLIC_LINK']:
        os.unlink(systemE2iPyCurlPath)
    elif acctionNeededBeforeInstall == 'BACKUP_FILE':
        backup = '%s_backup_%s' % (systemE2iPyCurlPath, str(time.time()))
        os.rename(systemE2iPyCurlPath, backup)

    # create symlink
    os.symlink(localE2iPyCurlPath, systemE2iPyCurlPath)

    # check if pycurl is working
    import e2ipycurl
    if e2ipycurl.E2IPLAYER_VERSION_NUM >= expectedPyCurlVersion:
        printMSG('Done. PyCurl version "%s" installed correctly.\nPlease remember to restart your Enigma2.' % (e2ipycurl.E2IPLAYER_VERSION_NUM))
    else:
        printFatal('Installed PyCurl is NOT working correctly! It report diffrent version "%s" then expected "%s"' % (e2ipycurl.E2IPLAYER_VERSION_NUM, expectedPyCurlVersion))

