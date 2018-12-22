#!/usr/bin/env python
# -*- coding: utf-8 -*-

debug_modules = [
{'name':'ffmpeg',        'lvl':0, 'silent':True},
{'name':'container',     'lvl':0, 'silent':True},
{'name':'latmenc',       'lvl':0, 'silent':True},
{'name':'audio_mgr',     'lvl':0, 'silent':True},
{'name':'subtitle_mgr',  'lvl':0, 'silent':True},
{'name':'video_mgr',     'lvl':0, 'silent':True},
{'name':'linuxdvb',      'lvl':0, 'silent':True},
{'name':'buff',          'lvl':0, 'silent':True},
{'name':'output',        'lvl':0, 'silent':True},
{'name':'subtitle',      'lvl':0, 'silent':True},
{'name':'writer',        'lvl':0, 'silent':True},
{'name':'playback',      'lvl':0, 'silent':True},
{'name':'aac',           'lvl':0, 'silent':True},
{'name':'ac3',           'lvl':0, 'silent':True},
{'name':'amr',           'lvl':0, 'silent':True},
{'name':'divx',          'lvl':0, 'silent':True},
{'name':'dts',           'lvl':0, 'silent':True},
{'name':'h263',          'lvl':0, 'silent':True},
{'name':'h264',          'lvl':0, 'silent':True},
{'name':'h265',          'lvl':0, 'silent':True},
{'name':'lpcm',          'lvl':0, 'silent':True},
{'name':'mp3',           'lvl':0, 'silent':True},
{'name':'mpeg2',         'lvl':0, 'silent':True},
{'name':'mpeg4',         'lvl':0, 'silent':True},
{'name':'pcm',           'lvl':0, 'silent':True},
{'name':'vc1',           'lvl':0, 'silent':True},
{'name':'vp',            'lvl':0, 'silent':True},
{'name':'wma',           'lvl':0, 'silent':True},
{'name':'wmv',           'lvl':0, 'silent':True},
]

lines = ['#include <stdio.h>']
lines.append('#define log_error(fmt, x...) do { printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)')
lines.append('#define log_printf(maxlevel, level, fmt, x...) do { if (maxlevel >= level) printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)')
lines.append('')

for item in debug_modules:
    lines.append('')
    lines.append('/*******************************************')
    lines.append(' * %s' % item['name'])
    lines.append(' *******************************************/')
    debug_name = '%s_DEBUG_LEVEL' % item['name'].upper()
    lines.append('#define %s %d' % (debug_name, item['lvl']))

    silent_name = '%s_SILENT' % item['name'].upper()
    silent_def = '#define %s' % silent_name
    if not item['silent']: silent_def = '// ' + silent_def
    lines.append(silent_def)

    lines.append('')
    lines.append('#if %s' % debug_name)
    lines.append('#define %s_printf(...) log_printf(%s, __VA_ARGS__)' % (item['name'], debug_name))
    lines.append('#else')
    lines.append('#define %s_printf(...)' % item['name'])
    lines.append('#endif')

    lines.append('')
    lines.append('#ifndef %s' % silent_name)
    lines.append('#define %s_err(...) log_error(__VA_ARGS__)' % item['name'])
    lines.append('#else')
    lines.append('#define %s_err(...)' % item['name'])
    lines.append('#endif')

f = open('include/debug.h', "w")
f.write('\n'.join(lines))
f.close()
