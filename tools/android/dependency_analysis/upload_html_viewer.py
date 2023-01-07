#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Deploy the Dependency Graph Viewer to Firebase hosting."""

import shutil
import subprocess
import tempfile

from pathlib import Path

FIREBASE_PROJECT = 'chromium-dependency-graph'
JS_DIR = Path(__file__).parent / 'js'


def _Prompt(message):
    """Prompt the user with a message and request affirmative outcome."""
    choice = input(message + ' [y/N] ').lower()
    return choice and choice[0] == 'y'


def _FirebaseLogin():
    """Login into the Firebase CLI"""
    subprocess.check_call(['firebase', 'login'])


def _CheckFirebaseCLI():
    """Fail with a proper error message, if Firebase CLI is not installed."""
    if subprocess.call(['firebase', '--version'],
                       stdout=subprocess.DEVNULL) != 0:
        link = 'https://firebase.google.com/docs/cli#install_the_firebase_cli'
        raise Exception(
            'Firebase CLI not installed or not on your PATH. Follow '
            'the instructions at ' + link + ' to install')


def _CheckNPM():
    """Fail with a proper error message, if npm is not installed."""
    if subprocess.call(['npm', '--version'], stdout=subprocess.DEVNULL) != 0:
        link = 'https://nodejs.org'
        raise Exception(
            'npm not installed or not on your PATH. Either install Node.js '
            'through your package manager or download it from ' + link + '.')


def _BuildDist():
    """Build distribution files."""
    subprocess.check_call(['npm', 'run', '--prefix', JS_DIR, 'build'])
    return JS_DIR / 'dist'


def _FirebaseInitProjectDir(project_dir):
    """Create a firebase.json file that is needed for deployment."""
    project_static_dir = project_dir / 'public'
    with open(project_dir / 'firebase.json', 'w') as f:
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
    return project_static_dir


def _FirebaseDeploy(project_dir):
    """Deploy the project to firebase hosting."""
    subprocess.check_call(['firebase', 'deploy', '-P', FIREBASE_PROJECT],
                          cwd=project_dir)


def _CopyDistFiles(dist_dir, project_static_dir):
    """Copy over static files from the dist directory."""
    shutil.copytree(dist_dir, project_static_dir)


def main():
    message = (
        f"""This script builds the Clank Dependency Graph Visualizer and \
deploys it to Firebase hosting at {FIREBASE_PROJECT}.firebaseapp.com.

Please ensure you have read the instructions at //{JS_DIR}/README.md first \
before running this.

Are you sure you want to continue?""")

    if not _Prompt(message):
        print('Nothing was deployed.')
        return

    _CheckFirebaseCLI()
    _CheckNPM()
    _FirebaseLogin()
    dist_dir = _BuildDist()
    with tempfile.TemporaryDirectory(prefix='firebase-') as project_dir_str:
        project_dir = Path(project_dir_str)
        project_static_dir = _FirebaseInitProjectDir(project_dir)
        shutil.copytree(dist_dir, project_static_dir)
        _FirebaseDeploy(project_dir)


if __name__ == '__main__':
    main()
