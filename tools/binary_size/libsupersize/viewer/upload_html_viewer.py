#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Update the firebase project hosting the Super Size UI."""

import argparse
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import uuid

_VIEWER_DIR = pathlib.Path(__file__).parent.resolve()
_STATIC_FILES_DIR = _VIEWER_DIR / 'static'

_FIREBASE_PROJECT = 'chrome-supersize'
_PROD_URL = 'https://chrome-supersize.firebaseapp.com/'
_WASM_FILES = [
    'caspian_web.js',
    'caspian_web.wasm',
]
_DEBUG_WASM_FILES = [
    'caspian_web.wasm.debug.wasm',
]

_PROD = 'prod'
_STAGING = 'staging'
_DEV = 'dev'


def _FirebaseLogin():
  """Login into the Firebase CLI"""
  subprocess.check_call(['firebase', 'login', '--no-localhost'])


def _CheckFirebaseCLI():
  """Fail with a proper error message, if Firebase CLI is not installed."""
  if subprocess.call(['firebase', '--version'], stdout=subprocess.DEVNULL) != 0:
    link = 'https://firebase.google.com/docs/cli#install_the_firebase_cli'
    raise Exception('Firebase CLI not installed or not on your PATH. Follow '
                    'the instructions at ' + link + ' to install')


def _FirebaseInitProjectDir(project_dir):
  """Create a firebase.json file that is needed for deployment."""
  static_dir = project_dir / 'public'
  project_dir.joinpath('firebase.json').write_text("""\
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


def _FirebaseDeploy(project_dir, deploy_mode=_PROD):
  """Deploy the project to firebase hosting."""
  if deploy_mode == _DEV:
    subprocess.check_call([
        'firebase', '-P', _FIREBASE_PROJECT, 'emulators:start', '--only',
        'hosting'
    ],
                          cwd=project_dir)
  elif deploy_mode == _STAGING:
    print('Note: deploying to staging requires firebase cli >= 8.12.0')
    subprocess.check_call([
        'firebase', '-P', _FIREBASE_PROJECT, 'hosting:channel:deploy', 'staging'
    ],
                          cwd=project_dir)
  else:
    subprocess.check_call(['firebase', 'deploy', '-P', _FIREBASE_PROJECT],
                          cwd=project_dir)


def _MaybeDownloadWasmFiles(force_download):
  """Download WASM files if they are missing or stale."""
  if not force_download:
    if not all(_STATIC_FILES_DIR.joinpath(f).exists() for f in _WASM_FILES):
      print(f'Some WASM files do not exist in: {_STATIC_FILES_DIR}')
      force_download = True

  if force_download:
    for f in _WASM_FILES:
      print(f'Downloading: {_PROD_URL + f}')
      with urllib.request.urlopen(_PROD_URL + f) as response:
        with _STATIC_FILES_DIR.joinpath(f).open('wb') as output:
          shutil.copyfileobj(response, output)


def _FillInAndCopyTemplates(project_static_dir):
  """Generate and copy over the templates/sw.js file."""
  src_path = _VIEWER_DIR / 'templates' / 'sw.js'
  dst_path = project_static_dir / 'sw.js'
  cache_hash = uuid.uuid4().hex
  dst_path.write_text(src_path.read_text().replace('{{cache_hash}}',
                                                   cache_hash))


def _CopyStaticFiles(project_static_dir, *, include_debug_wasm):
  shutil.copytree(_STATIC_FILES_DIR, project_static_dir)
  # Don't upload the debug info since it's machine-dependent and large.
  if not include_debug_wasm:
    for f in _DEBUG_WASM_FILES:
      project_static_dir.joinpath(f).unlink(missing_ok=True)


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
                                     const=_DEV,
                                     help='Deploy a locally hosted server.')
  deployment_mode_group.add_argument(
      '--staging',
      action='store_const',
      dest='deploy_mode',
      const=_STAGING,
      help='Deploy to staging channel (does not support authenticated '
      'requests).')
  deployment_mode_group.add_argument('--prod',
                                     action='store_const',
                                     dest='deploy_mode',
                                     const=_PROD,
                                     help='Deploy to prod.')
  parser.add_argument('--download-wasm',
                      action='store_true',
                      help='Update local copy of WASM files.')
  options = parser.parse_args()

  message = (f'This script deploys the viewer to {_PROD_URL}.\n'
             'Are you sure you want to continue?')

  if options.deploy_mode != _PROD or _Prompt(message):
    _CheckFirebaseCLI()
    if options.deploy_mode != _DEV:
      _FirebaseLogin()
    with tempfile.TemporaryDirectory(prefix='firebase-') as project_dir:
      project_dir = pathlib.Path(project_dir)
      _MaybeDownloadWasmFiles(options.download_wasm)
      project_static_dir = _FirebaseInitProjectDir(project_dir)
      _CopyStaticFiles(project_static_dir,
                       include_debug_wasm=options.deploy_mode == _DEV)
      _FillInAndCopyTemplates(project_static_dir)
      _FirebaseDeploy(project_dir, deploy_mode=options.deploy_mode)
  else:
    print('Nothing was deployed.')


if __name__ == '__main__':
  main()
