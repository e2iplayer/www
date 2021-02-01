#!/usr/bin/python
# -*- coding: utf-8 -*-

import os
import re
import sys
import time
import traceback
import re

MSG_FORMAT = "\n\n=====================================================\n{0}\n=====================================================\n"

try:
    sys.stdin = open('/dev/tty')
except Exception:
    pass

def ERROR(txt):
    print(MSG_FORMAT.format('FATAL ERROR:\n\t' + txt))
    sys.exit(-1)

def printWRN(txt):
    print(MSG_FORMAT.format(txt))
    
def printMSG(txt):
    print(MSG_FORMAT.format(txt))

def printDBG(txt):
    print(str(txt))

def printExc(msg=''):
    print("===============================================")
    print("                   EXCEPTION                   ")
    print("===============================================")
    msg = msg + ': \n%s' % traceback.format_exc()
    print(msg)
    print("===============================================")

def fillMountPaths(partitions):
    # get mount paths
    with open("/proc/mounts") as f:
        lines = re.compile('\s*?([^\s]+?)\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)[^\n]*?\n').findall(f.read())
        for line in lines:
            for part in partitions:
                if line[0].endswith('/' + part['name']):
                    part['mounts'].append(line[1])
                    part['fs'] = line[2]

E2IPLAYER_EXTENSION_PATH = '/usr/lib/enigma2/python/Plugins/Extensions/IPTVPlayer'
E2IPLAYER_ROOTFS = '/iptvplayer_rootfs'
if True:
    if os.path.islink(E2IPLAYER_EXTENSION_PATH):
        ERROR('{} is already symbolic link! Not supported!'.format(E2IPLAYER_EXTENSION_PATH))

    if os.path.islink(E2IPLAYER_ROOTFS):
        ERROR('{} is already symbolic link! Not supported!'.format(E2IPLAYER_ROOTFS))

    if not os.path.islink('/hdd'):
        ERROR('/hdd is not symbolic link! Not supported!')

    mountPath = os.readlink('/hdd')
    if mountPath != '/media/hdd' and mountPath != 'media/hdd':
        ERROR('/hdd is symbolic link to "{}". Not supported!'.format(mountPath))
    mountPath = '/media/hdd'
else:
    mountPath = '/media/hdd'

storageDevices = []
with open("/proc/partitions") as f:
    lines = re.compile('\s*?(\d+)\s+?(\d+)\s+?(\d+?)\s+?(sd[a-z]\d*?)\n').findall(f.read())
    for line in lines:
        printDBG(line)
        storageDevices.append({'major':int(line[0]),  'minor':int(line[1]), '#blocks':int(line[2]), 'name':line[3], 'mounts':[], 'fs':None})

# allow to use whole device only when it does not have partitions
partitions = []
for device in storageDevices:
    good = True
    if not re.match('sd[a-z]\d+?', device['name']):
        for tmp in storageDevices:
            if tmp['name'] != device['name'] and tmp['name'].startswith(device['name']):
                good = False
                break
    if good:
        partitions.append(device)

fillMountPaths(partitions)

# remove root partition
partitions = list(filter(lambda part: '/' not in part['mounts'], partitions))

printDBG('Parttions: {}'.format(partitions))

if not partitions:
    ERROR('No valid device detected!')

# if list is longer than one, give user choice
if len(partitions) > 1:
    msg = "Please select partition which you want to use as E2iPlayer rootfs"
    answer = -1
    while answer < 0 or answer >= len(partitions):
        choiceList = []
        for x in xrange(len(partitions)):
            choiceList.append('%d. %s' % (x + 1, partitions[x]['name']))
        answer = raw_input(MSG_FORMAT.format(msg) + '\n' + '\n'.join(choiceList) + "\nYour choice: ").strip().upper()
        try: answer = int(answer) - 1
        except Exception: answer = -1
        msg = 'Unknown answer! Please try again.'
else:
    answer = 0

for idx, partition in enumerate(partitions):
    if idx != answer and ('/hdd' in partition['mounts'] or '/media/hdd' in partition['mounts']):
        ERROR('Parttion "{}" is already mounted as HDD {}, but you selected other "{}".\n\tThis is not supported.'.format(partition['name'], partition['mounts'], partitions[answer]['name']))

partition = partitions[answer]
printDBG("Selected partition: {}".format(partition))

if 1: # mabe made format only in case if partition do not have ext4 filesystem? 
    # perform format
    msg = 'There is need to perform format of the "{}" - all data stored on it will be lost!!!\nDo you want to proceed?'.format(partition['name'])
    answer = ''
    while answer not in ['Y', 'N']:
        answer = raw_input(MSG_FORMAT.format(msg) + "\nY/N: ").strip().upper()
        msg = 'Unknown answer! Please try again.'
    if answer == 'N':
        sys.exit(-1)

    partition['prev_mounts'] = partition['mounts']
    if partition['mounts']:
        os.system('umount /dev/' + partition['name'])

    partition['mounts'] = []
    fillMountPaths([partition])
    if partition['mounts']:
        ERROR('Parttion "{}" still mounted after umount!\n\tMount points: {}'.format(partition['name'], partition['mounts']))

    printMSG('Formatting "{}". It can take a long time depending on the partition size and device speed.\nPlease wait!'.format(partition['name']))

    ret = os.system('mkfs.ext4 -L "hdd" -i 524288 -b 4096 -m 0 /dev/' + partition['name'])
    if ret != 0:
        ERROR('Parttion "{}" format failed!'.format(partition['name']))

    ret = os.system('mkdir -p ' + mountPath)
    if ret != 0:
        ERROR('Directory "{}" creation failed!'.format(mountPath))

    ret = os.system('mount -t ext4 /dev/{} {}'.format(partition['name'], mountPath))
    if ret != 0:
        ERROR('Mount "/dev/{}" on "{}" failed!'.format(partition['name'], mountPath))

    # remove E2IPLAYER_EXTENSION_PATH
    ret = os.system('rm -rf {}'.format(E2IPLAYER_EXTENSION_PATH))
    if ret != 0:
        ERROR('Removing "{}" failed!'.format(E2IPLAYER_EXTENSION_PATH))

    ret = os.system('mkdir -p {}/IPTVPlayer && ln -s {}/IPTVPlayer {}'.format(mountPath, mountPath, E2IPLAYER_EXTENSION_PATH))
    if ret != 0:
        ERROR('Creating link to "{}/IPTVPlayer" failed!'.format(mountPath))

    # remove E2IPLAYER_ROOTFS
    ret = os.system('rm -rf {}'.format(E2IPLAYER_ROOTFS))
    if ret != 0:
        ERROR('Removing "{}" failed!'.format(E2IPLAYER_ROOTFS))

    ret = os.system('mkdir -p {}/e2irootfs && ln -s {}/e2irootfs {}'.format(mountPath, mountPath, E2IPLAYER_ROOTFS))
    if ret != 0:
        ERROR('Creating link to "{}/e2irootfs" failed!'.format(mountPath))

    os.system('mkdir -p /hdd/e2irootfs/e2i_recording && mkdir -p /hdd/e2irootfs/e2i_temp && mkdir -p /hdd/e2irootfs/e2i_cache && mkdir -p /hdd/e2irootfs/e2i_config')
    printMSG('DONE!\nYou are ready to install OpenSSL and next E2iPlayer!'.format(partition['name']))

os.system('sync')

sys.exit(0)
##### END


# wget https://www.e2iplayer.gitlab.io/hddinit.sh -O - | /bin/sh
