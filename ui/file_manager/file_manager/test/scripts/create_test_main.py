#!/usr/bin/env python

# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Copies file_manager/main.html to file_manager/test.html.

Modifies it to be able to run the CrOS FileManager app
as a regular web page in a single renderer.
"""


import argparse
import json
import os
import re
import shutil
import sys
import time
import xml.etree.ElementTree as ET


assert __name__ == '__main__'

parser = argparse.ArgumentParser()
parser.add_argument('--output')
args = parser.parse_args()

# If --output is not provided, write to local test.html.
output = args.output or os.path.abspath(
    os.path.join(sys.path[0], '../../test.html'))

# SRC   : Absolute path to //src/.
# GEN   : Absolute path to $target_gen_dir.
# ROOT  : Relative path from GEN to //src/ui/file_manager/file_manager.
# CC_SRC: Source directory for components-chromium.
# CC_GEN: Directory where components-chromium is copied to.
SRC = os.path.abspath(os.path.join(sys.path[0], '../../../../..')) + '/'
GEN = os.path.dirname(os.path.abspath(args.output)) + '/'
ROOT = os.path.relpath(SRC, GEN) + '/ui/file_manager/file_manager/'
CC_SRC = 'third_party/polymer/v1_0/components-chromium/'
CC_GEN = 'test/gen/cc/'
scripts = []
GENERATED = 'Generated at %s by: %s' % (time.ctime(), sys.path[0])
GENERATED_HTML = '<!-- %s -->\n\n' % GENERATED


def read(path):
  with open(os.path.join(SRC, path)) as f:
    return f.read()


def write(path, content):
  fullpath = os.path.join(GEN, path)
  if not os.path.exists(os.path.dirname(fullpath)):
    os.makedirs(os.path.dirname(fullpath))
  with open(fullpath, 'w') as f:
    f.write(content)


def replaceline(f, match, lines):
  """Replace matching line in file with lines."""
  for i in range(len(f)):
    if match in f[i]:
      return f[:i] + lines + f[i+1:]
  return f


def includes2scripts(include_filename):
  """Convert <include src='foo'> to <script src='<prefix>foo'></script>."""
  scripts.append('<!-- %s -->' % include_filename)
  prefix = ROOT + include_filename[:include_filename.rindex('/')+1]
  f = read('ui/file_manager/file_manager/' + include_filename).split('\n')
  for i in range(len(f)):
    l = f[i]
    # Join back any include with a line-break.
    if l == '// <include' and f[i+1].startswith('// src='):
      f[i+1] = l + f[i+1][2:]
      continue
    if l.startswith('// <include '):
      l = l.replace('// <include ', '<script ')
      # main.js should be defer.
      if 'src="main.js"' in l:
        l = l.replace('src="main.js"', 'src="main.js" defer')
      # Fix the path for scripts to be relative to ROOT.
      if 'src="../../' in l:
        l = l.replace('src="../../', 'src="' + ROOT)
      else:
        l = l.replace('src="', 'src="' + prefix)
      tag = l + '</script>'
      if tag not in scripts:
        scripts.append(tag)

# Get strings from grdp files.  Remove any ph/ex elements before getting text.
# Parse private_api_strings.cc to match the string name to the grdp message.
strings = {
    'fontFamily': 'Roboto, sans-serif',
    'fontSize': '75%',
    'language': 'en',
    'textDirection': 'ltr',
    }
grdp_files = [
    'chrome/app/chromeos_strings.grdp',
    'ui/chromeos/file_manager_strings.grdp',
    ]
resource_bundle = {}
for grdp in grdp_files:
  for msg in ET.fromstring(read(grdp)).iter('message'):
    for ph in msg.findall('ph'):
      for ex in ph.findall('ex'):
        ph.remove(ex)
    resource_bundle[msg.attrib['name']] = ''.join(msg.itertext()).strip()
private_api_strings = read('chrome/browser/chromeos/'
                           'file_manager/file_manager_string_util.cc')
for m in re.finditer(r'SET_STRING\(\"(.*?)\",\s+(\w+)\);', private_api_strings):
  strings[m.group(1)] = resource_bundle.get(m.group(2), m.group(2))


# Substitute $i18n{} and $i18nRaw{} in template with strings from grdp files.
def i18n(template):
  repl = lambda x: strings.get(x.group(1), x.group())
  return re.sub(r'\$i18n(?:Raw)?\{(.*?)\}', repl, template)

# Substitute $i18n{}.
# Update relative paths.
# Fix link to action_link.css and text_defaults.css.
# Fix stylesheet from extension.
main_html = (i18n(read('ui/file_manager/file_manager/main.html'))
             .replace('chrome://resources/polymer/v1_0/', '../../../' + CC_SRC)
             .replace('chrome://resources/css/action_link.css',
                      '../../webui/resources/css/action_link.css')
             .replace('href="', 'href="' + ROOT)
             .replace('src="', 'src="' + ROOT)
             .replace(ROOT + 'chrome://resources/html/', CC_GEN + 'polymer/')
             .replace(ROOT + 'chrome://resources/css/text_defaults.css',
                      'test/gen/css/text_defaults.css')
             .split('\n'))

# Fix text_defaults.css.  Copy and replace placeholders.
text_defaults = i18n(read('ui/webui/resources/css/text_defaults.css'))
write('test/gen/css/text_defaults.css', text_defaults)

# Add scripts required for testing, and the test files (test/*.js).
src = [
    'test/js/chrome_api_test_impl.js',
    '../../webui/resources/js/assert.js',
    '../../webui/resources/js/cr.js',
    '../../webui/resources/js/cr/event_target.js',
    '../../webui/resources/js/cr/ui/array_data_model.js',
    '../../webui/resources/js/load_time_data.js',
    '../../webui/resources/js/webui_resource_test.js',
    'test/js/strings.js',
    'common/js/files_app_entry_types.js',
    'common/js/util.js',
    'common/js/mock_entry.js',
    '../base/js/volume_manager_types.js',
    'background/js/volume_info_impl.js',
    'background/js/volume_info_list_impl.js',
    'background/js/volume_manager_impl.js',
    'background/js/mock_volume_manager.js',
    'foreground/js/constants.js',
    'test/js/chrome_file_manager_private_test_impl.js',
    'test/js/test_util.js',
]
src += ['test/' + s for s in os.listdir(
    SRC + 'ui/file_manager/file_manager/test') if s.endswith('.js')]
scripts.append('<!-- required for testing -->')
scripts += ['<script src="%s%s"></script>' % (ROOT, s) for s in src]

# Convert all includes from:
#  * foreground/js/main_scripts.js
#  * background/js/background_common_scripts.js
#  * background/js/background_scripts.js
# into <script> tags in main.html.
# Add polymer libs at start.
# Define FILE_MANAGER_ROOT which is required to locate test data files.
includes2scripts('foreground/js/main_scripts.js')
includes2scripts('background/js/background_common_scripts.js')
includes2scripts('background/js/background_scripts.js')

# test_util_base.js in background_common_scripts.js loads this at runtime.
# However, test/js/test_util.js copies some functions from it into its own
# test context, so provide it here.
scripts += ['<script src="%s%s"></script>' %
            (ROOT, 'background/js/runtime_loaded_test_util.js')]

main_html = replaceline(main_html, 'foreground/js/main_scripts.js', [
    "<script>var FILE_MANAGER_ROOT = '%s';</script>" % ROOT,
    ] + scripts)


def elements_path(elements_filename):
  return '../../../../%sforeground/elements/%s' % (ROOT, elements_filename)

# Copy all files from //third_party/polymer/v1_0/components-chromium/ into
# test/gen/cc.  Rename polymer2 to polymer.
gen_cc = GEN + CC_GEN
if os.path.exists(gen_cc):
  shutil.rmtree(gen_cc)
shutil.copytree(SRC + CC_SRC, gen_cc)
shutil.rmtree(gen_cc + 'polymer')
os.rename(gen_cc + 'polymer2', gen_cc + 'polymer')

# Generate strings.js
# Copy all html files in foreground/elements and fix references to polymer
# Load QuickView in iframe rather than webview.
for filename, substitutions in (
    ('test/js/strings.js', (
        ('$GRDP', json.dumps(strings, sort_keys=True, indent=2)),
    )),
    ('foreground/elements/elements_bundle.html', (
        ('="files_xf', '="' + elements_path('files_xf')),
    )),
    ('foreground/js/elements_importer.js', (
        ("= 'foreground", "= 'test/gen/foreground"),
    )),
    ('foreground/elements/files_icon_button.html', ()),
    ('foreground/elements/files_metadata_box.html', ()),
    ('foreground/elements/files_metadata_entry.html', ()),
    ('foreground/elements/files_quick_view.html', (
        ('="files_quick', '="' + elements_path('files_quick')),
        ('webview', 'iframe'),
    )),
    ('foreground/elements/files_ripple.html', ()),
    ('foreground/elements/files_safe_media.html', (
        ('webview', 'iframe'),
        ('src="../../../../%sforeground/elements/' % ROOT, 'src="'),
    )),
    ('foreground/elements/files_safe_media.js', (
        ("'foreground/elements", "'%sforeground/elements" % ROOT),
        ("'webview'", "'iframe'"),
        ("'contentload'", "'load'"),
        ('this.webview_.contentWindow.postMessage(data, FILES_APP_ORIGIN);',
         ('this.webview_.contentWindow.content.type = this.type;'
          'this.webview_.contentWindow.content.src = this.src;')),
    )),
    ('foreground/elements/files_toast.html', ()),
    ('foreground/elements/files_toggle_ripple.html', ()),
    ('foreground/elements/files_tooltip.html', ()),
    ('foreground/elements/icons.html', ()),
    ):
  buf = i18n(read('ui/file_manager/file_manager/' + filename))
  buf = buf.replace('chrome://resources/html/', '../../cc/polymer/')
  buf = buf.replace('chrome://resources/polymer/v1_0/', '../../cc/')
  buf = buf.replace('<link rel="import" href="chrome://resources/cr_elements/'
                    'cr_input/cr_input.html">', '')
  buf = buf.replace('<link rel="import" href="chrome://resources/cr_elements/'
                    'cr_button/cr_button.html">', '')
  buf = buf.replace('src="files_', 'src="' + elements_path('files_'))
  # The files_format_dialog and files_message import various files that are
  # not available in a ui test, just ignore them completely.
  buf = buf.replace('<link rel="import" href="files_format_dialog.html">', '')
  buf = buf.replace('<link rel="import" href="files_message.html">', '')
  for old, new in substitutions:
    buf = buf.replace(old, new)
  write('test/gen/' + filename, buf)
  main_html = replaceline(main_html, filename,
                          ['<script src="test/gen/%s"></script>' % filename])

test_html = GENERATED_HTML + '\n'.join(main_html).encode('utf-8')
write('test.html', test_html)
