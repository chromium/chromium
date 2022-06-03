#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Update the firebase project hosting the Super Size UI."""

import argparse
import os
import requests
import shutil
import subprocess
import sys
import tempfile
import uuid


FIREBASE_PROJECT = 'chrome-supersize'
PROD_URL = 'https://chrome-supersize.firebaseapp.com/'
CASPIAN_FILES = [
    'caspian_web.js',
    'caspian_web.wasm',
    'caspian_web.wasm.map',
]

PROD = 'prod'
STAGING = 'staging'
DEV = 'dev'


def _FirebaseLogin():
  """Login into the Firebase CLI"""
  subprocess.check_call(['firebase', 'login'])


def _CheckFirebaseCLI():
  """Fail with a proper error message, if Firebase CLI is not installed."""
  if subprocess.call(['firebase', '--version'], stdout=subprocess.DEVNULL) != 0:
    link = 'https://firebase.google.com/docs/cli#install_the_firebase_cli'
    raise Exception('Firebase CLI not installed or not on your PATH. Follow '
                    'the instructions at ' + link + ' to install')


def _FirebaseInitProjectDir(project_dir):
  """Create a firebase.json file that is needed for deployment."""
  static_dir = os.path.join(project_dir, 'public')
  with open(os.path.join(project_dir, 'firebase.json'), 'w') as f:
    f.write("""
{
  "hosting": {
    "public": "public",
    "ignore": [
      "firebase.json",
      "**/README*",
      "**/.*"
    ]
  }
}
""")
  return static_dir


def _FirebaseDeploy(project_dir, deploy_mode=PROD):
  """Deploy the project to firebase hosting."""
  if deploy_mode == DEV:
    subprocess.check_call([
        'firebase', '-P', FIREBASE_PROJECT, 'emulators:start', '--only',
        'hosting'
    ],
                          cwd=project_dir)
  elif deploy_mode == STAGING:
    print('Note: deploying to staging requires firebase cli >= 8.12.0')
    subprocess.check_call([
        'firebase', '-P', FIREBASE_PROJECT, 'hosting:channel:deploy', 'staging'
    ],
                          cwd=project_dir)
  else:
    subprocess.check_call(['firebase', 'deploy', '-P', FIREBASE_PROJECT],
                          cwd=project_dir)


def _DownloadCaspianFiles(project_static_dir):
  for f in CASPIAN_FILES:
    response = requests.get(PROD_URL + f)
    with open(os.path.join(project_static_dir, f), 'wb') as output:
      output.write(response.content)


def _CopyStaticFiles(project_static_dir):
  """Copy over static files from the static directory."""
  static_files = os.path.join(os.path.dirname(__file__), 'static')
  shutil.copytree(static_files, project_static_dir)
  if not all(
      os.path.exists(os.path.join(static_files, f)) for f in CASPIAN_FILES):
    print('Some caspian files do not exist in ({}). Downloading *all* caspian '
          'files from currently deployed instance.'.format(static_files))
    _DownloadCaspianFiles(project_static_dir)


def _FillInAndCopyTemplates(project_static_dir):
  """Generate and copy over the templates/sw.js file."""
  template_file = os.path.join(os.path.dirname(__file__), 'templates', 'sw.js')
  cache_hash = uuid.uuid4().hex

  with open(template_file, 'r') as in_file:
    with open(os.path.join(project_static_dir, 'sw.js'), 'w') as out_file:
      out_file.write(in_file.read().replace('{{cache_hash}}', cache_hash))


def _Prompt(message):
  """Prompt the user with a message and request affirmative outcome."""
  choice = input(message + ' [y/N] ').lower()
  return choice and choice[0] == 'y'


def main():
  parser = argparse.ArgumentParser()
  deployment_mode_group = parser.add_mutually_exclusive_group(required=True)
  deployment_mode_group.add_argument('--local',
                                     action='store_const',
                                     dest='deploy_mode',
                                     const=DEV,
                                     help='Deploy a locally hosted server.')
  deployment_mode_group.add_argument(
      '--staging',
      action='store_const',
      dest='deploy_mode',
      const=STAGING,
      help='Deploy to staging channel (does not support authenticated '
      'requests).')
  deployment_mode_group.add_argument('--prod',
                                     action='store_const',
                                     dest='deploy_mode',
                                     const=PROD,
                                     help='Deploy to prod.')
  options = parser.parse_args()

  message = (
  """This script deploys the contents of //tools/binary_size/libsupersize/static
to firebase hosting at chrome-supersize.firebaseapp.com. Please ensure you have
read the instructions at //tools/binary_size/libsupersize/static/README.md first
before running this. Are you sure you want to continue?""")

  if options.deploy_mode != PROD or _Prompt(message):
    _CheckFirebaseCLI()
    _FirebaseLogin()
    with tempfile.TemporaryDirectory(prefix='firebase-') as project_dir:
      static_dir = _FirebaseInitProjectDir(project_dir)
      _CopyStaticFiles(static_dir)
      _FillInAndCopyTemplates(static_dir)
      _FirebaseDeploy(project_dir, deploy_mode=options.deploy_mode)
  else:
    print('Nothing was deployed.')


if __name__ == '__main__':
  main()
