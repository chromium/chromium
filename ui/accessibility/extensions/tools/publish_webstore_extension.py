#!/usr/bin/env python

# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Publishes a set of extensions to the webstore.
  Given an unpacked extension, compresses and sends to the Chrome webstore.

  Releasing to the webstore should involve the following manual steps before
      running this script:
    1. clean the output directory.
    2. make a release build.
    3. run manual smoke tests.
    4. run automated tests.
'''

import webstore_extension_util
import generate_manifest
import json
import optparse
import os
import sys
import tempfile
from zipfile import ZipFile

_CHROMEVOX_ID = 'kgejglhpjiefppelpmljglcjbhoiplfn'
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CHROME_SOURCE_DIR = os.path.normpath(
    os.path.join(_SCRIPT_DIR, *[os.path.pardir] * 3))

sys.path.insert(0, os.path.join(_CHROME_SOURCE_DIR, 'build', 'util'))
import version

# A list of files (or directories) to exclude from the webstore build.
EXCLUDE_PATHS = [
    'manifest.json',
    'manifest_guest.json',
]


def CreateOptionParser():
  parser = optparse.OptionParser(description=__doc__)
  parser.usage = (
      '%prog --client_secret <client_secret> extension_id:extension_path ...')
  parser.add_option(
      '-c',
      '--client_secret',
      dest='client_secret',
      action='store',
      metavar='CLIENT_SECRET')
  parser.add_option(
      '-p', '--publish', action='store_true', help='publish the extension(s)')
  return parser


def GetVersion():
  '''Returns the chrome version string.'''
  filename = os.path.join(_CHROME_SOURCE_DIR, 'chrome', 'VERSION')
  values = version.FetchValues([filename])
  return version.SubstTemplate('@MAJOR@.@MINOR@.@BUILD@.@PATCH@', values)


def MakeChromeVoxManifest():
  '''Create a manifest for the webstore.

  Returns:
    Temporary file with generated manifest.
  '''
  new_file = tempfile.NamedTemporaryFile(mode='w+a', bufsize=0)
  in_file_name = os.path.join(_SCRIPT_DIR, os.path.pardir,
                              'manifest.json.jinja2')
  context = {
      'is_guest_manifest': '0',
      'is_js_compressed': '1',
      'is_webstore': '1',
      'set_version': GetVersion()
  }
  generate_manifest.processJinjaTemplate(in_file_name, new_file.name, context)
  return new_file


def RunInteractivePrompt(client_secret, output_path):
  input = ''
  while True:
    print 'u upload'
    print 'g get upload status'
    print 't publish trusted tester'
    print 'p publish public'
    print 'q quit'
    input = raw_input('Please select an option: ')
    input = input.strip()
    if input == 'g':
      print('Upload status: %s' %
            webstore_extension_util.GetUploadStatus(client_secret).read())
    elif input == 'u':
      print('Uploaded with status: %s' % webstore_extension_util.PostUpload(
          output_path.name, client_secret))
    elif input == 't':
      print('Published to trusted testers with status: %s' %
            webstore_extension_util.PostPublishTrustedTesters(
                client_secret).read())
    elif input == 'p':
      print('Published to public with status: %s' %
            webstore_extension_util.PostPublish(client_secret).read())
    elif input == 'q':
      sys.exit()
    else:
      print 'Unrecognized option: %s' % input


def main():
  options, args = CreateOptionParser().parse_args()
  if len(args) < 1 or not options.client_secret:
    print 'Expected at least one argument and --client_secret flag'
    print str(args)
    sys.exit(1)

  client_secret = options.client_secret

  for extension in args:
    webstore_extension_util.g_app_id, extension_path = extension.split(':')
    output_path = tempfile.NamedTemporaryFile()
    extension_path = os.path.expanduser(extension_path)

    is_chromevox = webstore_extension_util.g_app_id == _CHROMEVOX_ID

    with ZipFile(output_path, 'w') as zip:
      for root, dirs, files in os.walk(extension_path):
        rel_path = os.path.join(os.path.relpath(root, extension_path), '')

        if is_chromevox and rel_path in EXCLUDE_PATHS:
          continue

        for extension_file in files:
          if is_chromevox and extension_file in EXCLUDE_PATHS:
            continue

          zip.write(
              os.path.join(root, extension_file),
              os.path.join(rel_path, extension_file))

      if is_chromevox:
        manifest_file = MakeChromeVoxManifest()
        zip.write(manifest_file.name, 'manifest.json')

    print 'Created extension zip file in %s' % output_path.name
    print 'Please run manual smoke tests before proceeding.'
    if options.publish:
      print('Uploading...%s' % webstore_extension_util.PostUpload(
          output_path.name, client_secret))
      print('publishing...%s' %
            webstore_extension_util.PostPublish(client_secret).read())
    else:
      RunInteractivePrompt(client_secret, output_path)


if __name__ == '__main__':
  main()
