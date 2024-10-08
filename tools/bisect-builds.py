#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Snapshot Build Bisect Tool

This script bisects a snapshot archive using binary search. It starts at
a bad revision (it will try to guess HEAD) and asks for a last known-good
revision. It will then binary search across this revision range by downloading,
unzipping, and opening Chromium for you. After testing the specific revision,
it will ask you whether it is good or bad before continuing the search.
"""

import abc
import argparse
import base64
import copy
import glob
import importlib
import json
import os
import platform
import re
import shlex
import subprocess
import sys
import tarfile
import tempfile
import threading
import traceback
import urllib.request, urllib.parse
from distutils.version import LooseVersion as BaseLooseVersion
from xml.etree import ElementTree
import zipfile

# These constants are used for android bisect which depends on
# Catapult repo.
DEFAULT_CATAPULT_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), 'catapult_bisect_dep'))
CATAPULT_DIR = os.environ.get('CATAPULT_DIR', DEFAULT_CATAPULT_DIR)
CATAPULT_REPO = 'https://github.com/catapult-project/catapult.git'
DEVIL_PATH = os.path.abspath(os.path.join(CATAPULT_DIR, 'devil'))

# The base URL for stored build archives.
CHROMIUM_BASE_URL = ('http://commondatastorage.googleapis.com'
                     '/chromium-browser-snapshots')
ASAN_BASE_URL = ('http://commondatastorage.googleapis.com'
                 '/chromium-browser-asan')

GSUTILS_PATH = None

# GS bucket name for perf builds
PERF_BASE_URL = 'gs://chrome-test-builds/official-by-commit'
# GS bucket name.
RELEASE_BASE_URL = 'gs://chrome-unsigned/desktop-5c0tCh'

# Android bucket starting at M45.
ANDROID_RELEASE_BASE_URL = 'gs://chrome-unsigned/android-B0urB0N'
ANDROID_RELEASE_BASE_URL_SIGNED = 'gs://chrome-signed/android-B0urB0N'

# A special bucket that need to be skipped.
ANDROID_INVALID_BUCKET = 'gs://chrome-signed/android-B0urB0N/Test'

# iOS bucket
IOS_RELEASE_BASE_URL = 'gs://chrome-unsigned/ios-G1N'
IOS_RELEASE_BASE_URL_SIGNED = 'gs://chrome-signed/ios-G1N'
IOS_ARCHIVE_BASE_URL = 'gs://bling-archive'

# Base URL for downloading release builds.
GOOGLE_APIS_URL = 'commondatastorage.googleapis.com'

# URL template for viewing changelogs between revisions.
CHANGELOG_URL = ('https://chromium.googlesource.com/chromium/src/+log/%s..%s')

# URL to convert SVN revision to git hash.
CRREV_URL = ('https://cr-rev.appspot.com/_ah/api/crrev/v1/redirect/')

# URL template for viewing changelogs between release versions.
RELEASE_CHANGELOG_URL = ('https://chromium.googlesource.com/chromium/'
                         'src/+log/%s..%s?pretty=fuller&n=10000')

# DEPS file URL.
DEPS_FILE_OLD = ('http://src.chromium.org/viewvc/chrome/trunk/src/'
                 'DEPS?revision=%d')
DEPS_FILE_NEW = ('https://chromium.googlesource.com/chromium/src/+/%s/DEPS')

# Source Tag
SOURCE_TAG_URL = ('https://chromium.googlesource.com/chromium/src/'
                  '+/refs/tags/%s?format=JSON')


DONE_MESSAGE_GOOD_MIN = ('You are probably looking for a change made after %s ('
                         'known good), but no later than %s (first known bad).')
DONE_MESSAGE_GOOD_MAX = ('You are probably looking for a change made after %s ('
                         'known bad), but no later than %s (first known good).')

VERSION_INFO_URL = ('https://chromiumdash.appspot.com/fetch_version?version=%s')

MILESTONES_URL = ('https://chromiumdash.appspot.com/fetch_milestones?mstone=%s')

CREDENTIAL_ERROR_MESSAGE = ('You are attempting to access protected data with '
                            'no configured credentials')
PATH_CONTEXT = {
    'release': {
        'android-arm': {
            # Binary name is the Chrome binary filename. On Android, we don't
            # use it to launch Chrome.
            'binary_name': None,
            'listing_platform_dir': 'arm/',
            # Archive name is the zip file on gcs. For Android, we don't have
            # such zip file. Instead we have a lot of apk files directly stored
            # on gcs. The archive_name is used to find zip file for other
            # platforms, but it will be apk filename defined by --apk for
            # Android platform.
            'archive_name': None,
            'archive_extract_dir': 'android-arm'
        },
        'android-arm64': {
            'binary_name': None,
            'listing_platform_dir': 'arm_64/',
            'archive_name': None,
            'archive_extract_dir': 'android-arm64'
        },
        'android-arm64-high': {
            'binary_name': None,
            'listing_platform_dir': 'high-arm_64/',
            'archive_name': None,
            'archive_extract_dir': 'android-arm64'
        },
        'android-x86': {
            'binary_name': None,
            'listing_platform_dir': 'x86/',
            'archive_name': None,
            'archive_extract_dir': 'android-x86'
        },
        'android-x64': {
            'binary_name': None,
            'listing_platform_dir': 'x86_64/',
            'archive_name': None,
            'archive_extract_dir': 'android-x64'
        },
        'ios': {
            'binary_name': None,
            'listing_platform_dir': 'ios/',
            'archive_name': None,
            'archive_extract_dir': None,
        },
        'ios-simulator': {
            'binary_name': 'Chromium.app',
            'listing_platform_dir': '',
            'archive_name': 'Chromium.tar.gz',
            'archive_extract_dir': None,
        },
        'linux64': {
            'binary_name': 'chrome',
            'listing_platform_dir': 'linux64/',
            'archive_name': 'chrome-linux64.zip',
            'archive_extract_dir': 'chrome-linux64',
            'chromedriver_binary_name': 'chromedriver',
            'chromedriver_archive_name': 'chromedriver_linux64.zip',
        },
        'mac': {
            'binary_name': 'Google Chrome.app/Contents/MacOS/Google Chrome',
            'listing_platform_dir': 'mac/',
            'archive_name': 'chrome-mac.zip',
            'archive_extract_dir': 'chrome-mac',
        },
        'mac64': {
            'binary_name': 'Google Chrome.app/Contents/MacOS/Google Chrome',
            'listing_platform_dir': 'mac64/',
            'archive_name': 'chrome-mac.zip',
            'archive_extract_dir': 'chrome-mac',
            'chromedriver_binary_name': 'chromedriver',
            'chromedriver_archive_name': 'chromedriver_mac64.zip',
        },
        'mac-arm': {
            'binary_name': 'Google Chrome.app/Contents/MacOS/Google Chrome',
            'listing_platform_dir': 'mac-arm64/',
            'archive_name': 'chrome-mac.zip',
            'archive_extract_dir': 'chrome-mac',
            'chromedriver_binary_name': 'chromedriver',
            'chromedriver_archive_name': 'chromedriver_mac64.zip',
        },
        'win': {
            'binary_name': 'chrome.exe',
            # Release builds switched to -clang in M64.
            'listing_platform_dir': 'win-clang/',
            'archive_name': 'chrome-win-clang.zip',
            'archive_extract_dir': 'chrome-win-clang',
            'chromedriver_binary_name': 'chromedriver.exe',
            'chromedriver_archive_name': 'chromedriver_win32.zip',
        },
        'win64': {
            'binary_name': 'chrome.exe',
            # Release builds switched to -clang in M64.
            'listing_platform_dir': 'win64-clang/',
            'archive_name': 'chrome-win64-clang.zip',
            'archive_extract_dir': 'chrome-win64-clang',
            'chromedriver_binary_name': 'chromedriver.exe',
            'chromedriver_archive_name': 'chromedriver_win64.zip',
        },
        'win-arm64': {
            'binary_name': 'chrome.exe',
            'listing_platform_dir': 'win-arm64-clang/',
            'archive_name': 'chrome-win-arm64-clang.zip',
            'archive_extract_dir': 'chrome-win-arm64-clang',
            'chromedriver_binary_name': 'chromedriver.exe',
            'chromedriver_archive_name': 'chromedriver_win64.zip',
        },
    },
    'official': {
        'android-arm': {
            'binary_name': None,
            'listing_platform_dir': 'android-builder-perf/',
            'archive_name': 'full-build-linux.zip',
            'archive_extract_dir': 'full-build-linux'
        },
        'android-arm64': {
            'binary_name': None,
            'listing_platform_dir': 'android_arm64-builder-perf/',
            'archive_name': 'full-build-linux.zip',
            'archive_extract_dir': 'full-build-linux'
        },
        'android-arm64-high': {
            'binary_name': None,
            'listing_platform_dir': 'android_arm64_high_end-builder-perf/',
            'archive_name': 'full-build-linux.zip',
            'archive_extract_dir': 'full-build-linux'
        },
        'linux64': {
            'binary_name': 'chrome',
            'listing_platform_dir': 'linux-builder-perf/',
            'archive_name': 'chrome-perf-linux.zip',
            'archive_extract_dir': 'full-build-linux',
            'chromedriver_binary_name': 'chromedriver',
        },
        'mac': {
            'binary_name': 'Google Chrome.app/Contents/MacOS/Google Chrome',
            'listing_platform_dir': 'mac-builder-perf/',
            'archive_name': 'chrome-perf-mac.zip',
            'archive_extract_dir': 'full-build-mac',
            'chromedriver_binary_name': 'chromedriver',
        },
        'mac-arm': {
            'binary_name': 'Google Chrome.app/Contents/MacOS/Google Chrome',
            'listing_platform_dir': 'mac-arm-builder-perf/',
            'archive_name': 'chrome-perf-mac.zip',
            'archive_extract_dir': 'full-build-mac',
            'chromedriver_binary_name': 'chromedriver',
        },
        'win64': {
            'binary_name': 'chrome.exe',
            'listing_platform_dir': 'win64-builder-perf/',
            'archive_name': 'chrome-perf-win.zip',
            'archive_extract_dir': 'full-build-win32',
            'chromedriver_binary_name': 'chromedriver.exe',
        },
    },
    'snapshot': {
        'android-arm': {
            'binary_name': None,
            'listing_platform_dir': 'Android/',
            'archive_name': 'chrome-android.zip',
            'archive_extract_dir': 'chrome-android'
        },
        'android-arm64': {
            'binary_name': None,
            'listing_platform_dir': 'Android_Arm64/',
            'archive_name': 'chrome-android.zip',
            'archive_extract_dir': 'chrome-android'
        },
        'linux64': {
            'binary_name': 'chrome',
            'listing_platform_dir': 'Linux_x64/',
            'archive_name': 'chrome-linux.zip',
            'archive_extract_dir': 'chrome-linux',
            'chromedriver_binary_name': 'chromedriver',
            'chromedriver_archive_name': 'chromedriver_linux64.zip',
        },
        'linux-arm': {
            'binary_name': 'chrome',
            'listing_platform_dir': 'Linux_ARM_Cross-Compile/',
            'archive_name': 'chrome-linux.zip',
            'archive_extract_dir': 'chrome-linux'
        },
        'chromeos': {
            'binary_name': 'chrome',
            'listing_platform_dir': 'Linux_ChromiumOS_Full/',
            'archive_name': 'chrome-chromeos.zip',
            'archive_extract_dir': 'chrome-chromeos'
        },
        'mac': {
            'binary_name': 'Chromium.app/Contents/MacOS/Chromium',
            'listing_platform_dir': 'Mac/',
            'archive_name': 'chrome-mac.zip',
            'archive_extract_dir': 'chrome-mac',
            'chromedriver_binary_name': 'chromedriver',
            'chromedriver_archive_name': 'chromedriver_mac64.zip',
        },
        'mac64': {
            'binary_name': 'Chromium.app/Contents/MacOS/Chromium',
            'listing_platform_dir': 'Mac/',
            'archive_name': 'chrome-mac.zip',
            'archive_extract_dir': 'chrome-mac',
            'chromedriver_binary_name': 'chromedriver',
            'chromedriver_archive_name': 'chromedriver_mac64.zip',
        },
        'mac-arm': {
            'binary_name': 'Chromium.app/Contents/MacOS/Chromium',
            'listing_platform_dir': 'Mac_Arm/',
            'archive_name': 'chrome-mac.zip',
            'archive_extract_dir': 'chrome-mac',
            'chromedriver_binary_name': 'chromedriver',
            'chromedriver_archive_name': 'chromedriver_mac64.zip',
        },
        'win': {
            'binary_name': 'chrome.exe',
            'listing_platform_dir': 'Win/',
            'archive_name': 'chrome-win.zip',
            'archive_extract_dir': 'chrome-win',
            'chromedriver_binary_name': 'chromedriver.exe',
            'chromedriver_archive_name': 'chromedriver_win32.zip',
        },
        'win64': {
            'binary_name': 'chrome.exe',
            'listing_platform_dir': 'Win_x64/',
            'archive_name': 'chrome-win.zip',
            'archive_extract_dir': 'chrome-win',
            'chromedriver_binary_name': 'chromedriver.exe',
            'chromedriver_archive_name': 'chromedriver_win32.zip',
        },
        'win-arm64': {
            'binary_name': 'chrome.exe',
            'listing_platform_dir': 'Win_Arm64/',
            'archive_name': 'chrome-win.zip',
            'archive_extract_dir': 'chrome-win',
            'chromedriver_binary_name': 'chromedriver.exe',
            'chromedriver_archive_name': 'chromedriver_win64.zip',
        },
    },
    'asan': {
        'linux': {},
        'mac': {},
        'win': {},
    },
}

CHROME_APK_FILENAMES = {
    'chrome': 'Chrome.apk',
    'chrome_beta': 'ChromeBeta.apk',
    'chrome_canary': 'ChromeCanary.apk',
    'chrome_dev': 'ChromeDev.apk',
    'chrome_stable': 'ChromeStable.apk',
    'chromium': 'ChromePublic.apk',
}

CHROME_MODERN_APK_FILENAMES = {
    'chrome': 'ChromeModern.apk',
    'chrome_beta': 'ChromeModernBeta.apk',
    'chrome_canary': 'ChromeModernCanary.apk',
    'chrome_dev': 'ChromeModernDev.apk',
    'chrome_stable': 'ChromeModernStable.apk',
    'chromium': 'ChromePublic.apk',
}

MONOCHROME_APK_FILENAMES = {
    'chrome': 'Monochrome.apk',
    'chrome_beta': 'MonochromeBeta.apk',
    'chrome_canary': 'MonochromeCanary.apk',
    'chrome_dev': 'MonochromeDev.apk',
    'chrome_stable': 'MonochromeStable.apk',
    'chromium': 'ChromePublic.apk',
}

TRICHROME_APK_FILENAMES = {
    'chrome': 'TrichromeChromeGoogle.apks',
    'chrome_beta': 'TrichromeChromeGoogleBeta.apks',
    'chrome_canary': 'TrichromeChromeGoogleCanary.apks',
    'chrome_dev': 'TrichromeChromeGoogleDev.apks',
    'chrome_stable': 'TrichromeChromeGoogleStable.apks',
}

TRICHROME64_APK_FILENAMES = {
    'chrome': 'TrichromeChromeGoogle6432.apks',
    'chrome_beta': 'TrichromeChromeGoogle6432Beta.apks',
    'chrome_canary': 'TrichromeChromeGoogle6432Canary.apks',
    'chrome_dev': 'TrichromeChromeGoogle6432Dev.apks',
    'chrome_stable': 'TrichromeChromeGoogle6432Stable.apks',
}

TRICHROME_LIBRARY_FILENAMES = {
    'chrome': 'TrichromeLibraryGoogle.apk',
    'chrome_beta': 'TrichromeLibraryGoogleBeta.apk',
    'chrome_canary': 'TrichromeLibraryGoogleCanary.apk',
    'chrome_dev': 'TrichromeLibraryGoogleDev.apk',
    'chrome_stable': 'TrichromeLibraryGoogleStable.apk',
}

TRICHROME64_LIBRARY_FILENAMES = {
    'chrome': 'TrichromeLibraryGoogle6432.apk',
    'chrome_beta': 'TrichromeLibraryGoogle6432Beta.apk',
    'chrome_canary': 'TrichromeLibraryGoogle6432Canary.apk',
    'chrome_dev': 'TrichromeLibraryGoogle6432Dev.apk',
    'chrome_stable': 'TrichromeLibraryGoogle6432Stable.apk',
}

WEBVIEW_APK_FILENAMES = {
    # clank release
    'android_webview': 'AndroidWebview.apk',
    # clank official
    'system_webview_google': 'SystemWebViewGoogle.apk',
    # upstream
    'system_webview': 'SystemWebView.apk',
}

# Old storage locations for per CL builds
OFFICIAL_BACKUP_BUILDS = {
    'android-arm': {
        'listing_platform_dir': ['Android Builder/'],
    },
    'linux64': {
        'listing_platform_dir': ['Linux Builder Perf/'],
    },
    'mac': {
        'listing_platform_dir': ['Mac Builder Perf/'],
    },
    'win64': {
        'listing_platform_dir': ['Win x64 Builder Perf/'],
    }
}

PLATFORM_ARCH_TO_ARCHIVE_MAPPING = {
    ('linux', 'x64'): 'linux64',
    ('mac', 'x64'): 'mac64',
    ('mac', 'x86'): 'mac',
    ('mac', 'arm'): 'mac-arm',
    ('win', 'x64'): 'win64',
    ('win', 'x86'): 'win',
    ('win', 'arm'): 'win-arm64',
}

# Set only during initialization.
is_verbose = False


class BisectException(Exception):

  def __str__(self):
    return '[Bisect Exception]: %s\n' % self.args[0]


def RunGsutilCommand(args, can_fail=False, ignore_fail=False):
  if not GSUTILS_PATH:
    raise BisectException('gsutils is not found in path.')
  if is_verbose:
    print('Running gsutil command: ' +
          str([sys.executable, GSUTILS_PATH] + args))
  gsutil = subprocess.Popen([sys.executable, GSUTILS_PATH] + args,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            env=None)
  stdout_b, stderr_b = gsutil.communicate()
  stdout = stdout_b.decode("utf-8")
  stderr = stderr_b.decode("utf-8")
  if gsutil.returncode:
    if (re.findall(r'(status|ServiceException:)[ |=]40[1|3]', stderr)
        or stderr.startswith(CREDENTIAL_ERROR_MESSAGE)):
      print(('Follow these steps to configure your credentials and try'
             ' running the bisect-builds.py again.:\n'
             '  1. Run "python3 %s config" and follow its instructions.\n'
             '  2. If you have a @google.com account, use that account.\n'
             '  3. For the project-id, just enter 0.' % GSUTILS_PATH))
      print('Warning: You might have an outdated .boto file. If this issue '
            'persists after running `gsutil.py config`, try removing your '
            '.boto, usually located in your home directory.')
      raise BisectException('gsutil credential error')
    elif can_fail:
      return stderr
    elif ignore_fail:
      return stdout
    else:
      raise Exception('Error running the gsutil command:\n%s\n%s' %
                      (args, stderr))
  return stdout


def GsutilList(*urls, ignore_fail=False):
  """List GCloud Storage with URLs and return a list of paths.

  This method lists all archive builds in a GCS bucket; it filters out invalid
  archive builds or files.

  Arguments:
    * urls - one or more gs:// URLs
    * ignore_fail - ignore gsutil command errors, e.g., 'matched no objects'

  Return:
    * list of paths that match the given URLs
  """
  # Get a directory listing with file sizes. Typical output looks like:
  #         7  2023-11-27T21:08:36Z  gs://.../LAST_CHANGE
  # 144486938  2023-03-07T14:41:25Z  gs://.../full-build-win32_1113893.zip
  # TOTAL: 114167 objects, 15913845813421 bytes (14.47 TiB)
  # This lets us ignore empty .zip files that will otherwise cause errors.
  stdout = RunGsutilCommand(['ls', '-l', *urls], ignore_fail=ignore_fail)
  # Trim off the summary line that only happens with -l
  lines = []
  for line in stdout.splitlines():
    parts = line.split(maxsplit=2)
    if not parts[-1].startswith('gs://'):
      continue
    # Check whether there is a size field. For release builds the listing
    # will be directories so there will be no size field.
    if len(parts) > 1:
      if ANDROID_INVALID_BUCKET in line:
        continue
      size = int(parts[0])
      # Empty .zip files are 22 bytes. Ignore anything less than 1,000 bytes,
      # but keep the LAST_CHANGE file since the code seems to expect that.
      if parts[-1].endswith('LAST_CHANGE') or size > 1000:
        lines.append(parts[-1])
    else:
      lines.append(parts[-1])
  return lines


class ArchiveBuild(abc.ABC):
  """Base class for a archived build."""

  def __init__(self, options):
    self.platform = options.archive
    self.good_revision = options.good
    self.bad_revision = options.bad
    self.use_local_cache = options.use_local_cache
    self.chromedriver = options.chromedriver
    # PATH_CONTEXT
    path_context = PATH_CONTEXT[self.build_type].get(self.platform, {})
    self.binary_name = path_context.get('binary_name')
    self.listing_platform_dir = path_context.get('listing_platform_dir')
    self.archive_name = path_context.get('archive_name')
    self.archive_extract_dir = path_context.get('archive_extract_dir')
    self.chromedriver_binary_name = path_context.get('chromedriver_binary_name')
    self.chromedriver_archive_name = path_context.get(
        'chromedriver_archive_name')
    if self.chromedriver and not self.chromedriver_binary_name:
      raise BisectException(
          'Could not find chromedriver_binary_name, '
          f'--chromedriver might not supported on {self.platform}.')
    # run_revision options
    self.profile = options.profile
    self.command = options.command
    self.num_runs = options.times

  @property
  @abc.abstractmethod
  def build_type(self):
    raise NotImplemented()

  @abc.abstractmethod
  def _get_rev_list(self, min_rev=None, max_rev=None):
    """The actual method to get revision list without cache.

    min_rev and max_rev could be None, indicating that the method should return
    all revisions.

    The method should return at least the revision list that exists for the
    given (min_rev, max_rev) range. However, it could return a revision list
    with revisions beyond min_rev and max_rev based on the implementation for
    better caching. The rev_list should contain all available revisions between
    the returned minimum and maximum values.

    The return value of revisions in the list should match the type of
    good_revision and bad_revision, and should be comparable.
    """
    raise NotImplemented()

  @property
  def _rev_list_cache_filename(self):
    return os.path.join(os.path.abspath(os.path.dirname(__file__)),
                        '.bisect-builds-cache.json')

  @property
  @abc.abstractmethod
  def _rev_list_cache_key(self):
    """Returns the cache key for archive build. The cache key should be able to
    distinguish like build_type, platform."""
    raise NotImplemented()

  def _load_rev_list_cache(self):
    if not self.use_local_cache:
      return []
    cache_filename = self._rev_list_cache_filename
    try:
      with open(cache_filename) as cache_file:
        cache = json.load(cache_file)
        revisions = cache.get(self._rev_list_cache_key, [])
        if revisions:
          print('Loaded revisions %s-%s from %s' %
                (revisions[0], revisions[-1], cache_filename))
        return revisions
    except FileNotFoundError:
      return []
    except (EnvironmentError, ValueError) as e:
      print('Load revisions cache error:', e)
      return []

  def _save_rev_list_cache(self, revisions):
    if not self.use_local_cache:
      return
    if not revisions:
      return
    cache = {}
    cache_filename = self._rev_list_cache_filename
    # Load cache for all of the builds.
    try:
      with open(cache_filename) as cache_file:
        cache = json.load(cache_file)
    except FileNotFoundError:
      pass
    except (EnvironmentError, ValueError) as e:
      print('Load existing revisions cache error:', e)
      return
    # Update and save cache for current build.
    cache[self._rev_list_cache_key] = revisions
    try:
      with open(cache_filename, 'w') as cache_file:
        json.dump(cache, cache_file)
      print('Saved revisions %s-%s to %s' %
            (revisions[0], revisions[-1], cache_filename))
    except EnvironmentError as e:
      print('Save revisions cache error:', e)
      return

  def get_rev_list(self):
    """Gets the list of revision numbers between self.good_revision and
    self.bad_revision. The result might be cached when use_local_cache."""
    # Download the rev_list_all
    min_rev, max_rev = sorted((self.good_revision, self.bad_revision))
    rev_list_all = self._load_rev_list_cache()
    if not rev_list_all:
      rev_list_all = sorted(self._get_rev_list(min_rev, max_rev))
      self._save_rev_list_cache(rev_list_all)
    else:
      rev_list_min, rev_list_max = rev_list_all[0], rev_list_all[-1]
      if min_rev < rev_list_min or max_rev > rev_list_max:
        # We only need to request and merge the rev_list beyond the cache.
        rev_list_requested = self._get_rev_list(
            min_rev if min_rev < rev_list_min else rev_list_max,
            max_rev if max_rev > rev_list_max else rev_list_min)
        rev_list_all = sorted(set().union(rev_list_all, rev_list_requested))
        self._save_rev_list_cache(rev_list_all)
    # If we still don't get a rev_list_all for the given range, adjust the
    # range to get the full revision list for better messaging.
    if not rev_list_all:
      rev_list_all = sorted(self._get_rev_list())
      self._save_rev_list_cache(rev_list_all)
    if not rev_list_all:
      raise BisectException('Could not retrieve the revisions for %s.' %
                            self.platform)

    # Filter for just the range between good and bad.
    rev_list = [x for x in rev_list_all if min_rev <= x <= max_rev]
    # Don't have enough builds to bisect.
    if len(rev_list) < 2:
      rev_list_min, rev_list_max = rev_list_all[0], rev_list_all[-1]
      # Check for specifying a number before the available range.
      if max_rev < rev_list_min:
        msg = (
            'First available bisect revision for %s is %d. Be sure to specify '
            'revision numbers, not branch numbers.' %
            (self.platform, rev_list_min))
        raise BisectException(msg)
      # Check for specifying a number beyond the available range.
      if min_rev > rev_list_max:
        # Check for the special case of linux where bisect builds stopped at
        # revision 382086, around March 2016.
        if self.platform == 'linux':
          msg = ('Last available bisect revision for %s is %d. Try linux64 '
                 'instead.' % (self.platform, rev_list_max))
        else:
          msg = ('Last available bisect revision for %s is %d. Try a different '
                 'good/bad range.' % (self.platform, rev_list_max))
        raise BisectException(msg)
      # Otherwise give a generic message.
      msg = 'We don\'t have enough builds to bisect. rev_list: %s' % rev_list
      raise BisectException(msg)

    # Set good and bad revisions to be legit revisions.
    if rev_list:
      if self.good_revision < self.bad_revision:
        self.good_revision = rev_list[0]
        self.bad_revision = rev_list[-1]
      else:
        self.bad_revision = rev_list[0]
        self.good_revision = rev_list[-1]
    return rev_list

  @abc.abstractmethod
  def get_download_url(self, revision):
    """Gets the download URL for the specific revision."""
    raise NotImplemented()

  def get_download_job(self, revision, name=None):
    """Gets as a DownloadJob that download the specific revision in threads."""
    return DownloadJob(self.get_download_url(revision), revision, name)

  def _get_extra_args(self):
    """Get extra chrome args"""
    return ['--user-data-dir=%s' % self.profile]

  def _get_extract_binary_glob(self, tempdir):
    """Get the pathname for extracted chrome binary"""
    return '%s/*/%s' % (tempdir, self.binary_name)

  def _get_chromedriver_binary_glob(self, tempdir):
    """Get the pathname for extracted chromedriver binary"""
    if not self.chromedriver_binary_name:
      raise BisectException(f"chromedriver is not supported on {self.platform}")
    return '%s/*/%s' % (tempdir, self.chromedriver_binary_name)

  def _run(self, runcommand, cwd=None, shell=False):
    # is_verbos is a global variable.
    if is_verbose:
      print(('Running ' + str(runcommand)))
    subproc = subprocess.Popen(runcommand,
                               cwd=cwd,
                               shell=shell,
                               bufsize=-1,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    (stdout, stderr) = subproc.communicate()
    if is_verbose:
      print(f'retcode:{subproc.returncode}\nstdout:\n')
      sys.stdout.buffer.write(stdout)
      sys.stdout.flush()
      print('stderr:\n')
      sys.stderr.buffer.write(stderr)
      sys.stderr.flush()
    return subproc.returncode, stdout, stderr

  @staticmethod
  def _glob_with_unique_match(executable_name, tempdir, pathname):
    executables = glob.glob(pathname)
    if len(executables) == 0:
      raise BisectException(
          f'Can not find the {executable_name} binary from {tempdir}')
    elif len(executables) > 1:
      raise BisectException(
          f'Multiple {executable_name} executables found: {executables}')
    return os.path.abspath(executables[0])

  def _install_revision(self, download, tempdir):
    """Unzip and/or install the given download to tempdir. Return executable
    binaries in a dict."""
    if isinstance(download, dict):
      for each in download.values():
        UnzipFilenameToDir(each, tempdir)
    else:
      UnzipFilenameToDir(download, tempdir)
    # Searching for the executable, it's unlikely the zip file contains multiple
    # folders with the binary_name.
    result = {}
    result['chrome'] = self._glob_with_unique_match(
        'chrome', tempdir, self._get_extract_binary_glob(tempdir))
    if self.chromedriver:
      result['chromedriver'] = self._glob_with_unique_match(
          'chromedriver', tempdir, self._get_chromedriver_binary_glob(tempdir))
    return result

  def _launch_revision(self, tempdir, executables, args=()):
    args = [*self._get_extra_args(), *args]
    args_str = shlex.join(args)
    command = (self.command.replace(r'%p', executables['chrome']).replace(
        r'%s', args_str).replace(r'%a', args_str).replace(r'%t', tempdir))
    if self.chromedriver:
      command = command.replace(r'%d', executables['chromedriver'])
    return self._run(command, shell=True)

  def run_revision(self, download, tempdir, args=()):
    """Run downloaded archive"""
    executables = self._install_revision(download, tempdir)
    result = None
    for _ in range(self.num_runs):
      returncode, _, _ = result = self._launch_revision(tempdir, executables,
                                                        args)
      if returncode:
        break
    return result


class LooseVersion(BaseLooseVersion):

  def __hash__(self):
    return hash(str(self))


class ReleaseBuild(ArchiveBuild):

  def __init__(self, options):
    super().__init__(options)
    self.good_revision = LooseVersion(self.good_revision)
    self.bad_revision = LooseVersion(self.bad_revision)

  @property
  def build_type(self):
    return 'release'

  def _get_release_bucket(self):
    return RELEASE_BASE_URL

  def _get_rev_list(self, min_rev=None, max_rev=None):
    # Get all build numbers in the build bucket.
    build_numbers = []
    revision_re = re.compile(r'(\d+\.\d\.\d{4}\.\d+)')
    for path in GsutilList(self._get_release_bucket()):
      match = revision_re.search(path)
      if match:
        build_numbers.append(LooseVersion(match[1]))
    # Filter the versions between min_rev and max_rev.
    build_numbers = [
        x for x in build_numbers
        if (not min_rev or min_rev <= x) and (not max_rev or x <= max_rev)
    ]
    # Check if target archive build exists in batches.
    # batch size is limited by maximum length for the command line. Which is
    # 32,768 characters on Windows, which should be enough up to 400 files.
    batch_size = 100
    final_list = []
    for batch in (build_numbers[i:i + batch_size]
                  for i in range(0, len(build_numbers), batch_size)):
      sys.stdout.write('\rFetching revisions at marker %s' % batch[0])
      sys.stdout.flush()
      # List the files that exists with listing_platform_dir and archive_name.
      # Gsutil could fail because some of the path not exists. It's safe to
      # ignore them.
      for path in GsutilList(*[self._get_archive_path(x) for x in batch],
                             ignore_fail=True):
        match = revision_re.search(path)
        if match:
          final_list.append(LooseVersion(match[1]))
    sys.stdout.write('\r')
    sys.stdout.flush()
    return final_list

  def _get_listing_url(self):
    return self._get_release_bucket()

  def _get_archive_path(self, build_number, archive_name=None):
    if archive_name is None:
      archive_name = self.archive_name
    return '/'.join((self._get_release_bucket(), str(build_number),
                     self.listing_platform_dir.rstrip('/'), archive_name))

  @property
  def _rev_list_cache_key(self):
    return self._get_archive_path('**')

  def _save_rev_list_cache(self, revisions):
    # LooseVersion is not json-able, convert it back to string format.
    super()._save_rev_list_cache([str(x) for x in revisions])

  def _load_rev_list_cache(self):
    # Convert to LooseVersion that revisions can be correctly compared.
    revisions = super()._load_rev_list_cache()
    return [LooseVersion(x) for x in revisions]

  def get_download_url(self, revision):
    if self.chromedriver:
      return {
          'chrome':
          self._get_archive_path(revision),
          'chromedriver':
          self._get_archive_path(revision, self.chromedriver_archive_name),
      }
    return self._get_archive_path(revision)


class ArchiveBuildWithCommitPosition(ArchiveBuild):
  """Class for ArchiveBuilds that organized based on commit position."""

  def get_last_change_url(self):
    return None

  def __init__(self, options):
    super().__init__(options)
    # convert good and bad to commit position as int.
    self.good_revision = GetRevision(self.good_revision)
    if not options.bad:
      self.bad_revision = GetChromiumRevision(self.get_last_change_url())
    self.bad_revision = GetRevision(self.bad_revision)


class OfficialBuild(ArchiveBuildWithCommitPosition):

  @property
  def build_type(self):
    return 'official'

  def _get_listing_url(self):
    return '/'.join((PERF_BASE_URL, self.listing_platform_dir))

  def _get_rev_list(self, min_rev=None, max_rev=None):
    # For official builds, it's getting the list from perf build bucket.
    # Since it's cheap to get full list, we are returning the full list for
    # caching.
    revision_re = re.compile(r'%s_(\d+)\.zip' % (self.archive_extract_dir))
    revision_files = GsutilList(self._get_listing_url())
    revision_numbers = []
    for revision_file in revision_files:
      revision_num = revision_re.search(revision_file)
      if revision_num:
        revision_numbers.append(int(revision_num[1]))
    return revision_numbers

  @property
  def _rev_list_cache_key(self):
    return self._get_listing_url()

  def get_download_url(self, revision):
    return '%s/%s%s_%s.zip' % (PERF_BASE_URL, self.listing_platform_dir,
                               self.archive_extract_dir, revision)


class SnapshotBuild(ArchiveBuildWithCommitPosition):

  def __init__(self, options):
    self.base_url = CHROMIUM_BASE_URL
    super().__init__(options)

  @property
  def build_type(self):
    return 'snapshot'

  def _get_marker_for_revision(self, revision):
    return '%s%d' % (self.listing_platform_dir, revision)

  def _fetch_and_parse(self, url):
    """Fetches a URL and returns a 2-Tuple of ([revisions], next-marker). If
    next-marker is not None, then the listing is a partial listing and another
    fetch should be performed with next-marker being the marker= GET
    parameter."""
    handle = urllib.request.urlopen(url)
    document = ElementTree.parse(handle)
    # All nodes in the tree are namespaced. Get the root's tag name to extract
    # the namespace. Etree does namespaces as |{namespace}tag|.
    root_tag = document.getroot().tag
    end_ns_pos = root_tag.find('}')
    if end_ns_pos == -1:
      raise Exception('Could not locate end namespace for directory index')
    namespace = root_tag[:end_ns_pos + 1]
    # Find the prefix (_listing_platform_dir) and whether or not the list is
    # truncated.
    prefix_len = len(document.find(namespace + 'Prefix').text)
    next_marker = None
    is_truncated = document.find(namespace + 'IsTruncated')
    if is_truncated is not None and is_truncated.text.lower() == 'true':
      next_marker = document.find(namespace + 'NextMarker').text
    # Get a list of all the revisions.
    revisions = []
    revision_re = re.compile(r'(\d+)')
    all_prefixes = document.findall(namespace + 'CommonPrefixes/' + namespace +
                                    'Prefix')
    # The <Prefix> nodes have content of the form of
    # |_listing_platform_dir/revision/|. Strip off the platform dir and the
    # trailing slash to just have a number.go
    for prefix in all_prefixes:
      match = revision_re.search(prefix.text[prefix_len:])
      if match:
        revisions.append(int(match[1]))
    return revisions, next_marker

  def get_last_change_url(self):
    """Returns a URL to the LAST_CHANGE file."""
    return self.base_url + '/' + self.listing_platform_dir + 'LAST_CHANGE'

  def _get_rev_list(self, min_rev=None, max_rev=None):
    # This method works by parsing the Google Storage directory listing into a
    # list of revision numbers. This method can return a full revision list for
    # a full scan.
    if not max_rev:
      max_rev = GetChromiumRevision(self.get_last_change_url())
    # The commondatastorage API listing the files by alphabetical order instead
    # of numerical order (e.g. 1, 10, 2, 3, 4). That starting or breaking the
    # pagination from a known position is only valid when the number of digits
    # of min_rev == max_rev.
    start_marker = None
    next_marker = None
    if min_rev is not None and max_rev is not None and len(str(min_rev)) == len(
        str(max_rev)):
      start_marker = next_marker = self._get_marker_for_revision(min_rev)
    else:
      max_rev = None

    revisions = []
    while True:
      sys.stdout.write('\rFetching revisions at marker %s' % next_marker)
      sys.stdout.flush()
      new_revisions, next_marker = self._fetch_and_parse(
          self._get_listing_url(next_marker))
      revisions.extend(new_revisions)
      if max_rev and new_revisions and max_rev <= max(new_revisions):
        break
      if not next_marker:
        break
    sys.stdout.write('\r')
    sys.stdout.flush()
    # We can only ensure the revisions have no gap (due to the alphabetical
    # order) between min_rev and max_rev.
    if start_marker or next_marker:
      return [
          x for x in revisions if ((min_rev is None or min_rev <= x) and (
              max_rev is None or x <= max_rev))
      ]
    # Unless we did a full scan. `not start_marker and not next_marker`
    else:
      return revisions

  def _get_listing_url(self, marker=None):
    """Returns the URL for a directory listing, with an optional marker."""
    marker_param = ''
    if marker:
      marker_param = '&marker=' + str(marker)
    return (self.base_url + '/?delimiter=/&prefix=' +
            self.listing_platform_dir + marker_param)

  @property
  def _rev_list_cache_key(self):
    return self._get_listing_url()

  def get_download_url(self, revision):
    archive_name = self.archive_name
    # `archive_name` was changed for chromeos, win and win64 at revision 591483
    # This is patched for backward compatibility.
    if revision < 591483:
      if self.platform == 'chromeos':
        archive_name = 'chrome-linux.zip'
      elif self.platform in ('win', 'win64'):
        archive_name = 'chrome-win32.zip'
    url_prefix = '%s/%s%s/' % (self.base_url, self.listing_platform_dir,
                               revision)
    chrome_url = url_prefix + archive_name
    if self.chromedriver:
      return {
          'chrome': chrome_url,
          'chromedriver': url_prefix + self.chromedriver_archive_name,
      }
    return chrome_url


class ASANBuild(SnapshotBuild):
  """ASANBuilds works like SnapshotBuild which fetch from commondatastorage, but
  with a different listing url."""

  def __init__(self, options):
    super().__init__(options)
    self.base_url = ASAN_BASE_URL
    self.asan_build_type = 'release'

  @property
  def build_type(self):
    return 'asan'

  def GetASANPlatformDir(self):
    """ASAN builds are in directories like "linux-release", or have filenames
    like "asan-win32-release-277079.zip". This aligns to our platform names
    except in the case of Windows where they use "win32" instead of "win"."""
    if self.platform == 'win':
      return 'win32'
    else:
      return self.platform

  def GetASANBaseName(self):
    """Returns the base name of the ASAN zip file."""
    # TODO: These files were not update since 2016 for linux, 2021 for win.
    # Need to confirm if it's moved.
    if 'linux' in self.platform:
      return 'asan-symbolized-%s-%s' % (self.GetASANPlatformDir(),
                                        self.asan_build_type)
    else:
      return 'asan-%s-%s' % (self.GetASANPlatformDir(), self.asan_build_type)

  def get_last_change_url(self):
    # LAST_CHANGE is not supported in asan build.
    return None

  def _get_listing_url(self, marker=None):
    """Returns the URL for a directory listing, with an optional marker."""
    marker_param = ''
    if marker:
      marker_param = '&marker=' + str(marker)
    prefix = '%s-%s/%s' % (self.GetASANPlatformDir(), self.asan_build_type,
                           self.GetASANBaseName())
    # This is a hack for delimiter to make commondata API return file path as
    # prefix that can reuse the code of SnapshotBuild._fetch_and_parse.
    return self.base_url + '/?delimiter=.zip&prefix=' + prefix + marker_param

  def _get_marker_for_revision(self, revision):
    # The build type is hardcoded as release in the original code.
    return '%s-%s/%s-%d.zip' % (self.GetASANPlatformDir(), self.asan_build_type,
                                self.GetASANBaseName(), revision)

  def get_download_url(self, revision):
    return '%s/%s' % (self.base_url, self._get_marker_for_revision(revision))


class AndroidBuildMixin:

  def __init__(self, options):
    super().__init__(options)
    self.apk = options.apk
    self.device = InitializeAndroidDevice(options.device_id, self.apk, None)
    self.flag_changer = None
    if not self.device:
      raise BisectException('Failed to initialize device.')
    self.binary_name = self._get_apk_filename()

  def _get_apk_filename(self, prefer_64bit=True):
    sdk = self.device.build_version_sdk
    apk_mapping = None
    if 'webview' in self.apk.lower():
      apk_mapping = WEBVIEW_APK_FILENAMES
    # Need these logic to bisect very old build. Release binaries are stored
    # forever and occasionally there are requests to bisect issues introduced
    # in very old versions.
    elif sdk < version_codes.LOLLIPOP:
      apk_mapping = CHROME_APK_FILENAMES
    elif sdk < version_codes.NOUGAT:
      apk_mapping = CHROME_MODERN_APK_FILENAMES
    else:
      apk_mapping = MONOCHROME_APK_FILENAMES
    if self.apk not in apk_mapping:
      raise BisectException(
          'Bisecting on Android only supported for these apks: [%s].' %
          '|'.join(apk_mapping))
    return apk_mapping[self.apk]

  def _install_revision(self, download, tempdir):
    apk_paths = super()._install_revision(download, tempdir)
    InstallOnAndroid(self.device, apk_paths['chrome'])

  def _launch_revision(self, tempdir, executables, args=()):
    if args:
      if self.apk not in chrome.PACKAGE_INFO:
        raise BisectException(
            f'Launching args are not supported for {self.apk}')
      if not self.flag_changer:
        self.flag_changer = flag_changer.FlagChanger(
            self.device, chrome.PACKAGE_INFO[self.apk].cmdline_file)
      self.flag_changer.ReplaceFlags(args)
    LaunchOnAndroid(self.device, self.apk)
    return (0, sys.stdout, sys.stderr)

  def _get_extract_binary_glob(self, tempdir):
    return '%s/*/apks/%s' % (tempdir, self.binary_name)


class AndroidTrichromeMixin(AndroidBuildMixin):

  def __init__(self, options):
    self._64bit_platforms = ('android-arm64', 'android-x64',
                             'android-arm64-high')
    super().__init__(options)
    if self.device.build_version_sdk < version_codes.Q:
      raise BisectException("Trichrome is only supported after Android Q.")
    self.library_binary_name = self._get_library_filename()

  def _get_apk_filename(self, prefer_64bit=True):
    if self.platform in self._64bit_platforms and prefer_64bit:
      apk_mapping = TRICHROME64_APK_FILENAMES
    else:
      apk_mapping = TRICHROME_APK_FILENAMES
    if self.apk not in apk_mapping:
      raise BisectException(
          'Bisecting on Android only supported for these apks: [%s].' %
          '|'.join(apk_mapping))
    return apk_mapping[self.apk]

  def _get_library_filename(self, prefer_64bit=True):
    apk_mapping = None
    if self.platform in self._64bit_platforms and prefer_64bit:
      apk_mapping = TRICHROME64_LIBRARY_FILENAMES
    else:
      apk_mapping = TRICHROME_LIBRARY_FILENAMES
    if self.apk not in apk_mapping:
      raise BisectException(
          'Bisecting for Android Trichrome only supported for these apks: [%s].'
          % '|'.join(apk_mapping))
    return apk_mapping[self.apk]


class AndroidReleaseBuild(AndroidBuildMixin, ReleaseBuild):

  def __init__(self, options):
    super().__init__(options)
    self.signed = options.signed
    # We could download the apk directly from build bucket
    self.archive_name = self.binary_name

  def _get_release_bucket(self):
    if self.signed:
      return ANDROID_RELEASE_BASE_URL_SIGNED
    else:
      return ANDROID_RELEASE_BASE_URL

  def _get_rev_list(self, min_rev=None, max_rev=None):
    # Android release builds store archives directly in a GCS bucket that
    # contains a large number of objects. Listing the full revision list takes
    # too much time, so we should disallow it and fail fast.
    if not min_rev or not max_rev:
      raise BisectException(
          "Could not found enough revisions for Android %s release channel." %
          self.apk)
    return super()._get_rev_list(min_rev, max_rev)

  def _install_revision(self, download, tempdir):
    # AndroidRelease build downloads the apks directly from GCS bucket.
    InstallOnAndroid(self.device, download)


class AndroidTrichromeReleaseBuild(AndroidTrichromeMixin, AndroidReleaseBuild):

  def __init__(self, options):
    super().__init__(options)
    # Release build will download the binary directly from GCS bucket.
    self.archive_name = self.binary_name
    self.library_archive_name = self.library_binary_name

  def _get_library_filename(self, prefer_64bit=True):
    if self.apk == 'chrome' and self.platform == 'android-arm64-high':
      raise BisectException('chrome debug build is not supported for %s' %
                            self.platform)
    return super()._get_library_filename(prefer_64bit)

  def get_download_url(self, revision):
    # M112 is when we started serving 6432 to 4GB+ devices. Before this it was
    # only to 6GB+ devices.
    if revision >= LooseVersion('112'):
      trichrome = self.binary_name
      trichrome_library = self.library_binary_name
    else:
      trichrome = self._get_apk_filename(prefer_64bit=False)
      trichrome_library = self._get_library_filename(prefer_64bit=False)
    return {
        'trichrome': self._get_archive_path(revision, trichrome),
        'trichrome_library': self._get_archive_path(revision,
                                                    trichrome_library),
    }

  def _install_revision(self, download, tempdir):
    if not isinstance(download, dict):
      raise Exception("Trichrome should download multiple files from GCS.")
    # AndroidRelease build downloads the apks directly from GCS bucket.
    # Trichrome need to install the trichrome_library first.
    InstallOnAndroid(self.device, download['trichrome_library'])
    InstallOnAndroid(self.device, download['trichrome'])


class AndroidTrichromeOfficialBuild(AndroidTrichromeMixin, OfficialBuild):

  def _get_apk_filename(self, prefer_64bit=True):
    filename = super()._get_apk_filename(prefer_64bit)
    return filename.replace(".apks", ".minimal.apks")

  def _install_revision(self, download, tempdir):
    UnzipFilenameToDir(download, tempdir)
    trichrome_library_filename = self._get_library_filename()
    trichrome_library_path = glob.glob(
        f'{tempdir}/*/apks/{trichrome_library_filename}')
    if len(trichrome_library_path) == 0:
      raise Exception(
          f'Can not find {trichrome_library_filename} from {tempdir}')
    trichrome_filename = self._get_apk_filename()
    trichrome_path = glob.glob(f'{tempdir}/*/apks/{trichrome_filename}')
    if len(trichrome_path) == 0:
      raise Exception(f'Can not find {trichrome_filename} from {tempdir}')
    InstallOnAndroid(self.device, trichrome_library_path[0])
    InstallOnAndroid(self.device, trichrome_path[0])


class LinuxReleaseBuild(ReleaseBuild):

  def _get_extra_args(self):
    args = super()._get_extra_args()
    # The sandbox must be run as root on release Chrome, so bypass it.
    if self.platform.startswith('linux'):
      args.append('--no-sandbox')
    return args


class AndroidOfficialBuild(AndroidBuildMixin, OfficialBuild):
  pass


class AndroidSnapshotBuild(AndroidBuildMixin, SnapshotBuild):
  pass


class IOSReleaseBuild(ReleaseBuild):

  def __init__(self, options):
    super().__init__(options)
    self.signed = options.signed
    if not self.signed:
      print('WARNING: --signed is recommended for iOS release builds.')
    self.device_id = options.device_id
    if not self.device_id:
      raise BisectException('--device-id is required for iOS builds.')
    self.ipa = options.ipa
    if not self.ipa:
      raise BisectException('--ipa is required for iOS builds.')
    if self.ipa.endswith('.ipa'):
      self.ipa = self.ipa[:-4]
    self.binary_name = self.archive_name = f'{self.ipa}.ipa'

  def _get_release_bucket(self):
    if self.signed:
      return IOS_RELEASE_BASE_URL_SIGNED
    return IOS_RELEASE_BASE_URL

  def _get_archive_path(self, build_number, archive_name=None):
    if archive_name is None:
      archive_name = self.archive_name
    # The format for iOS build is
    # {IOS_RELEASE_BASE_URL}/{build_number}/{sdk_version}
    # /{builder_name}/{build_number}/{archive_name}
    # that it's not possible to generate the actual archive_path for a build.
    # That we are returning a path with wildcards and expecting only one match.
    return (f'{self._get_release_bucket()}/{build_number}/*/'
            f'{self.listing_platform_dir.rstrip("/")}/*/{archive_name}')

  def _install_revision(self, download, tempdir):
    # install ipa
    retcode, stdout, stderr = self._run([
        'xcrun', 'devicectl', 'device', 'install', 'app', '--device',
        self.device_id, download
    ])
    if retcode:
      raise BisectException(f'Install app error, code:{retcode}\n'
                            f'stdout:\n{stdout}\n'
                            f'stderr:\n{stderr}')
    # extract and return CFBundleIdentifier from ipa.
    UnzipFilenameToDir(download, tempdir)
    plist = glob.glob(f'{tempdir}/Payload/*/Info.plist')
    if not plist:
      raise BisectException(f'Could not find Info.plist from {tempdir}.')
    retcode, stdout, stderr = self._run(
        ['plutil', '-extract', 'CFBundleIdentifier', 'raw', plist[0]])
    if retcode:
      raise BisectException(f'Extract bundle identifier error, code:{retcode}\n'
                            f'stdout:\n{stdout}\n'
                            f'stderr:\n{stderr}')
    bundle_identifier = stdout.strip()
    return bundle_identifier

  def _launch_revision(self, tempdir, bundle_identifier, args=()):
    retcode, stdout, stderr = self._run([
        'xcrun', 'devicectl', 'device', 'process', 'launch', '--device',
        self.device_id, bundle_identifier, *args
    ])
    if retcode:
      print(f'Warning: App launching error, code:{retcode}\n'
            f'stdout:\n{stdout}\n'
            f'stderr:\n{stderr}')
    return retcode, stdout, stderr


class IOSSimulatorReleaseBuild(ReleaseBuild):
  """
  chrome/ci/ios-simulator is generating this build and archiving it in
  gs://bling-archive with Chrome versions. It's not actually a release build,
  but it's similar to one.
  """

  def __init__(self, options):
    super().__init__(options)
    self.device_id = options.device_id
    if not self.device_id:
      raise BisectException('--device-id is required for iOS Simulator.')

  def _get_release_bucket(self):
    return IOS_ARCHIVE_BASE_URL

  def _get_archive_path(self, build_number, archive_name=None):
    if archive_name is None:
      archive_name = self.archive_name
    # The path format for ios-simulator build is
    # {%chromium_version%}/{%timestamp%}/Chromium.tar.gz
    # that it's not possible to generate the actual archive_path for a build.
    # We are returning a path with wildcards and expecting only one match.
    return f'{self._get_release_bucket()}/{build_number}/*/{archive_name}'

  def _get_extract_binary_glob(self, tempdir):
    return f'{tempdir}/{self.binary_name}'

  def _install_revision(self, download, tempdir):
    executables = super()._install_revision(download, tempdir)
    executable = executables['chrome']
    # install app
    retcode, stdout, stderr = self._run(
        ['xcrun', 'simctl', 'install', self.device_id, executable])
    if retcode:
      raise BisectException(f'Install app error, code:{retcode}\n'
                            f'stdout:\n{stdout}\n'
                            f'stderr:\n{stderr}')
    # extract and return CFBundleIdentifier from app.
    plist = glob.glob(f'{executable}/Info.plist')
    if not plist:
      raise BisectException(f'Could not find Info.plist from {executable}.')
    retcode, stdout, stderr = self._run(
        ['plutil', '-extract', 'CFBundleIdentifier', 'raw', plist[0]])
    if retcode:
      raise BisectException(f'Extract bundle identifier error, code:{retcode}\n'
                            f'stdout:\n{stdout}\n'
                            f'stderr:\n{stderr}')
    bundle_identifier = stdout.strip()
    return bundle_identifier

  def _launch_revision(self, tempdir, bundle_identifier, args=()):
    retcode, stdout, stderr = self._run(
        ['xcrun', 'simctl', 'launch', self.device_id, bundle_identifier, *args])
    if retcode:
      print(f'Warning: App launching error, code:{retcode}\n'
            f'stdout:\n{stdout}\n'
            f'stderr:\n{stderr}')
    return retcode, stdout, stderr


def create_archive_build(options):
  if options.build_type == 'release':
    if options.archive == 'android-arm64-high':
      return AndroidTrichromeReleaseBuild(options)
    elif options.archive.startswith('android'):
      return AndroidReleaseBuild(options)
    elif options.archive.startswith('linux'):
      return LinuxReleaseBuild(options)
    elif options.archive == 'ios-simulator':
      return IOSSimulatorReleaseBuild(options)
    elif options.archive == 'ios':
      return IOSReleaseBuild(options)
    return ReleaseBuild(options)
  elif options.build_type == 'official':
    if options.archive == 'android-arm64-high':
      return AndroidTrichromeOfficialBuild(options)
    elif options.archive.startswith('android'):
      return AndroidOfficialBuild(options)
    return OfficialBuild(options)
  elif options.build_type == 'asan':
    # ASANBuild is only supported on win/linux/mac.
    return ASANBuild(options)
  else:
    if options.archive.startswith('android'):
      return AndroidSnapshotBuild(options)
    return SnapshotBuild(options)


def IsMac():
  return sys.platform.startswith('darwin')


def UnzipFilenameToDir(filename, directory):
  """Unzip |filename| to |directory|."""
  cwd = os.getcwd()
  if not os.path.isabs(filename):
    filename = os.path.join(cwd, filename)
  # Make base.
  if not os.path.isdir(directory):
    os.mkdir(directory)
  os.chdir(directory)

  # Support for tar archives.
  if tarfile.is_tarfile(filename):
    tf = tarfile.open(filename, 'r')
    tf.extractall(directory)
    os.chdir(cwd)
    return

  # The Python ZipFile does not support symbolic links, which makes it
  # unsuitable for Mac builds. so use ditto instead.
  if IsMac():
    unzip_cmd = ['ditto', '-x', '-k', filename, '.']
    proc = subprocess.Popen(unzip_cmd, bufsize=0, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    proc.communicate()
    os.chdir(cwd)
    return

  zf = zipfile.ZipFile(filename)
  # Extract files.
  for info in zf.infolist():
    name = info.filename
    if name.endswith('/'):  # dir
      if not os.path.isdir(name):
        os.makedirs(name)
    else:  # file
      directory = os.path.dirname(name)
      if directory and not os.path.isdir(directory):
        os.makedirs(directory)
      out = open(name, 'wb')
      out.write(zf.read(name))
      out.close()
    # Set permissions. Permission info in external_attr is shifted 16 bits.
    os.chmod(name, info.external_attr >> 16)
  os.chdir(cwd)


def gsutil_download(download_url, filename):
  command = ['cp', download_url, filename]
  RunGsutilCommand(command)


def EvaluateRevision(archive_build, download, revision, args, evaluate):
  """fetch.wait_for(), archive_build.run_revision() and evaluate the result."""
  while True:
    exit_status = stdout = stderr = None
    # Create a temp directory and unzip the revision into it.
    with tempfile.TemporaryDirectory(prefix='bisect_tmp') as tempdir:
      # On Windows 10, file system needs to be readable from App Container.
      if sys.platform == 'win32' and platform.release() == '10':
        icacls_cmd = ['icacls', tempdir, '/grant', '*S-1-15-2-2:(OI)(CI)(RX)']
        proc = subprocess.Popen(icacls_cmd,
                                bufsize=0,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)
        proc.communicate()
      # run_revision
      print(f'Trying revision {revision!s}: {download!s} in {tempdir!s}')
      try:
        exit_status, stdout, stderr = archive_build.run_revision(
            download, tempdir, args)
      except SystemExit:
        raise
      except Exception:
        traceback.print_exc(file=sys.stderr)
      # evaluate
      answer = evaluate(revision, exit_status, stdout, stderr)
      if answer != 'r':
        return answer


# The arguments release_builds, status, stdout and stderr are unused.
# They are present here because this function is passed to Bisect which then
# calls it with 5 arguments.
# pylint: disable=W0613
def AskIsGoodBuild(rev, exit_status, stdout, stderr):
  """Asks the user whether build |rev| is good or bad."""
  # Loop until we get a response that we can parse.
  while True:
    response = input('Revision %s is '
                     '[(g)ood/(b)ad/(r)etry/(u)nknown/(s)tdout/(q)uit]: ' %
                     str(rev))
    if response in ('g', 'b', 'r', 'u'):
      return response
    if response == 'q':
      raise SystemExit()
    if response == 's':
      print(stdout)
      print(stderr)


def IsGoodASANBuild(rev, exit_status, stdout, stderr):
  """Determine if an ASAN build |rev| is good or bad

  Will examine stderr looking for the error message emitted by ASAN. If not
  found then will fallback to asking the user."""
  if stderr:
    bad_count = 0
    for line in stderr.splitlines():
      print(line)
      if line.find('ERROR: AddressSanitizer:') != -1:
        bad_count += 1
    if bad_count > 0:
      print('Revision %d determined to be bad.' % rev)
      return 'b'
  return AskIsGoodBuild(rev, exit_status, stdout, stderr)


def DidCommandSucceed(rev, exit_status, stdout, stderr):
  if exit_status:
    print('Bad revision: %s' % rev)
    return 'b'
  else:
    print('Good revision: %s' % rev)
    return 'g'


class DownloadJob:
  """
  DownloadJob represents a task to download a given url.
  """

  def __init__(self, url, rev, name=None):
    """
    Args:
      url: The url to download or a dict of {key: url} to download multiple
        targets.
      rev: The revision of the target.
      name: The name of the thread.
    """
    if isinstance(url, dict):
      self.is_multiple = True
      self.urls = url
    else:
      self.is_multiple = False
      self.urls = {None: url}
    self.rev = rev
    self.name = name

    self.results = {}
    self.exc_info = None  # capture exception from worker thread
    self.quit_event = threading.Event()
    self.progress_event = threading.Event()
    self.thread = None

  def _clear_up_tmp_files(self):
    if not self.results:
      return
    for tmp_file in self.results.values():
      try:
        os.unlink(tmp_file)
      except FileNotFoundError:
        # Handle missing archives.
        pass
    self.results = None

  def __del__(self):
    self._clear_up_tmp_files()

  def _report_hook(self, blocknum, blocksize, totalsize):
    if self.quit_event and self.quit_event.is_set():
      raise RuntimeError('Aborting download of revision %s' % str(self.rev))
    if not self.progress_event or not self.progress_event.is_set():
      return
    size = blocknum * blocksize
    if totalsize == -1:  # Total size not known.
      progress = 'Received %d bytes' % size
    else:
      size = min(totalsize, size)
      progress = 'Received %d of %d bytes, %.2f%%' % (size, totalsize,
                                                      100.0 * size / totalsize)
    # Send a \r to let all progress messages use just one line of output.
    print(progress, end='\r', flush=True)

  def _fetch(self, url, tmp_file):
    if url.startswith('gs'):
      gsutil_download(url, tmp_file)
    else:
      urllib.request.urlretrieve(url, tmp_file, self._report_hook)
      if self.progress_event and self.progress_event.is_set():
        print()

  def fetch(self):
    try:
      for key, url in self.urls.items():
        # Keep the basename as part of tempfile name that make it easier to
        # identify what's been downloaded.
        basename = os.path.basename(urllib.parse.urlparse(url).path)
        fd, tmp_file = tempfile.mkstemp(suffix=basename)
        self.results[key] = tmp_file
        os.close(fd)
        self._fetch(url, tmp_file)
    except RuntimeError:
      pass
    except BaseException:
      self.exc_info = sys.exc_info()

  def start(self):
    """Start the download in a thread."""
    assert self.thread is None, "DownloadJob is already started."
    self.thread = threading.Thread(target=self.fetch, name=self.name)
    self.thread.start()
    return self

  def stop(self):
    """Stops the download which must have been started previously."""
    assert self.thread, 'DownloadJob must be started before Stop is called.'
    self.quit_event.set()
    self.thread.join()
    self._clear_up_tmp_files()

  def wait_for(self):
    """Prints a message and waits for the download to complete.
    The method will return the path of downloaded files.
    """
    if not self.thread:
      self.start()
    print('Downloading revision %s...' % str(self.rev))
    self.progress_event.set()  # Display progress of download.
    try:
      while self.thread.is_alive():
        # The parameter to join is needed to keep the main thread responsive to
        # signals. Without it, the program will not respond to interruptions.
        self.thread.join(1)
      if self.exc_info:
        raise self.exc_info[1].with_traceback(self.exc_info[2])
      if self.quit_event.is_set():
        raise Exception('The DownloadJob was stopped.')
      if self.is_multiple:
        return self.results
      else:
        return self.results[None]
    except (KeyboardInterrupt, SystemExit):
      self.stop()
      raise


def Bisect(archive_build,
           try_args=(),
           evaluate=AskIsGoodBuild,
           verify_range=False):
  """Runs a binary search on to determine the last known good revision.

    Args:
      archive_build: ArchiveBuild object initialized with user provided
               parameters.
      try_args: A tuple of arguments to pass to the test application.
      evaluate: A function which returns 'g' if the argument build is good,
               'b' if it's bad or 'u' if unknown.
      verify_range: If true, tests the first and last revisions in the range
                    before proceeding with the bisect.

  Threading is used to fetch Chromium revisions in the background, speeding up
  the user's experience. For example, suppose the bounds of the search are
  good_rev=0, bad_rev=100. The first revision to be checked is 50. Depending on
  whether revision 50 is good or bad, the next revision to check will be either
  25 or 75. So, while revision 50 is being checked, the script will download
  revisions 25 and 75 in the background. Once the good/bad verdict on rev 50 is
  known:
    - If rev 50 is good, the download of rev 25 is cancelled, and the next test
      is run on rev 75.
    - If rev 50 is bad, the download of rev 75 is cancelled, and the next test
      is run on rev 25.
  """
  print('Downloading list of known revisions.', end=' ')
  print('If the range is large, this can take several minutes...')
  if not archive_build.use_local_cache:
    print('(use --use-local-cache to cache and re-use the list of revisions)')
  else:
    print()
  rev_list = archive_build.get_rev_list()
  # Ensure rev_list[0] is good and rev_list[-1] is bad for easier process.
  if archive_build.good_revision > archive_build.bad_revision:
    rev_list = rev_list[::-1]

  if verify_range:
    good_rev_fetch = archive_build.get_download_job(rev_list[0],
                                                    'good_rev_fetch').start()
    bad_rev_fetch = archive_build.get_download_job(rev_list[-1],
                                                   'bad_rev_fetch').start()
    try:
      good_download = good_rev_fetch.wait_for()
      answer = EvaluateRevision(archive_build, good_download, rev_list[0],
                                try_args, evaluate)
      if answer != 'g':
        print(f'Expecting revision {rev_list[0]} to be good but got {answer}. '
              'Please make sure the --good is a good revision.')
        raise SystemExit
      bad_download = bad_rev_fetch.wait_for()
      answer = EvaluateRevision(archive_build, bad_download, rev_list[-1],
                                try_args, evaluate)
      if answer != 'b':
        print(f'Expecting revision {rev_list[-1]} to be bad but got {answer}. '
              'Please make sure that the issue can be reproduced for --bad.')
        raise SystemExit
    except (KeyboardInterrupt, SystemExit):
      print('Cleaning up...')
      return None, None
    finally:
      good_rev_fetch.stop()
      bad_rev_fetch.stop()

  prefetch = {}
  try:
    while len(rev_list) > 2:
      # We are retaining the boundary elements in the rev_list, that should not
      # count towards the steps when calculating the number the steps.
      print('You have %d revisions with about %d steps left.' %
            (len(rev_list), ((len(rev_list) - 2).bit_length())))
      print('Bisecting range [%s (bad), %s (good)].' %
            (rev_list[-1], rev_list[0]))
      # clean prefetch to keep only the valid fetches
      for key in list(prefetch.keys()):
        if key not in rev_list:
          prefetch.pop(key).stop()
      # get next revision to evaluate from prefetch
      if prefetch:
        fetch = None
        # For any possible index in rev_list, abs(mid - index) < abs(mid -
        # pivot). This will ensure that we can always get a fetch from prefetch.
        pivot = len(rev_list)
        for revision, pfetch in prefetch.items():
          prefetch_pivot = rev_list.index(revision)
          # Prefer the revision closer to the mid point.
          mid_point = len(rev_list) // 2
          if abs(mid_point - pivot) > abs(mid_point - prefetch_pivot):
            fetch = pfetch
            pivot = prefetch_pivot
        prefetch.pop(rev_list[pivot])
      # or just the mid point
      else:
        pivot = len(rev_list) // 2
        fetch = archive_build.get_download_job(rev_list[pivot], 'fetch').start()
      # prefetch left_pivot = len(rev_list[:pivot+1]) // 2
      left_revision = rev_list[(pivot + 1) // 2]
      if left_revision != rev_list[0] and left_revision not in prefetch:
        prefetch[left_revision] = archive_build.get_download_job(
            left_revision, 'prefetch').start()
      # prefetch right_pivot = len(rev_list[pivot:]) // 2
      right_revision = rev_list[(len(rev_list) + pivot) // 2]
      if right_revision != rev_list[-1] and right_revision not in prefetch:
        prefetch[right_revision] = archive_build.get_download_job(
            right_revision, 'prefetch').start()
      try:
        # evaluate the revision
        download = fetch.wait_for()
        answer = EvaluateRevision(archive_build, download, rev_list[pivot],
                                  try_args, evaluate)
        # Ensure rev_list[0] is good and rev_list[-1] is bad after adjust.
        if answer == 'g':  # good
          rev_list = rev_list[pivot:]
        elif answer == 'b':  # bad
          # Retain the pivot element within the list to act as a confirmed
          # boundary for identifying bad revisions.
          rev_list = rev_list[:pivot + 1]
        elif answer == 'u':  # unknown
          # Nuke the revision from the rev_list.
          rev_list.pop(pivot)
        else:
          assert False, 'Unexpected return value from evaluate(): ' + answer
      finally:
        fetch.stop()
    # end of `while len(rev_list) > 2`
  finally:
    for each in prefetch.values():
      each.stop()
    prefetch.clear()
  return sorted((rev_list[0], rev_list[-1]))

def GetChromiumRevision(url, default=999999999):
  """Returns the chromium revision read from given URL."""
  if not url:
    return default
  try:
    # Location of the latest build revision number
    latest_revision = urllib.request.urlopen(url).read()
    if latest_revision.isdigit():
      return int(latest_revision)
    return default
  except Exception:
    print('Could not determine latest revision. This could be bad...')
    return default


def FetchJsonFromURL(url):
  """Returns JSON data from the given URL"""
  # Allow retry for 3 times for unexpected network error
  for i in range(3):
    try:
      return json.loads(urllib.request.urlopen(url).read())
    except urllib.request.HTTPError as e:
      print(f'urlopen {url} HTTPError: {e}')
    except json.JSONDecodeError as e:
      print(f'urlopen {url} JSON decode error: {e}')
  return None

def GetGitHashFromSVNRevision(svn_revision):
  """Returns GitHash from SVN Revision"""
  crrev_url = CRREV_URL + str(svn_revision)
  data = FetchJsonFromURL(crrev_url)
  if data and 'git_sha' in data:
    return data['git_sha']
  return None

def PrintChangeLog(min_chromium_rev, max_chromium_rev):
  """Prints the changelog URL."""
  print(('  ' + CHANGELOG_URL % (GetGitHashFromSVNRevision(min_chromium_rev),
                                 GetGitHashFromSVNRevision(max_chromium_rev))))


def IsVersionNumber(revision):
  """Checks if provided revision is version_number"""
  return re.match(r'^\d+\.\d+\.\d+\.\d+$', revision) is not None


def GetRevisionFromSourceTag(tag):
  """Return Base Commit Position based on the commit message of a version tag"""
  # Searching from commit message for
  # Cr-Branched-From: (?P<githash>\w+)-refs/heads/master@{#857950}
  # Cr-Commit-Position: refs/heads/main@{#992738}
  revision_regex = re.compile(r'refs/heads/\w+@{#(\d+)}$')
  source_url = SOURCE_TAG_URL % str(tag)
  data = FetchJsonFromURL(source_url)
  match = revision_regex.search(data.get('message', ''))
  if not match:
    # The commit message for version tag before M116 doesn't contains
    # Cr-Branched-From and Cr-Commit-Position message lines. However they might
    # exists in the parent commit.
    source_url = SOURCE_TAG_URL % str(tag) + '^'
    data = FetchJsonFromURL(source_url)
    match = revision_regex.search(data.get('message', ''))
  if match:
    return int(match.group(1))


def GetRevisionFromVersion(version):
  """Returns Base Commit Position from a version number"""
  chromiumdash_url = VERSION_INFO_URL % str(version)
  data = FetchJsonFromURL(chromiumdash_url)
  if not data:
    # Not every tag is released as a version for users. Some "versions" from the
    # release builder might not exist in the VERSION_INFO_URL API. With the
    # `MaybeSwitchBuildType` functionality, users might get such unreleased
    # versions and try to use them with the -o flag, resulting in a 404 error.
    # Meanwhile, this method retrieves the `chromium_main_branch_position`,
    # which should be the same for all 127.0.6533.* versions, so we can get the
    # branch position from 127.0.6533.0 instead.
    chromiumdash_url = VERSION_INFO_URL % re.sub(r'\d+$', '0', str(version))
    data = FetchJsonFromURL(chromiumdash_url)
  if data and data.get('chromium_main_branch_position'):
    return data['chromium_main_branch_position']
  revision_from_source_tag = GetRevisionFromSourceTag(version)
  if revision_from_source_tag:
    return revision_from_source_tag
  raise BisectException(
      f'Can not find revision for {version} from chromiumdash and source')


def GetRevisionFromMilestone(milestone):
  """Get revision (e.g. 782793) from milestone such as 85."""
  response = urllib.request.urlopen(MILESTONES_URL % milestone)
  milestones = json.loads(response.read())
  for m in milestones:
    if m['milestone'] == milestone:
      return m['chromium_main_branch_position']
  raise BisectException(f'Can not find revision for milestone {milestone}')


def GetRevision(revision):
  """Get revision from either milestone M85, full version 85.0.4183.0,
     or a commit position.
  """
  if type(revision) == type(0):
    return revision
  if IsVersionNumber(revision):
    return GetRevisionFromVersion(revision)
  elif revision[:1].upper() == 'M' and revision[1:].isdigit():
    return GetRevisionFromMilestone(int(revision[1:]))
  # By default, we assume it's a commit position.
  return int(revision)


def CheckDepotToolsInPath():
  delimiter = ';' if sys.platform.startswith('win') else ':'
  path_list = os.environ['PATH'].split(delimiter)
  for path in path_list:
    if path.rstrip(os.path.sep).endswith('depot_tools'):
      return path
  return None


def SetupEnvironment(options):
  global is_verbose
  global GSUTILS_PATH

  # Release and Official builds bisect requires "gsutil" inorder to
  # List and Download binaries.
  # Check if depot_tools is installed and path is set.
  gsutil_path = CheckDepotToolsInPath()
  if (options.build_type in ('release', 'official') and not gsutil_path):
    raise BisectException(
        'Looks like depot_tools is not installed.\n'
        'Follow the instructions in this document '
        'http://dev.chromium.org/developers/how-tos/install-depot-tools '
        'to install depot_tools and then try again.')
  elif gsutil_path:
    GSUTILS_PATH = os.path.join(gsutil_path, 'gsutil.py')

  # Catapult repo is required for Android bisect,
  # Update Catapult repo if it exists otherwise checkout repo.
  if options.archive.startswith('android-'):
    SetupAndroidEnvironment()

  # Set up verbose logging if requested.
  if options.verbose:
    is_verbose = True


def SetupAndroidEnvironment():

  def SetupCatapult():
    print('Setting up Catapult in %s.' % CATAPULT_DIR)
    print('Set the environment var CATAPULT_DIR to override '
          'Catapult directory.')
    if (os.path.exists(CATAPULT_DIR)):
      print('Updating Catapult...\n')
      process = subprocess.Popen(args=['git', 'pull', '--rebase'],
                                 cwd=CATAPULT_DIR)
      exit_code = process.wait()
      if exit_code != 0:
        raise BisectException('Android bisect requires Catapult repo checkout. '
                              'Attempt to update Catapult failed.')
    else:
      print('Downloading Catapult...\n')
      process = subprocess.Popen(
          args=['git', 'clone', CATAPULT_REPO, CATAPULT_DIR])
      exit_code = process.wait()
      if exit_code != 0:
        raise BisectException('Android bisect requires Catapult repo checkout. '
                              'Attempt to download Catapult failed.')

  SetupCatapult()
  sys.path.append(DEVIL_PATH)
  from devil.android.sdk import version_codes

  # Modules required from devil
  devil_imports = {
      'devil_env': 'devil.devil_env',
      'device_errors': 'devil.android.device_errors',
      'device_utils': 'devil.android.device_utils',
      'flag_changer': 'devil.android.flag_changer',
      'chrome': 'devil.android.constants.chrome',
      'adb_wrapper': 'devil.android.sdk.adb_wrapper',
      'intent': 'devil.android.sdk.intent',
      'version_codes': 'devil.android.sdk.version_codes',
      'run_tests_helper': 'devil.utils.run_tests_helper'
  }
  # Dynamically import devil modules required for android bisect.
  for i, j in devil_imports.items():
    globals()[i] = importlib.import_module(j)

  print('Done setting up Catapult.\n')


def InitializeAndroidDevice(device_id, apk, chrome_flags):
  """Initializes device and sets chrome flags."""
  devil_env.config.Initialize()
  run_tests_helper.SetLogLevel(0)
  device = device_utils.DeviceUtils.HealthyDevices(device_arg=device_id)[0]
  if chrome_flags:
    flags = flag_changer.FlagChanger(device,
                                     chrome.PACKAGE_INFO[apk].cmdline_file)
    flags.AddFlags(chrome_flags)
  return device


def InstallOnAndroid(device, apk_path):
  """Installs the chromium build on a given device."""
  print('Installing %s on android device...' % apk_path)
  device.Install(apk_path)


def LaunchOnAndroid(device, apk):
  """Launches the chromium build on a given device."""
  if 'webview' in apk:
    return

  print('Launching  chrome on android device...')
  device.StartActivity(intent.Intent(action='android.intent.action.MAIN',
                                     activity=chrome.PACKAGE_INFO[apk].activity,
                                     package=chrome.PACKAGE_INFO[apk].package),
                       blocking=True,
                       force_stop=True)


def _CreateCommandLineParser():
  """Creates a parser with bisect options.

  Returns:
    An instance of argparse.ArgumentParser.
  """
  description = """
