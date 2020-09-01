#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Update the firebase project hosting the Super Size UI."""

import os
import shutil
import subprocess
import sys
import tempfile
import uuid


FIREBASE_PROJECT = 'chrome-supersize'


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


def _FirebaseDeploy(project_dir):
  """Deploy the project to firebase hosting."""
  subprocess.check_call(['firebase', 'deploy', '-P', FIREBASE_PROJECT],
                        cwd=project_dir)


def _CopyStaticFiles(project_static_dir):
  """Copy over static files from the static directory."""
  static_files = os.path.join(os.path.dirname(__file__), 'static')
  if not os.path.exists(os.path.join(static_files, 'caspian_web.js')):
    raise Exception('static/caspian_web.js is missing. See caspian/README.md')
  shutil.copytree(static_files, project_static_dir)


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
  message = (
  """This script deploys the contents of //tools/binary_size/libsupersize/static
to firebase hosting at chrome-supersize.firebaseapp.com. Please ensure you have
read the instructions at //tools/binary_size/libsupersize/static/README.md first
before running this. Are you sure you want to continue?""")

  if _Prompt(message):
    _CheckFirebaseCLI()
    _FirebaseLogin()
    with tempfile.TemporaryDirectory(prefix='firebase-') as project_dir:
      static_dir = _FirebaseInitProjectDir(project_dir)
      _CopyStaticFiles(static_dir)
      _FillInAndCopyTemplates(static_dir)
      _FirebaseDeploy(project_dir)
  else:
    print('Nothing was deployed.')


if __name__ == '__main__':
  main()