Performs binary search on the chrome binaries to find a minimal range of \
revisions where a behavior change happened.
The behaviors are described as "good" and "bad". It is NOT assumed that the \
behavior of the later revision is the bad one.

Revision numbers should use:
  a) Release versions: (e.g. 1.0.1000.0) for release builds. (-r)
  b) Commit Positions: (e.g. 123456) for chromium builds, from trunk.
        Use chromium_main_branch_position from \
https://chromiumdash.appspot.com/fetch_version?version=<chrome_version>
        Please Note: Chrome's about: build number and chromiumdash branch \
revision are incorrect, they are from branches.

Tip: add "-- --no-first-run" to bypass the first run prompts.
"""

  parser = argparse.ArgumentParser(
      formatter_class=argparse.RawTextHelpFormatter, description=description)
  # Strangely, the default help output doesn't include the choice list.
  choices = sorted(
      set(arch for build in PATH_CONTEXT for arch in PATH_CONTEXT[build]))
  parser.add_argument(
      '-a',
      '--archive',
      choices=choices,
      metavar='ARCHIVE',
      required=True,
      help='The buildbot platform to bisect {%s}.' % ','.join(choices),
  )

  build_type_group = parser.add_mutually_exclusive_group()
  build_type_group.add_argument(
      '-s',
      dest='build_type',
      action='store_const',
      const='snapshot',
      default='snapshot',
      help='Bisect across Chromium snapshot archives (default).',
  )
  build_type_group.add_argument(
      '-r',
      dest='build_type',
      action='store_const',
      const='release',
      help='Bisect across release Chrome builds (internal only) instead of '
      'Chromium archives.',
  )
  build_type_group.add_argument(
      '-o',
      dest='build_type',
      action='store_const',
      const='official',
      help='Bisect across continuous perf official Chrome builds (internal '
      'only) instead of Chromium archives.',
  )
  build_type_group.add_argument(
      '--asan',
      dest='build_type',
      action='store_const',
      const='asan',
      help='Allow the script to bisect ASAN builds',
  )

  parser.add_argument(
      '-g',
      '--good',
      type=str,
      metavar='GOOD_REVISION',
      required=True,
      help='A good revision to start bisection. May be earlier or later than '
      'the bad revision.',
  )
  parser.add_argument(
      '-b',
      '--bad',
      type=str,
      metavar='BAD_REVISION',
      help='A bad revision to start bisection. May be earlier or later than '
      'the good revision. Default is HEAD.',
  )
  parser.add_argument(
      '-p',
      '--profile',
      '--user-data-dir',
      type=str,
      default='%t/profile',
      help='Profile to use; this will not reset every run. Defaults to a clean '
      'profile.',
  )
  parser.add_argument(
      '-t',
      '--times',
      type=int,
      default=1,
      help='Number of times to run each build before asking if it\'s good or '
      'bad. Temporary profiles are reused.',
  )
  parser.add_argument(
      '--chromedriver',
      action='store_true',
      help='Also download ChromeDriver. Use %%d in --command to reference the '
      'ChromeDriver path in the command line.',
  )
  parser.add_argument(
      '-c',
      '--command',
      type=str,
      default=r'%p %a',
      help='Command to execute. %%p and %%a refer to Chrome executable and '
      'specified extra arguments respectively. Use %%t for tempdir where '
      'Chrome extracted. Use %%d for chromedriver path when --chromedriver '
      'enabled. Defaults to "%%p %%a". Note that any extra paths specified '
      'should be absolute.',
  )
  parser.add_argument(
      '-v',
      '--verbose',
      action='store_true',
      help='Log more verbose information.',
  )
  parser.add_argument(
      '--not-interactive',
      action='store_true',
      default=False,
      help='Use command exit code to tell good/bad revision.',
  )

  local_cache_group = parser.add_mutually_exclusive_group()
  local_cache_group.add_argument(
      '--use-local-cache',
      dest='use_local_cache',
      action='store_true',
      default=True,
      help='Use a local file in the current directory to cache a list of known '
      'revisions to speed up the initialization of this script.',
  )
  local_cache_group.add_argument(
      '--no-local-cache',
      dest='use_local_cache',
      action='store_false',
      help='Do not use local file for known revisions.',
  )

  parser.add_argument(
      '--verify-range',
      dest='verify_range',
      action='store_true',
      default=False,
      help='Test the first and last revisions in the range before proceeding '
      'with the bisect.',
  )
  apk_choices = sorted(set().union(CHROME_APK_FILENAMES,
                                   CHROME_MODERN_APK_FILENAMES,
                                   MONOCHROME_APK_FILENAMES,
                                   WEBVIEW_APK_FILENAMES))
  parser.add_argument(
      '--apk',
      choices=apk_choices,
      dest='apk',
      default='chromium',
      metavar='{chromium,chrome_dev,android_webview...}',
      help='Apk you want to bisect {%s}.' % ','.join(apk_choices),
  )
  parser.add_argument(
      '--ipa',
      dest='ipa',
      default='canary.ipa',
      metavar='{canary,beta,stable...}',
      help='ipa you want to bisect.',
  )
  parser.add_argument(
      '--signed',
      dest='signed',
      action='store_true',
      default=False,
      help='Using signed binary for release build. Only support iOS and '
      'Android platforms.',
  )
  parser.add_argument(
      '-d',
      '--device-id',
      dest='device_id',
      type=str,
      help='Device to run the bisect on.',
  )
  parser.add_argument(
      '--update-script',
      dest='update_script',
      action='store_true',
      default=False,
      help='Update this script to the latest.',
  )
  parser.add_argument(
      'args',
      nargs='*',
      metavar='chromium-option',
      help='Additional chromium options passed to chromium process.',
  )
  return parser


def _DetectArchive():
  """Detect the buildbot archive to use based on local environment."""
  os_name = None
  plat = sys.platform
  if plat.startswith('linux'):
    os_name = 'linux'
  elif plat in ('win32', 'cygwin'):
    os_name = 'win'
  elif plat == 'darwin':
    os_name = 'mac'

  arch = None
  machine = platform.machine().lower()
  if machine.startswith(('arm', 'aarch')):
    arch = 'arm'
  elif machine in ('amd64', 'x86_64'):
    arch = 'x64'
  elif machine in ('i386', 'i686', 'i86pc', 'x86'):
    arch = 'x86'

  return PLATFORM_ARCH_TO_ARCHIVE_MAPPING.get((os_name, arch), None)


def ParseCommandLine(args=None):
  """Parses the command line for bisect options."""
  parser = _CreateCommandLineParser()
  opts = parser.parse_args(args)

  if opts.update_script:
    UpdateScript()

  if opts.archive is None:
    archive = _DetectArchive()
    if archive:
      print('The buildbot archive (-a/--archive) detected as:', archive)
      opts.archive = archive
    else:
      parser.error('Error: Missing required parameter: --archive')

  if opts.archive not in PATH_CONTEXT[opts.build_type]:
    supported_build_types = [
        "%s(%s)" % (b, BuildTypeToCommandLineArgument(b, omit_default=False))
        for b, context in PATH_CONTEXT.items() if opts.archive in context
    ]
    parser.error(f'Bisecting on {opts.build_type} is only supported on these '
                 'platforms (-a/--archive): '
                 f'{{{",".join(PATH_CONTEXT[opts.build_type].keys())}}}\n'
                 f'To bisect for {opts.archive}, please choose from '
                 f'{", ".join(supported_build_types)}')

  if opts.signed and not (opts.archive.startswith('android-')
                          or opts.archive.startswith('ios')):
    parser.error('--signed is only supported for Android and iOS platform.')

  if opts.signed and not opts.build_type == 'release':
    parser.error('--signed is only supported for release bisection.')

  if opts.build_type == 'official':
    print('Bisecting on continuous Chrome builds. If you would like '
          'to bisect on release builds, try running with -r option '
          'instead. Previous -o options is currently changed to -r option '
          'as continous official builds were added for bisect')

  if not opts.good:
    parser.error('Please specify a good version.')

  if opts.build_type == 'release':
    if not opts.bad:
      parser.error('Please specify a bad version.')
    if not IsVersionNumber(opts.good) or not IsVersionNumber(opts.bad):
      parser.error('For release, you can only use chrome version to bisect.')
    if opts.archive.startswith('android-'):
      # Channel builds have _ in their names, e.g. chrome_canary or chrome_beta.
      # Non-channel builds don't, e.g. chrome or chromium. Make this a warning
      # instead of an error since older archives might have non-channel builds.
      if '_' not in opts.apk:
        print('WARNING: Android release typically only uploads channel builds, '
              f'so you will often see "Found 0 builds" with --apk={opts.apk}'
              '. Switch to using --apk=chrome_stable or one of the other '
              'channels if you see `[Bisect Exception]: Could not found enough'
              'revisions for Android chrome release channel.\n')

  if opts.times < 1:
    parser.error(f'Number of times to run ({opts.times}) must be greater than '
                 'or equal to 1.')

  return opts


def BuildTypeToCommandLineArgument(build_type, omit_default=True):
  """Convert the build_type back to command line argument."""
  if build_type == 'release':
    return '-r'
  elif build_type == 'official':
    return '-o'
  elif build_type == 'snapshot':
    if not omit_default:
      return '-s'
    else:
      return ''
  elif build_type == 'asan':
    return '--asan'
  else:
    raise ValueError(f'Unknown build type: {build_type}')


def GenerateCommandLine(opts):
  """Generate a command line for bisect options.

  Args:
    opts: The new bisect options to generate the command line for.

  This generates prompts for the suggestion to use another build type
  (MaybeSwitchBuildType). Not all options are supported when generating the new
  command, however the remaining unsupported args would be copied from sys.argv.
  """
  # Using a parser to remove the arguments (key and value) that we are going to
  # generate based on the opts and appending the remaining args as is in command
  # line.
  parser_to_remove_known_options = argparse.ArgumentParser()
  parser_to_remove_known_options.add_argument('-a', '--archive', '-g', '--good',
                                              '-b', '--bad')
  parser_to_remove_known_options.add_argument('-r',
                                              '-o',
                                              '--signed',
                                              action='store_true')
  _, remaining_args = parser_to_remove_known_options.parse_known_args()
  args = []
  args.append(BuildTypeToCommandLineArgument(opts.build_type))
  if opts.archive:
    args.extend(['-a', opts.archive])
  if opts.signed:
    args.append('--signed')
  if opts.good:
    args.extend(['-g', opts.good])
  if opts.bad:
    args.extend(['-b', opts.bad])
  if opts.verify_range:
    args.append('--verify-range')
  return ['./%s' % os.path.relpath(__file__)] + args + remaining_args


def MaybeSwitchBuildType(opts, good, bad):
  """Generate and print suggestions to use official build to bisect for a more
  precise range when possible."""
  if opts.build_type != 'release':
    return
  if opts.archive not in PATH_CONTEXT['official']:
    return
  new_opts = copy.deepcopy(opts)
  new_opts.signed = False  # --signed is only supported by release builds
  new_opts.build_type = 'official'
  new_opts.verify_range = True  # always verify_range when switching the build
  new_opts.good = str(good)  # good could be LooseVersion
  new_opts.bad = str(bad)  # bad could be LooseVersion
  rev_list = None
  if opts.use_local_cache:
    print('Checking available official builds (-o) for %s.' % new_opts.archive)
    archive_build = create_archive_build(new_opts)
    try:
      rev_list = archive_build.get_rev_list()
    except BisectException as e:
      # We don't have enough builds from official builder, skip suggesting.
      print("But we don't have more builds from official builder.")
      return
    if len(rev_list) <= 2:
      print("But we don't have more builds from official builder.")
      return
  if rev_list:
    print(
        "There are %d revisions between %s and %s from the continuous official "
        "build (-o). You could try to get a more precise culprit range using "
        "the following command:" % (len(rev_list), *sorted([good, bad])))
  else:
    print(
        "You could try to get a more precise culprit range with the continuous "
        "official build (-o) using the following command:")
  command_line = GenerateCommandLine(new_opts)
  print(shlex.join(command_line))
  return command_line


def UpdateScript():
  script_path = sys.argv[0]
  script_content = str(
      base64.b64decode(
          urllib.request.urlopen(
              "https://chromium.googlesource.com/chromium/src/+/HEAD/"
              "tools/bisect-builds.py?format=TEXT").read()), 'utf-8')
  with open(script_path, "w") as f:
    f.write(script_content)
  print("Update successful!")
  exit(0)


def main():
  opts = ParseCommandLine()

  try:
    SetupEnvironment(opts)
  except BisectException as e:
    print(e)
    sys.exit(1)

  # Create the AbstractBuild object.
  archive_build = create_archive_build(opts)

  if opts.not_interactive:
    evaluator = DidCommandSucceed
  elif opts.build_type == 'asan':
    evaluator = IsGoodASANBuild
  else:
    evaluator = AskIsGoodBuild

  # Save these revision numbers to compare when showing the changelog URL
  # after the bisect.
  good_rev = archive_build.good_revision
  bad_rev = archive_build.bad_revision

  min_chromium_rev, max_chromium_rev = Bisect(archive_build, opts.args,
                                              evaluator, opts.verify_range)
  if min_chromium_rev is None or max_chromium_rev is None:
    return
  # We're done. Let the user know the results in an official manner.
  if good_rev > bad_rev:
    print(DONE_MESSAGE_GOOD_MAX %
          (str(min_chromium_rev), str(max_chromium_rev)))
    good_rev, bad_rev = max_chromium_rev, min_chromium_rev
  else:
    print(DONE_MESSAGE_GOOD_MIN %
          (str(min_chromium_rev), str(max_chromium_rev)))
    good_rev, bad_rev = min_chromium_rev, max_chromium_rev

  print('CHANGELOG URL:')
  if opts.build_type == 'release':
    print(RELEASE_CHANGELOG_URL % (min_chromium_rev, max_chromium_rev))
    MaybeSwitchBuildType(opts, good=good_rev, bad=bad_rev)
  else:
    if opts.build_type == 'official':
      print('The script might not always return single CL as suspect '
            'as some perf builds might get missing due to failure.')
    PrintChangeLog(min_chromium_rev, max_chromium_rev)

if __name__ == '__main__':
  sys.exit(main())
