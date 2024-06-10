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

import base64
import bisect
import http.client
import importlib
import json
import optparse
import os
import platform
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import time
import threading
import traceback
import urllib.request, urllib.parse, urllib.error
from distutils.version import LooseVersion
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
WEBKIT_BASE_URL = ('http://commondatastorage.googleapis.com'
                   '/chromium-webkit-snapshots')
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

# Blink changelogs URL.
BLINK_CHANGELOG_URL = ('http://build.chromium.org'
                      '/f/chromium/perf/dashboard/ui/changelog_blink.html'
                      '?url=/trunk&range=%d%%3A%d')

DONE_MESSAGE_GOOD_MIN = ('You are probably looking for a change made after %s ('
                         'known good), but no later than %s (first known bad).')
DONE_MESSAGE_GOOD_MAX = ('You are probably looking for a change made after %s ('
                         'known bad), but no later than %s (first known good).')

CHROMIUM_GITHASH_TO_SVN_URL = (
    'https://chromium.googlesource.com/chromium/src/+/%s?format=json')

BLINK_GITHASH_TO_SVN_URL = (
    'https://chromium.googlesource.com/chromium/blink/+/%s?format=json')

GITHASH_TO_SVN_URL = {
    'chromium': CHROMIUM_GITHASH_TO_SVN_URL,
    'blink': BLINK_GITHASH_TO_SVN_URL,
}

VERSION_INFO_URL = ('https://chromiumdash.appspot.com/fetch_version?version=%s')

# Search pattern to be matched in the JSON output from
# CHROMIUM_GITHASH_TO_SVN_URL to get the chromium revision (svn revision).
CHROMIUM_SEARCH_PATTERN_OLD = (
    r'.*git-svn-id: svn://svn.chromium.org/chrome/trunk/src@(\d+) ')
CHROMIUM_SEARCH_PATTERN = (r'Cr-Commit-Position: refs/heads/main@{#(\d+)}')

# Search pattern to be matched in the json output from
# BLINK_GITHASH_TO_SVN_URL to get the blink revision (svn revision).
BLINK_SEARCH_PATTERN = (
    r'.*git-svn-id: svn://svn.chromium.org/blink/trunk@(\d+) ')

SEARCH_PATTERN = {
    'chromium': CHROMIUM_SEARCH_PATTERN,
    'blink': BLINK_SEARCH_PATTERN,
}

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
        'linux64': {
            'binary_name': 'chrome',
            'listing_platform_dir': 'linux64/',
            'archive_name': 'chrome-linux64.zip',
            'archive_extract_dir': 'chrome-linux64'
        },
        'mac': {
            'binary_name': 'Google Chrome.app/Contents/MacOS/Google Chrome',
            'listing_platform_dir': 'mac/',
            'archive_name': 'chrome-mac.zip',
            'archive_extract_dir': 'chrome-mac'
        },
        'mac64': {
            'binary_name': 'Google Chrome.app/Contents/MacOS/Google Chrome',
            'listing_platform_dir': 'mac64/',
            'archive_name': 'chrome-mac.zip',
            'archive_extract_dir': 'chrome-mac'
        },
        'mac-arm': {
            'binary_name': 'Google Chrome.app/Contents/MacOS/Google Chrome',
            'listing_platform_dir': 'mac-arm64/',
            'archive_name': 'chrome-mac.zip',
            'archive_extract_dir': 'chrome-mac'
        },
        'win-clang': {  # Release builds switched to -clang in M64.
            'binary_name': 'chrome.exe',
            'listing_platform_dir': 'win-clang/',
            'archive_name': 'chrome-win-clang.zip',
            'archive_extract_dir': 'chrome-win-clang'
        },
        'win64-clang': {  # Release builds switched to -clang in M64.
            'binary_name': 'chrome.exe',
            'listing_platform_dir': 'win64-clang/',
            'archive_name': 'chrome-win64-clang.zip',
            'archive_extract_dir': 'chrome-win64-clang'
        },
        'lacros64': {
            'binary_name': 'chrome',
            'listing_platform_dir': 'lacros64/',
            'archive_name': 'lacros.zip',
            'archive_extract_dir': 'chrome-lacros64'
        },
        'lacros-arm32': {
            'binary_name': 'chrome',
            'listing_platform_dir': 'lacros-arm32/',
            'archive_name': 'lacros.zip',
            'archive_extract_dir': 'chrome-lacros-arm32'
        },
        'lacros-arm64': {
            'binary_name': 'chrome',
            'listing_platform_dir': 'lacros-arm64/',
            'archive_name': 'lacros.zip',
            'archive_extract_dir': 'chrome-lacros-arm64'
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
        'linux64': {
            'binary_name': 'chrome',
            'listing_platform_dir': 'linux-builder-perf/',
            'archive_name': 'chrome-perf-linux.zip',
            'archive_extract_dir': 'full-build-linux'
        },
        'mac': {
            'binary_name': 'Google Chrome.app/Contents/MacOS/Google Chrome',
            'listing_platform_dir': 'mac-builder-perf/',
            'archive_name': 'chrome-perf-mac.zip',
            'archive_extract_dir': 'full-build-mac'
        },
        'mac-arm': {
            'binary_name': 'Google Chrome.app/Contents/MacOS/Google Chrome',
            'listing_platform_dir': 'mac-arm-builder-perf/',
            'archive_name': 'chrome-perf-mac.zip',
            'archive_extract_dir': 'full-build-mac'
        },
        'win64': {
            'binary_name': 'chrome.exe',
            'listing_platform_dir': 'win64-builder-perf/',
            'archive_name': 'chrome-perf-win.zip',
            'archive_extract_dir': 'full-build-win32'
        },
        'lacros64': {
            'binary_name': 'chrome',
            'listing_platform_dir':
            'chromeos-amd64-generic-lacros-builder-perf/',
            'archive_name': 'chrome-perf-lacros64.zip',
            'archive_extract_dir': 'full-build-linux'
        },
        'lacros-arm32': {
            'binary_name': 'chrome',
            'listing_platform_dir': 'chromeos-arm-generic-lacros-builder-perf/',
            'archive_name': 'chrome-perf-lacros-arm32.zip',
            'archive_extract_dir': 'full-build-linux'
        },
        'lacros-arm64': {
            'binary_name': 'chrome',
            'listing_platform_dir':
            'chromeos-arm64-generic-lacros-builder-perf/',
            'archive_name': 'chrome-perf-lacros-arm64.zip',
            'archive_extract_dir': 'full-build-linux'
        }
    },
    'snapshot': {
        'linux64': {
            'binary_name': 'chrome',
            'listing_platform_dir': 'Linux_x64/',
            'archive_name': 'chrome-linux.zip',
            'archive_extract_dir': 'chrome-linux'
        },
        'linux-arm': {
            'binary_name': 'chrome',
            'listing_platform_dir': 'Linux_ARM_Cross-Compile/',
            'archive_name': 'chrome-linux.zip',
            'archive_extract_dir': 'chrome-linux'
        },
        # Note: changed at revision 591483; see GetDownloadURL and GetLaunchPath
        # below where these are patched.
        'chromeos': {
            'binary_name': 'chrome',
            'listing_platform_dir': 'Linux_ChromiumOS_Full/',
            'archive_name': 'chrome-linux.zip',
            'archive_extract_dir': 'chrome-linux'
        },
        'mac': {
            'binary_name': 'Chromium.app/Contents/MacOS/Chromium',
            'listing_platform_dir': 'Mac/',
            'archive_name': 'chrome-mac.zip',
            'archive_extract_dir': 'chrome-mac'
        },
        'mac64': {
            'binary_name': 'Chromium.app/Contents/MacOS/Chromium',
            'listing_platform_dir': 'Mac/',
            'archive_name': 'chrome-mac.zip',
            'archive_extract_dir': 'chrome-mac'
        },
        'mac-arm': {
            'binary_name': 'Chromium.app/Contents/MacOS/Chromium',
            'listing_platform_dir': 'Mac_Arm/',
            'archive_name': 'chrome-mac.zip',
            'archive_extract_dir': 'chrome-mac'
        },
        # Note: changed at revision 591483; see GetDownloadURL and GetLaunchPath
        # below where these are patched.
        'win': {
            'binary_name': 'chrome.exe',
            'listing_platform_dir': 'Win/',
            'archive_name': 'chrome-win32.zip',
            'archive_extract_dir': 'chrome-win32'
        },
        'win64': {
            'binary_name': 'chrome.exe',
            'listing_platform_dir': 'Win_x64/',
            'archive_name': 'chrome-win32.zip',
            'archive_extract_dir': 'chrome-win32'
        },
        'lacros64': {
            'binary_name': 'chrome',
            'listing_platform_dir': 'lacros64/',
            'archive_name': 'lacros.zip',
            'archive_extract_dir': 'chrome-lacros64'
        },
        'lacros-arm32': {
            'binary_name': 'chrome',
            'listing_platform_dir': 'lacros_arm/',
            'archive_name': 'lacros.zip',
            'archive_extract_dir': 'chrome-lacros-arm32'
        },
        'lacros-arm64': {
            'binary_name': 'chrome',
            'listing_platform_dir': 'lacros_arm64/',
            'archive_name': 'lacros.zip',
            'archive_extract_dir': 'chrome-lacros-arm64'
        }
    }
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

# Set only during initialization.
is_verbose = False


class BisectException(Exception):

  def __str__(self):
    return '[Bisect Exception]: %s\n' % self.args[0]


def RunGsutilCommand(args, can_fail=False, verbose=False):
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
      sys.exit(1)
    elif can_fail:
      return stderr
    else:
      raise Exception('Error running the gsutil command:\n%s\n%s' %
                      (args, stderr))
  return stdout


class PathContext(object):
  """A PathContext is used to carry the information used to construct URLs and
  paths when dealing with the storage server and archives."""

  def __init__(self, options, device=None):
    super(PathContext, self).__init__()
    # Store off the input parameters.
    self.platform = options.archive
    self.good_revision = options.good
    self.bad_revision = options.bad
    self.is_release = options.release_builds
    self.is_official = options.official_builds
    self.is_asan = options.asan
    self.signed = options.signed
    self.build_type = 'release'
    # Whether to cache and use the list of known revisions in a local file to
    # speed up the initialization of the script at the next run.
    self.use_local_cache = options.use_local_cache
    if options.asan:
      self.base_url = ASAN_BASE_URL
    elif options.blink:
      self.base_url = WEBKIT_BASE_URL
    else:
      self.base_url = CHROMIUM_BASE_URL

    self.apk = options.apk
    self.device = device

    # Dictionary which stores svn revision number as key and it's
    # corresponding git hash as value. This data is populated in
    # _FetchAndParse and used later in GetDownloadURL while downloading
    # the build.
    self.githash_svn_dict = {}
    # The name of the ZIP file in a revision directory on the server.
    self.archive_name = None


    # Locate the local checkout to speed up the script by using locally stored
    # metadata.
    abs_file_path = os.path.abspath(os.path.realpath(__file__))
    local_src_path = os.path.join(os.path.dirname(abs_file_path), '..')
    if abs_file_path.endswith(os.path.join('tools', 'bisect-builds.py')) and\
        os.path.exists(os.path.join(local_src_path, '.git')):
      self.local_src_path = os.path.normpath(local_src_path)
    else:
      self.local_src_path = None

    # Set some internal members:
    #   _listing_platform_dir = Directory that holds revisions. Ends with a '/'.
    #   _archive_extract_dir = Uncompressed directory in the archive_name file.
    #   _binary_name = The name of the executable to run.
    self.SetPathContextMembers(self.platform)

  def SetPathContextMembers(self, platform):
    if self.is_release:
      test_type = 'release'
      # Linux release archives changed their name during the M60 cycle, so older
      # builds won't be found in the current path.
      if platform == 'linux64':
        # For releases, the revision # is actually a "1.2.3.4"-style version.
        good_major = int(self.good_revision.split('.')[0])
        bad_major = int(self.bad_revision.split('.')[0])
        # The new path definitely doesn't exist before M57
        if min(good_major, bad_major) < 58:
          print('Linux release archives changed location during M58, '
                'and older builds are currently not supported by the script. '
                'If you really need to bisect pre-M58 builds, please contact '
                'trooper for assistance, otherwise please re-run '
                'with more recent revision values.')
          sys.exit(1)
        if min(good_major, bad_major) < 59:
          print('-----------------------------------------------------------\n'
                'WARNING: Linux release archives changed location during the '
                'M58 cycle, so this bisect might be be missing some builds. '
                'If you really need to bisect against all M58 builds, please '
                'contact trooper for assistance.\n'
                '-----------------------------------------------------------')
    elif self.is_official:
      test_type = 'official'
    else:
      test_type = 'snapshot'  # default test type
    path_members = PATH_CONTEXT[test_type].get(platform)
    if not path_members:
      raise BisectException(
          'Error: Bisecting on %s builds are only '
          'supported on these platforms: [%s].' %
          (test_type, '|'.join(PATH_CONTEXT[test_type].keys())))
    self._binary_name = path_members['binary_name']
    self._listing_platform_dir = path_members['listing_platform_dir']
    if self.is_release and 'android' in self.platform:
      self.archive_name = GetAndroidApkFilename(self)
    else:
      self.archive_name = path_members['archive_name']
    self._archive_extract_dir = path_members['archive_extract_dir']

  def GetASANPlatformDir(self):
    """ASAN builds are in directories like "linux-release", or have filenames
    like "asan-win32-release-277079.zip". This aligns to our platform names
    except in the case of Windows where they use "win32" instead of "win"."""
    if self.platform == 'win':
      return 'win32'
    else:
      return self.platform

  def GetListingURL(self, marker=None):
    """Returns the URL for a directory listing, with an optional marker."""
    marker_param = ''
    if marker:
      marker_param = '&marker=' + str(marker)
    if self.is_asan:
      prefix = '%s-%s' % (self.GetASANPlatformDir(), self.build_type)
      return self.base_url + '/?delimiter=&prefix=' + prefix + marker_param
    elif self.is_official:
      return (self.base_url + '/?prefix=' + self._listing_platform_dir +
              marker_param)
    else:
      return (self.base_url + '/?delimiter=/&prefix=' +
              self._listing_platform_dir + marker_param)

  def GetDownloadURL(self, revision):
    """Gets the download URL for a build archive of a specific revision."""
    archive_name = self.archive_name
    # At revision 591483, the names of two of the archives changed
    # due to: https://chromium-review.googlesource.com/#/q/1226086
    # See: http://crbug.com/789612
    # revision passed in can either be a cr commit position(int),
    # or a chrome version(str).
    if '.' not in str(revision) and revision >= 591483:
      if self.platform == 'chromeos':
        archive_name = 'chrome-chromeos.zip'
      elif self.platform in ('win', 'win64'):
        archive_name = 'chrome-win.zip'

    if self.is_asan:
      return '%s/%s-%s/%s-%d.zip' % (ASAN_BASE_URL,
                                     self.GetASANPlatformDir(), self.build_type,
                                     self.GetASANBaseName(), revision)
    if self.is_release:
      return '%s/%s/%s%s' % (self.GetReleaseBucket(), revision,
                             self._listing_platform_dir, archive_name)

    if self.is_official:
      return '%s/%s%s_%s.zip' % (PERF_BASE_URL, self._listing_platform_dir,
                                 self._archive_extract_dir, revision)
    else:
      if str(revision) in self.githash_svn_dict:
        revision = self.githash_svn_dict[str(revision)]
      return '%s/%s%s/%s' % (self.base_url, self._listing_platform_dir,
                             revision, archive_name)

  def GetLastChangeURL(self):
    """Returns a URL to the LAST_CHANGE file."""
    return self.base_url + '/' + self._listing_platform_dir + 'LAST_CHANGE'

  def GetASANBaseName(self):
    """Returns the base name of the ASAN zip file."""
    if 'linux' in self.platform:
      return 'asan-symbolized-%s-%s' % (self.GetASANPlatformDir(),
                                        self.build_type)
    else:
      return 'asan-%s-%s' % (self.GetASANPlatformDir(), self.build_type)

  def GetLaunchPath(self, revision):
    """Returns a relative path (presumably from the archive extraction location)
    that is used to run the executable."""
    if self.is_asan:
      extract_dir = '%s-%d' % (self.GetASANBaseName(), revision)
    elif self.is_official:
      extract_dir = '%s_%s' % (self._archive_extract_dir, revision)
    else:
      extract_dir = self._archive_extract_dir
      # At revision 591483, the names of two of the archives changed
      # due to: https://chromium-review.googlesource.com/#/q/1226086
      # See: http://crbug.com/789612
      if '.' not in str(revision) and revision >= 591483:
        if self.platform == 'chromeos':
          extract_dir = 'chrome-chromeos'
        elif self.platform in ('win', 'win64'):
          extract_dir = 'chrome-win'

    return os.path.join(extract_dir, self._binary_name)

  def ParseDirectoryIndex(self, last_known_rev):
    """Parses the Google Storage directory listing into a list of revision
    numbers."""

    def _GetMarkerForRev(revision):
      if self.is_asan:
        return '%s-%s/%s-%d.zip' % (
            self.GetASANPlatformDir(), self.build_type,
            self.GetASANBaseName(), revision)
      return '%s%d' % (self._listing_platform_dir, revision)

    def _FetchAndParse(url):
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
      githash_svn_dict = {}
      if self.is_asan:
        asan_regex = re.compile(r'.*%s-(\d+)\.zip$' % (self.GetASANBaseName()))
        # Non ASAN builds are in a <revision> directory. The ASAN builds are
        # flat
        all_prefixes = document.findall(namespace + 'Contents/' +
                                        namespace + 'Key')
        for prefix in all_prefixes:
          m = asan_regex.match(prefix.text)
          if m:
            try:
              revisions.append(int(m.group(1)))
            except ValueError:
              pass
      else:
        all_prefixes = document.findall(namespace + 'CommonPrefixes/' +
                                        namespace + 'Prefix')
        # The <Prefix> nodes have content of the form of
        # |_listing_platform_dir/revision/|. Strip off the platform dir and the
        # trailing slash to just have a number.go
        for prefix in all_prefixes:
          revnum = prefix.text[prefix_len:-1]
          try:
            revnum = int(revnum)
            revisions.append(revnum)
          # Notes:
          # Ignore hash in chromium-browser-snapshots as they are invalid
          # Resulting in 404 error in fetching pages:
          # https://chromium.googlesource.com/chromium/src/+/[rev_hash]
          except ValueError:
            pass
      return (revisions, next_marker, githash_svn_dict)

    # Fetch the first list of revisions.
    if last_known_rev:
      revisions = []
      # Optimization: Start paging at the last known revision (local cache).
      next_marker = _GetMarkerForRev(last_known_rev)
      # Optimization: Stop paging at the last known revision (remote).
      last_change_rev = GetChromiumRevision(self, self.GetLastChangeURL())
      if last_known_rev == last_change_rev:
        return []
    else:
      (revisions, next_marker, new_dict) = _FetchAndParse(self.GetListingURL())
      self.githash_svn_dict.update(new_dict)
      last_change_rev = None

    # If the result list was truncated, refetch with the next marker. Do this
    # until an entire directory listing is done.
    while next_marker:
      sys.stdout.write('\rFetching revisions at marker %s' % next_marker)
      sys.stdout.flush()

      next_url = self.GetListingURL(next_marker)
      (new_revisions, next_marker, new_dict) = _FetchAndParse(next_url)
      revisions.extend(new_revisions)
      self.githash_svn_dict.update(new_dict)
      if last_change_rev and last_change_rev in new_revisions:
        break
    sys.stdout.write('\r')
    sys.stdout.flush()
    return revisions

  def _GetSVNRevisionFromGitHashWithoutGitCheckout(self, git_sha1, depot):
    json_url = GITHASH_TO_SVN_URL[depot] % git_sha1
    response = urllib.request.urlopen(json_url)
    if response.getcode() == 200:
      try:
        data = json.loads(response.read()[4:])
      except ValueError:
        print('ValueError for JSON URL: %s' % json_url)
        raise ValueError
    else:
      raise ValueError
    if 'message' in data:
      message = data['message'].split('\n')
      message = [line for line in message if line.strip()]
      search_pattern = re.compile(SEARCH_PATTERN[depot])
      result = search_pattern.search(message[len(message)-1])
      if result:
        return result.group(1)
      else:
        if depot == 'chromium':
          result = re.search(CHROMIUM_SEARCH_PATTERN_OLD,
                             message[len(message)-1])
          if result:
            return result.group(1)
    print('Failed to get svn revision number for %s' % git_sha1)
    raise ValueError

  def _GetSVNRevisionFromGitHashFromGitCheckout(self, git_sha1, depot):
    def _RunGit(command, path):
      command = ['git'] + command
      shell = sys.platform.startswith('win')
      proc = subprocess.Popen(command, shell=shell, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, cwd=path)
      (output, _) = proc.communicate()
      return (output, proc.returncode)

    path = self.local_src_path
    if depot == 'blink':
      path = os.path.join(self.local_src_path, 'third_party', 'WebKit')
    revision = None
    try:
      command = ['svn', 'find-rev', git_sha1]
      (git_output, return_code) = _RunGit(command, path)
      if not return_code:
        revision = git_output.strip('\n')
    except ValueError:
      pass
    if not revision:
      command = ['log', '-n1', '--format=%s', git_sha1]
      (git_output, return_code) = _RunGit(command, path)
      if not return_code:
        revision = re.match('SVN changes up to revision ([0-9]+)', git_output)
        revision = revision.group(1) if revision else None
    if revision:
      return revision
    raise ValueError

  def GetSVNRevisionFromGitHash(self, git_sha1, depot='chromium'):
    if not self.local_src_path:
      return self._GetSVNRevisionFromGitHashWithoutGitCheckout(git_sha1, depot)
    else:
      return self._GetSVNRevisionFromGitHashFromGitCheckout(git_sha1, depot)

  def GetReleaseBucket(self):
    if 'android' in self.platform:
      if self.signed:
        return ANDROID_RELEASE_BASE_URL_SIGNED
      else:
        return ANDROID_RELEASE_BASE_URL
    return RELEASE_BASE_URL

  def GetRevList(self, archive):
    """Gets the list of revision numbers between self.good_revision and
    self.bad_revision."""

    cache = {}
    # The cache is stored in the same directory as bisect-builds.py
    cache_filename = os.path.join(
        os.path.abspath(os.path.dirname(__file__)),
        '.bisect-builds-cache.json')
    cache_dict_key = self.GetListingURL()

    def _LoadBucketFromCache():
      if self.use_local_cache:
        try:
          with open(cache_filename) as cache_file:
            for (key, value) in json.load(cache_file).items():
              cache[key] = value
            revisions = cache.get(cache_dict_key, [])
            githash_svn_dict = cache.get('githash_svn_dict', {})
            if revisions:
              print('Loaded revisions %d-%d from %s' %
                    (revisions[0], revisions[-1], cache_filename))
            return (revisions, githash_svn_dict)
        except (EnvironmentError, ValueError):
          pass
      return ([], {})

    def _SaveBucketToCache():
      """Save the list of revisions and the git-svn mappings to a file.
      The list of revisions is assumed to be sorted."""
      if self.use_local_cache:
        cache[cache_dict_key] = revlist_all
        cache['githash_svn_dict'] = self.githash_svn_dict
        try:
          with open(cache_filename, 'w') as cache_file:
            json.dump(cache, cache_file)
          print('Saved revisions %d-%d to %s' %
                (revlist_all[0], revlist_all[-1], cache_filename))
        except EnvironmentError:
          pass

    # Download the revlist and filter for just the range between good and bad.
    minrev = min(self.good_revision, self.bad_revision)
    maxrev = max(self.good_revision, self.bad_revision)

    (revlist_all, self.githash_svn_dict) = _LoadBucketFromCache()
    last_known_rev = revlist_all[-1] if revlist_all else 0
    if last_known_rev < maxrev:
      if self.is_official:
        revlist_all.extend(list(map(int, self.GetPerCLRevList())))
      else:
        revlist_all.extend(
            list(map(int, self.ParseDirectoryIndex(last_known_rev))))
      revlist_all.sort()
      _SaveBucketToCache()

    revlist = [x for x in revlist_all if x >= int(minrev) and x <= int(maxrev)]
    if len(revlist) < 2:  # Don't have enough builds to bisect.
      last_known_rev = revlist_all[-1] if revlist_all else 0
      first_known_rev = revlist_all[0] if revlist_all else 0
      # Check for specifying a number before the available range.
      if maxrev < first_known_rev:
        msg = (
            'First available bisect revision for %s is %d. Be sure to specify '
            'revision numbers, not branch numbers.' %
            (archive, first_known_rev))
        raise (RuntimeError(msg))

      # Check for specifying a number beyond the available range.
      if maxrev > last_known_rev:
        # Check for the special case of linux where bisect builds stopped at
        # revision 382086, around March 2016.
        if archive == 'linux':
          msg = ('Last available bisect revision for %s is %d. Try linux64 '
                 'instead.' % (archive, last_known_rev))
        else:
          msg = ('Last available bisect revision for %s is %d. Try a different '
                 'good/bad range.' % (archive, last_known_rev))
        raise (RuntimeError(msg))

      # Otherwise give a generic message.
      msg = 'We don\'t have enough builds to bisect. revlist: %s' % revlist
      raise RuntimeError(msg)
    # Set good and bad revisions to be legit revisions.
    if revlist:
      if self.good_revision < self.bad_revision:
        self.good_revision = revlist[0]
        self.bad_revision = revlist[-1]
      else:
        self.bad_revision = revlist[0]
        self.good_revision = revlist[-1]

      # Fix chromium rev so that the deps blink revision matches REVISIONS file.
      if self.base_url == WEBKIT_BASE_URL:
        revlist_all.sort()
        self.good_revision = FixChromiumRevForBlink(revlist,
                                                    revlist_all,
                                                    self,
                                                    self.good_revision)
        self.bad_revision = FixChromiumRevForBlink(revlist,
                                                   revlist_all,
                                                   self,
                                                   self.bad_revision)
    return revlist

  def GsutilExists(self, query):
    output = RunGsutilCommand(['stat', query], can_fail=True)
    if output.startswith(query):
      return True
    elif 'No URLs matched' in output:
      return False
    else:
      raise Exception('Error running the gsutil command: %s' % output)

  def GsutilList(self, query):
    # Get a directory listing with file sizes. Typical output looks like:
    #         7  2023-11-27T21:08:36Z  gs://.../LAST_CHANGE
    # 144486938  2023-03-07T14:41:25Z  gs://.../full-build-win32_1113893.zip
    # TOTAL: 114167 objects, 15913845813421 bytes (14.47 TiB)
    # This lets us ignore empty .zip files that will otherwise cause errors.
    stdout = RunGsutilCommand(['ls', '-l', query])
    # Trim off the summary line that only happens with -l
    temp = stdout.splitlines()[:-1]
    lines = []
    for line in temp:
      parts = line.split()
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
    results = [url[len(query):].strip('/') for url in lines]
    return results

  def GetPerCLRevList(self):
    """ Gets the list of revision numbers between self.good_revision and
    self.bad_revision from a perf build."""
    minrev = min(self.good_revision, self.bad_revision)
    maxrev = max(self.good_revision, self.bad_revision)
    perf_bucket = '%s/%s' % (PERF_BASE_URL, self._listing_platform_dir)
    revision_re = re.compile(r'%s_(\d+)\.zip' % (self._archive_extract_dir))
    revision_files = self.GsutilList(perf_bucket)
    revision_numbers = []

    for revision_file in revision_files:
      revision_num = re.match(revision_re, revision_file)
      if revision_num:
        revision_numbers.append(int(revision_num.groups()[0]))
    final_list = []
    for revision_number in sorted(revision_numbers):
      if revision_number > maxrev:
        break
      if revision_number < minrev:
        continue
      final_list.append(revision_number)
    return final_list

  def GetPerfCLRevListFromBackup(self):
    """Checks for builds in older GS folders."""
    revlist = []
    # Lacros doesn't have the old backup builds.
    if 'lacros' in self.platform:
      return revlist
    for f in OFFICIAL_BACKUP_BUILDS[self.platform]['listing_platform_dir']:
      print('Checking "%s" directory for build archives...' % f)
      self._listing_platform_dir = f
      revlist = self.GetPerCLRevList()
      if len(revlist) >= 2:
        break

    return revlist

  def GetReleaseBuildsList(self):
    """Gets the list of release build numbers between self.good_revision and
    self.bad_revision."""
    # Download the revlist and filter for just the range between good and bad.
    minrev = min(self.good_revision, self.bad_revision)
    maxrev = max(self.good_revision, self.bad_revision)
    # Check against a version number that is many years in the future in order
    # to detect when a revision number is passed instead of a version number.
    if maxrev > LooseVersion('2000'):
      raise BisectException('Max version of %s is too high. Be sure to use a '
                            'version number, not revision number with release '
                            'builds.' % maxrev)
    build_numbers = self.GsutilList(self.GetReleaseBucket())
    revision_re = re.compile(r'(\d+\.\d\.\d{4}\.\d+)')
    build_numbers = [b for b in build_numbers if revision_re.search(b)]
    final_list = []
    parsed_build_numbers = [LooseVersion(x) for x in build_numbers]
    parsed_build_numbers = sorted(parsed_build_numbers)
    start = bisect.bisect_left(parsed_build_numbers, minrev)
    end = bisect.bisect_right(parsed_build_numbers, maxrev)
    # Each call to GsutilExists takes about one second so give an estimate of
    # the wait time.
    build_count = end - start
    print('Checking the existence of %d builds. This will take about %.1f '
          'minutes' % (build_count, build_count / 60.0))
    for build_number in parsed_build_numbers[start:end]:
      path = (self.GetReleaseBucket() + '/' + str(build_number) + '/' +
              self._listing_platform_dir + self.archive_name)
      if self.GsutilExists(path):
        final_list.append(str(build_number))
    print('Found %d builds' % len(final_list))
    return final_list


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


def FetchRevision(context, rev, filename, quit_event=None, progress_event=None):
  """Downloads and unzips revision |rev|.
  @param context A PathContext instance.
  @param rev The Chromium revision number/tag to download.
  @param filename The destination for the downloaded file.
  @param quit_event A threading.Event which will be set by the main thread to
                    indicate that the download should be aborted.
  @param progress_event A threading.Event which will be set by the main thread
                    to indicate that the progress of the download should be
                    displayed.
  """
  def ReportHook(blocknum, blocksize, totalsize):
    if quit_event and quit_event.is_set():
      raise RuntimeError('Aborting download of revision %s' % str(rev))
    if progress_event and progress_event.is_set():
      size = blocknum * blocksize
      if totalsize == -1:  # Total size not known.
        progress = 'Received %d bytes' % size
      else:
        size = min(totalsize, size)
        progress = 'Received %d of %d bytes, %.2f%%' % (
            size, totalsize, 100.0 * size / totalsize)
      # Send a \r to let all progress messages use just one line of output.
      sys.stdout.write('\r' + progress)
      sys.stdout.flush()
  download_url = context.GetDownloadURL(rev)
  try:
    if download_url.startswith('gs'):
      gsutil_download(download_url, filename)
    else:
      urllib.request.urlretrieve(download_url, filename, ReportHook)
      if progress_event and progress_event.is_set():
        print()
  except RuntimeError:
    pass


def GetAndroidApkFilename(context):
  sdk = context.device.build_version_sdk
  if 'webview' in context.apk:
    return WEBVIEW_APK_FILENAMES[context.apk]
  # Need these logic to bisect very old build. Release binaries are stored
  # forever and occasionally there are requests to bisect issues introduced
  # in very old versions.
  elif sdk < version_codes.LOLLIPOP:
    return CHROME_APK_FILENAMES[context.apk]
  elif sdk < version_codes.NOUGAT:
    return CHROME_MODERN_APK_FILENAMES[context.apk]
  return MONOCHROME_APK_FILENAMES[context.apk]


def RunRevisionForAndroid(context, revision, zip_file):
  """Installs apk and launches chrome for android bisect."""

  # For release, we directly download the apk file from gcs.
  # For non-release, we download a zip file first, then un-zip the file
  # to a temporary folder and locate the apk file.
  if context.is_release:
    InstallonAndroid(context.device, zip_file)
    LaunchOnAndroid(context.device, context.apk)
    return (0, sys.stdout, sys.stderr)

  try:
    tempdir = tempfile.mkdtemp(prefix='bisect_tmp')
    UnzipFilenameToDir(zip_file, tempdir)

    apk_dir = os.path.join(tempdir, context._archive_extract_dir, 'apks')
    apk_path = os.path.join(apk_dir, GetAndroidApkFilename(context))
    if not os.path.exists(apk_path):
      print('%s does not exist.' % apk_path)
      if os.path.exists(apk_dir):
        print('Are you using the correct apk? The list of available apks:')
        apk_files = [f for f in os.listdir(apk_dir) if f.endswith('.apk')]
        print(apk_files)
      exit(1)
    InstallonAndroid(context.device, apk_path)
    LaunchOnAndroid(context.device, context.apk)
  finally:
    try:
      shutil.rmtree(tempdir, True)
    except Exception:
      pass
  return (0, sys.stdout, sys.stderr)


def InstallRevisionForLacros(context, zip_file):
  """Install revision on cros device."""

  try:
    tempdir = tempfile.mkdtemp(prefix='bisect_tmp')
    UnzipFilenameToDir(zip_file, tempdir)
    if context.is_official:
      tempdir = os.path.join(tempdir, context._archive_extract_dir)

    cmdline = [
        context.deploy_chrome_path, '--build-dir=' + tempdir,
        '--device=' + context.device, '--nostrip', '--lacros', '--reset-lacros'
    ]
    print('Lacros deploy command:\n')
    print(' '.join(cmdline))
    subproc = subprocess.Popen(cmdline)
    (stdout, stderr) = subproc.communicate()
    if subproc.returncode == 0:
      print('deploy succeeded!')
      print('You may now click Lacros icon on DUT to start testing.')
    else:
      print('deploy failed!')
    return (subproc.returncode, stdout, stderr)
  finally:
    try:
      shutil.rmtree(tempdir, True)
    except Exception:
      pass


def RunRevision(context, revision, zip_file, profile, num_runs, command, args):
  """Given a zipped revision, unzip it and run the test."""
  print('Trying revision %s...' % str(revision))
  if context.platform in ['android-arm', 'android-arm64']:
    return RunRevisionForAndroid(context, revision, zip_file)

  if context.platform in ['lacros64', 'lacros-arm32', 'lacros-arm64']:
    return InstallRevisionForLacros(context, zip_file)

  # Create a temp directory and unzip the revision into it.
  cwd = os.getcwd()
  tempdir = tempfile.mkdtemp(prefix='bisect_tmp')
  # On Windows 10, file system needs to be readable from App Container.
  if sys.platform == 'win32' and platform.release() == '10':
    icacls_cmd = ['icacls', tempdir, '/grant', '*S-1-15-2-2:(OI)(CI)(RX)']
    proc = subprocess.Popen(icacls_cmd,
                            bufsize=0,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    proc.communicate()

  UnzipFilenameToDir(zip_file, tempdir)

  # Special case for perf builds. The directory can be either versioned
  # or unversioned. For example, full-build-linux directory will be converted to
  # full-build-linux_<revision_number> directoy.
  if context.is_official:
    unversioned_archive = os.path.join(tempdir, context._archive_extract_dir)
    if os.path.isdir(unversioned_archive):
      versioned_archive = os.path.join(
          tempdir, '%s_%s' % (context._archive_extract_dir, revision))
      # On Windows this renaming can transiently fail - because of
      # antivirus software, even in monitoring mode? - so retry it up
      # to a few times. It seems it can fail for at least 10 seconds
      # in a row on developers' machines.
      retries = 20
      succeeded = False
      while not succeeded:
        try:
          os.rename(unversioned_archive, versioned_archive)
          succeeded = True
        except Exception as e:
          retries -= 1
          if retries == 0:
            print('Failed to rename: ' + unversioned_archive)
            print('              to: ' + versioned_archive)
            raise e
          time.sleep(1)
  # Hack: Chrome OS archives are missing icudtl.dat; try to copy it from
  # the local directory.
  if context.platform == 'chromeos' and revision < 591483:
    icudtl_path = 'third_party/icu/common/icudtl.dat'
    if not os.access(icudtl_path, os.F_OK):
      print('Couldn\'t find: ' + icudtl_path)
      sys.exit()
    os.system('cp %s %s/chrome-linux/' % (icudtl_path, tempdir))

  os.chdir(tempdir)

  # Run the build as many times as specified.
  testargs = ['--user-data-dir=%s' % profile] + args
  # The sandbox must be run as root on release Chrome, so bypass it.
  if ((context.is_release) and context.platform.startswith('linux')):
    testargs.append('--no-sandbox')

  runcommand = []
  for token in shlex.split(command):
    if token == '%a':
      runcommand.extend(testargs)
    else:
      runcommand.append(
          token.replace('%p', os.path.abspath(
              context.GetLaunchPath(revision))).replace('%s',
                                                        ' '.join(testargs)))

  results = []
  if is_verbose:
    print(('Running ' + str(runcommand)))

  result = None
  try:
    for _ in range(num_runs):
      use_shell = ('android' in context.platform
                   or 'webview' in context.platform)
      subproc = subprocess.Popen(runcommand,
                                 shell=use_shell,
                                 bufsize=-1,
                                 stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE)
      (stdout, stderr) = subproc.communicate()
      result = (subproc.returncode, stdout, stderr)
      if subproc.returncode:
        break
    return result
  finally:
    os.chdir(cwd)
    try:
      shutil.rmtree(tempdir, True)
    except Exception:
      pass


# The arguments release_builds, status, stdout and stderr are unused.
# They are present here because this function is passed to Bisect which then
# calls it with 5 arguments.
# pylint: disable=W0613
def AskIsGoodBuild(rev, release_builds, exit_status, stdout, stderr):
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


def IsGoodASANBuild(rev, release_builds, exit_status, stdout, stderr):
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
  return AskIsGoodBuild(rev, release_builds, exit_status, stdout, stderr)


def DidCommandSucceed(rev, release_builds, exit_status, stdout, stderr):
  if exit_status:
    print('Bad revision: %s' % rev)
    return 'b'
  else:
    print('Good revision: %s' % rev)
    return 'g'


class DownloadJob(object):
  """DownloadJob represents a task to download a given Chromium revision."""

  def __init__(self, context, name, rev, zip_file):
    super(DownloadJob, self).__init__()
    # Store off the input parameters.
    self.context = context
    self.name = name
    self.rev = rev
    self.zip_file = zip_file
    self.quit_event = threading.Event()
    self.progress_event = threading.Event()
    self.thread = None

  def Start(self):
    """Starts the download."""
    fetchargs = (self.context,
                 self.rev,
                 self.zip_file,
                 self.quit_event,
                 self.progress_event)
    self.thread = threading.Thread(target=FetchRevision,
                                   name=self.name,
                                   args=fetchargs)
    self.thread.start()

  def Stop(self):
    """Stops the download which must have been started previously."""
    assert self.thread, 'DownloadJob must be started before Stop is called.'
    self.quit_event.set()
    self.thread.join()
    try:
      os.unlink(self.zip_file)
    except FileNotFoundError:
      # Handle missing archives.
      pass

  def WaitFor(self):
    """Prints a message and waits for the download to complete. The download
    must have been started previously."""
    assert self.thread, 'DownloadJob must be started before WaitFor is called.'
    print('Downloading revision %s...' % str(self.rev))
    self.progress_event.set()  # Display progress of download.
    try:
      while self.thread.is_alive():
        # The parameter to join is needed to keep the main thread responsive to
        # signals. Without it, the program will not respond to interruptions.
        self.thread.join(1)
    except (KeyboardInterrupt, SystemExit):
      self.Stop()
      raise


def VerifyEndpoint(fetch, context, rev, profile, num_runs, command, try_args,
                   evaluate, expected_answer):
  fetch.WaitFor()
  try:
    (exit_status, stdout, stderr) = RunRevision(context, rev, fetch.zip_file,
                                                profile, num_runs, command,
                                                try_args)
  except Exception as e:
    if not isinstance(e, SystemExit):
      traceback.print_exc(file=sys.stderr)
    exit_status = None
    stdout = None
    stderr = None
  if (evaluate(rev, context.is_release, exit_status, stdout, stderr)
      != expected_answer):
    print('Unexpected result at a range boundary! Your range is not correct.')
    raise SystemExit


def Bisect(context,
           num_runs=1,
           command='%p %a',
           try_args=(),
           profile='profile',
           evaluate=AskIsGoodBuild,
           verify_range=False,
           archive=None):
  """Runs a binary search on to determine the last known good revision.

    Args:
      context: PathContext object initialized with user provided parameters.
      num_runs: Number of times to run each build for asking good/bad.
      try_args: A tuple of arguments to pass to the test application.
      profile: The name of the user profile to run with.
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
  good_rev = context.good_revision
  bad_rev = context.bad_revision
  cwd = os.getcwd()

  print('Downloading list of known revisions.', end=' ')
  print('If the range is large, this can take several minutes...')
  if not context.use_local_cache and not context.is_release:
    print('(use --use-local-cache to cache and re-use the list of revisions)')
  else:
    print()
  _GetDownloadPath = lambda rev: os.path.join(cwd,
      '%s-%s' % (str(rev), context.archive_name))
  if context.is_release:
    revlist = context.GetReleaseBuildsList()
  elif context.is_official:
    revlist = context.GetRevList(archive)
  else:
    revlist = context.GetRevList(archive)

  # Get a list of revisions to bisect across.
  if len(revlist) < 2:  # Don't have enough builds to bisect.
    msg = 'We don\'t have enough builds to bisect. revlist: %s' % revlist
    raise RuntimeError(msg)

  # Figure out our bookends and first pivot point; fetch the pivot revision.
  minrev = 0
  maxrev = len(revlist) - 1
  pivot = maxrev // 2
  rev = revlist[pivot]
  fetch = DownloadJob(context, 'initial_fetch', rev, _GetDownloadPath(rev))
  fetch.Start()

  if verify_range:
    minrev_fetch = DownloadJob(
        context, 'minrev_fetch', revlist[minrev],
        _GetDownloadPath(revlist[minrev]))
    maxrev_fetch = DownloadJob(
        context, 'maxrev_fetch', revlist[maxrev],
        _GetDownloadPath(revlist[maxrev]))
    minrev_fetch.Start()
    maxrev_fetch.Start()
    try:
      VerifyEndpoint(minrev_fetch, context, revlist[minrev], profile, num_runs,
          command, try_args, evaluate, 'b' if bad_rev < good_rev else 'g')
      VerifyEndpoint(maxrev_fetch, context, revlist[maxrev], profile, num_runs,
          command, try_args, evaluate, 'g' if bad_rev < good_rev else 'b')
    except (KeyboardInterrupt, SystemExit):
      print('Cleaning up...')
      fetch.Stop()
      sys.exit(0)
    finally:
      minrev_fetch.Stop()
      maxrev_fetch.Stop()

  fetch.WaitFor()

  # Binary search time!
  prefetch_revisions = True
  while fetch and fetch.zip_file and maxrev - minrev > 1:
    if bad_rev < good_rev:
      min_str, max_str = 'bad', 'good'
    else:
      min_str, max_str = 'good', 'bad'
    print('You have about %d more steps left.' %
          ((maxrev - minrev).bit_length() - 1))
    print('Bisecting range [%s (%s), %s (%s)].' %
          (revlist[minrev], min_str, revlist[maxrev], max_str))

    # Pre-fetch next two possible pivots
    #   - down_pivot is the next revision to check if the current revision turns
    #     out to be bad.
    #   - up_pivot is the next revision to check if the current revision turns
    #     out to be good.
    down_pivot = int((pivot - minrev) / 2) + minrev
    down_fetch = None
    if prefetch_revisions:
      if down_pivot != pivot and down_pivot != minrev:
        down_rev = revlist[down_pivot]
        down_fetch = DownloadJob(context, 'down_fetch', down_rev,
                                 _GetDownloadPath(down_rev))
        down_fetch.Start()

    up_pivot = int((maxrev - pivot) / 2) + pivot
    if prefetch_revisions:
      up_fetch = None
      if up_pivot != pivot and up_pivot != maxrev:
        up_rev = revlist[up_pivot]
        up_fetch = DownloadJob(context, 'up_fetch', up_rev,
                               _GetDownloadPath(up_rev))
        up_fetch.Start()

    # Run test on the pivot revision.
    exit_status = None
    stdout = None
    stderr = None
    try:
      (exit_status, stdout, stderr) = RunRevision(
          context, rev, fetch.zip_file, profile, num_runs, command, try_args)
    except SystemExit:
      raise
    except Exception:
      traceback.print_exc(file=sys.stderr)

    # Call the evaluate function to see if the current revision is good or bad.
    # On that basis, kill one of the background downloads and complete the
    # other, as described in the comments above.
    try:
      answer = evaluate(rev, context.is_release, exit_status, stdout, stderr)
      prefetch_revisions = True
      if ((answer == 'g' and good_rev < bad_rev)
          or (answer == 'b' and bad_rev < good_rev)):
        fetch.Stop()
        minrev = pivot
        if down_fetch:
          down_fetch.Stop()  # Kill the download of the older revision.
          fetch = None
        if up_fetch:
          up_fetch.WaitFor()
          pivot = up_pivot
          fetch = up_fetch
      elif ((answer == 'b' and good_rev < bad_rev)
            or (answer == 'g' and bad_rev < good_rev)):
        fetch.Stop()
        maxrev = pivot
        if up_fetch:
          up_fetch.Stop()  # Kill the download of the newer revision.
          fetch = None
        if down_fetch:
          down_fetch.WaitFor()
          pivot = down_pivot
          fetch = down_fetch
      elif answer == 'r':
        # Don't redundantly prefetch.
        prefetch_revisions = False
      elif answer == 'u':
        # Nuke the revision from the revlist and choose a new pivot.
        fetch.Stop()
        revlist.pop(pivot)
        maxrev -= 1  # Assumes maxrev >= pivot.

        if maxrev - minrev > 1:
          # Alternate between using down_pivot or up_pivot for the new pivot
          # point, without affecting the range. Do this instead of setting the
          # pivot to the midpoint of the new range because adjacent revisions
          # are likely affected by the same issue that caused the (u)nknown
          # response.
          if up_fetch and down_fetch:
            fetch = [up_fetch, down_fetch][len(revlist) % 2]
          elif up_fetch:
            fetch = up_fetch
          else:
            fetch = down_fetch
          fetch.WaitFor()
          if fetch == up_fetch:
            pivot = up_pivot - 1  # Subtracts 1 because revlist was resized.
          else:
            pivot = down_pivot

        if down_fetch and fetch != down_fetch:
          down_fetch.Stop()
        if up_fetch and fetch != up_fetch:
          up_fetch.Stop()
      else:
        assert False, 'Unexpected return value from evaluate(): ' + answer
    except (KeyboardInterrupt, SystemExit):
      print('Cleaning up...')
      for f in [_GetDownloadPath(rev),
                _GetDownloadPath(revlist[down_pivot]),
                _GetDownloadPath(revlist[up_pivot])]:
        try:
          os.unlink(f)
        except OSError:
          pass
      sys.exit(0)

    rev = revlist[pivot]

  return (revlist[minrev], revlist[maxrev], context)


def GetBlinkDEPSRevisionForChromiumRevision(self, rev):
  """Returns the blink revision that was in REVISIONS file at
  chromium revision |rev|."""

  def _GetBlinkRev(url, blink_re):
    m = blink_re.search(url.read())
    url.close()
    if m:
      return m.group(1)

  url = urllib.request.urlopen(DEPS_FILE_OLD % rev)
  if url.getcode() == 200:
    # . doesn't match newlines without re.DOTALL, so this is safe.
    blink_re = re.compile(r'webkit_revision\D*(\d+)')
    return int(_GetBlinkRev(url, blink_re))
  else:
    url = urllib.request.urlopen(DEPS_FILE_NEW % GetGitHashFromSVNRevision(rev))
    if url.getcode() == 200:
      blink_re = re.compile(r'webkit_revision\D*\d+;\D*\d+;(\w+)')
      blink_git_sha = _GetBlinkRev(url, blink_re)
      return self.GetSVNRevisionFromGitHash(blink_git_sha, 'blink')
  raise Exception('Could not get Blink revision for Chromium rev %d' % rev)


def GetBlinkRevisionForChromiumRevision(context, rev):
  """Returns the blink revision that was in REVISIONS file at
  chromium revision |rev|."""
  def _IsRevisionNumber(revision):
    if isinstance(revision, int):
      return True
    else:
      return revision.isdigit()
  if str(rev) in context.githash_svn_dict:
    rev = context.githash_svn_dict[str(rev)]
  file_url = '%s/%s%s/REVISIONS' % (context.base_url,
                                    context._listing_platform_dir, rev)
  url = urllib.request.urlopen(file_url)
  if url.getcode() == 200:
    try:
      data = json.loads(url.read())
    except ValueError:
      print('ValueError for JSON URL: %s' % file_url)
      raise ValueError
  else:
    raise ValueError
  url.close()
  if 'webkit_revision' in data:
    blink_rev = data['webkit_revision']
    if not _IsRevisionNumber(blink_rev):
      blink_rev = int(context.GetSVNRevisionFromGitHash(blink_rev, 'blink'))
    return blink_rev
  else:
    raise Exception('Could not get blink revision for cr rev %d' % rev)


def FixChromiumRevForBlink(revisions_final, revisions, self, rev):
  """Returns the chromium revision that has the correct blink revision
  for blink bisect, DEPS and REVISIONS file might not match since
  blink snapshots point to tip of tree blink.
  Note: The revisions_final variable might get modified to include
  additional revisions."""
  blink_deps_rev = GetBlinkDEPSRevisionForChromiumRevision(self, rev)

  while (GetBlinkRevisionForChromiumRevision(self, rev) > blink_deps_rev):
    idx = revisions.index(rev)
    if idx > 0:
      rev = revisions[idx-1]
      if rev not in revisions_final:
        revisions_final.insert(0, rev)

  revisions_final.sort()
  return rev


def GetChromiumRevision(context, url):
  """Returns the chromium revision read from given URL."""
  try:
    # Location of the latest build revision number
    latest_revision = urllib.request.urlopen(url).read()
    if latest_revision.isdigit():
      return int(latest_revision)
    return context.GetSVNRevisionFromGitHash(latest_revision)
  except Exception:
    print('Could not determine latest revision. This could be bad...')
    return 999999999


def FetchJsonFromURL(url):
  """Returns JSON data from the given URL"""
  url = urllib.request.urlopen(url)
  # Allow retry for 3 times for unexpected network error
  for i in range(3):
    if url.getcode() == 200:
      data = json.loads(url.read())
      return data
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


def GetRevisionFromVersion(version):
  """Returns Base Commit Position from a version number"""
  chromiumdash_url = VERSION_INFO_URL % str(version)
  data = FetchJsonFromURL(chromiumdash_url)
  if data and 'chromium_main_branch_position' in data:
    return data['chromium_main_branch_position']
  print('Something went wrong. The data we got from chromiumdash:\n%s' % data)
  return None


def CheckDepotToolsInPath():
  delimiter = ';' if sys.platform.startswith('win') else ':'
  path_list = os.environ['PATH'].split(delimiter)
  for path in path_list:
    if path.rstrip(os.path.sep).endswith('depot_tools'):
      return path
  return None


def SetupEnvironment(options):
  global is_verbose

  # Release and Official builds bisect requires "gsutil" inorder to
  # List and Download binaries.
  # Check if depot_tools is installed and path is set.
  gsutil_path = CheckDepotToolsInPath()
  if ((options.release_builds or options.official_builds) and not gsutil_path):
    raise BisectException(
        'Looks like depot_tools is not installed.\n'
        'Follow the instructions in this document '
        'http://dev.chromium.org/developers/how-tos/install-depot-tools '
        'to install depot_tools and then try again.')

  global GSUTILS_PATH
  GSUTILS_PATH = os.path.join(gsutil_path, 'gsutil.py')

  # Catapult repo is required for Android bisect,
  # Update Catapult repo if it exists otherwise checkout repo.
  if options.archive in ['android-arm', 'android-arm64']:
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


def InstallonAndroid(device, apk_path):
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
  usage = """%prog [options] [-- chromium-options]

Performs binary search on the chrome binaries to find a minimal range of
revisions where a behavior change happened.
The behaviors are described as "good" and "bad". It is NOT assumed that the
behavior of the later revision is the bad one.

Revision numbers should use:
  a) Release versions: (e.g. 1.0.1000.0) for release builds. (-r)
  b) Commit Positions: (e.g. 123456) for chromium builds, from trunk.
        Use chromium_main_branch_position from
        https://chromiumdash.appspot.com/fetch_version?version=<chrome_version>
        Please Note: Chrome's about: build number and chromiumdash branch
        revision are incorrect, they are from branches.

Tip: add "-- --no-first-run" to bypass the first run prompts.
"""

  parser = optparse.OptionParser(usage=usage)
  # Strangely, the default help output doesn't include the choice list.
  choices = [
      'android-arm', 'android-arm64', 'mac', 'mac64', 'mac-arm', 'win',
      'win-clang', 'win64', 'win64-clang', 'linux64', 'linux-arm', 'chromeos',
      'lacros64', 'lacros-arm32', 'lacros-arm64'
  ]
  parser.add_option('-a',
                    '--archive',
                    choices=choices,
                    help='The buildbot archive to bisect [%s].' %
                    '|'.join(choices))
  parser.add_option('-r',
                    action='store_true',
                    dest='release_builds',
                    help='Bisect across release Chrome builds (internal '
                    'only) instead of Chromium archives.')
  parser.add_option('-o',
                    action='store_true',
                    dest='official_builds',
                    help='Bisect across continuous perf officialChrome builds '
                    '(internal only) instead of Chromium archives. '
                    'With this flag, you can provide either commit '
                    'position numbers (for example, 397000) or '
                    'version numbers (for example, 53.0.2754.0 '
                    'as good and bad revisions.')
  parser.add_option('-b',
                    '--bad',
                    type='str',
                    help='A bad revision to start bisection. '
                    'May be earlier or later than the good revision. '
                    'Default is HEAD.')
  parser.add_option('-g',
                    '--good',
                    type='str',
                    help='A good revision to start bisection. ' +
                    'May be earlier or later than the bad revision. ' +
                    'Default is 0.')
  parser.add_option('-p',
                    '--profile',
                    '--user-data-dir',
                    type='str',
                    default='profile',
                    help='Profile to use; this will not reset every run. '
                    'Defaults to a clean profile.')
  parser.add_option('-t',
                    '--times',
                    type='int',
                    default=1,
                    help='Number of times to run each build before asking '
                    'if it\'s good or bad. Temporary profiles are reused.')
  parser.add_option('-c',
                    '--command',
                    type='str',
                    default='%p %a',
                    help='Command to execute. %p and %a refer to Chrome '
                    'executable and specified extra arguments '
                    'respectively. Use %s to specify all extra arguments '
                    'as one string. Defaults to "%p %a". Note that any '
                    'extra paths specified should be absolute.')
  parser.add_option('-l',
                    '--blink',
                    action='store_true',
                    help='Use Blink bisect instead of Chromium. ')
  parser.add_option('-v',
                    '--verbose',
                    action='store_true',
                    help='Log more verbose information.')
  parser.add_option('',
                    '--not-interactive',
                    action='store_true',
                    default=False,
                    help='Use command exit code to tell good/bad revision.')
  parser.add_option('--asan',
                    dest='asan',
                    action='store_true',
                    default=False,
                    help='Allow the script to bisect ASAN builds')
  parser.add_option('--use-local-cache',
                    dest='use_local_cache',
                    action='store_true',
                    default=False,
                    help='Use a local file in the current directory to cache '
                         'a list of known revisions to speed up the '
                         'initialization of this script.')
  parser.add_option('--verify-range',
                    dest='verify_range',
                    action='store_true',
                    default=False,
                    help='Test the first and last revisions in the range ' +
                         'before proceeding with the bisect.')
  parser.add_option('--apk',
                    choices=list(set().union(CHROME_APK_FILENAMES,
                                             CHROME_MODERN_APK_FILENAMES,
                                             MONOCHROME_APK_FILENAMES,
                                             WEBVIEW_APK_FILENAMES)),
                    dest='apk',
                    default='chromium',
                    help='Apk you want to bisect.')
  parser.add_option('--signed',
                    dest='signed',
                    action='store_true',
                    default=False,
                    help='Using signed binary for release build. Only support '
                    'android platform.')
  parser.add_option('-d',
                    '--device-id',
                    dest='device_id',
                    type='str',
                    help='Device to run the bisect on.')
  parser.add_option('--deploy-chrome-path',
                    dest='deploy_chrome_path',
                    type='str',
                    help='deploy_chrome binary path.')
  parser.add_option('--update-script',
                    dest='update_script',
                    action='store_true',
                    default=False,
                    help='Update this script to the latest.')

  return parser


def ParseCommandLine(args=None):
  """Parses the command line for bisect options."""
  official_choices = [
      'android-arm', 'android-arm64', 'linux64', 'mac', 'mac-arm', 'win64',
      'lacros64', 'lacros-arm32', 'lacros-arm64'
  ]
  parser = _CreateCommandLineParser()
  opts, args = parser.parse_args(args)

  if opts.update_script:
    UpdateScript()

  if opts.archive is None:
    print('Error: Missing required parameter: --archive')
    parser.print_help()
    sys.exit(1)

  if opts.signed and opts.archive not in ['android-arm', 'android-arm64']:
    print('Signed bisection is only supported for Android platform.')
    exit(1)

  if opts.signed and not opts.release_builds:
    print('Signed bisection is only supported for release bisection.')
    exit(1)

  if opts.official_builds and opts.archive not in official_choices:
    raise BisectException(
        ('Error: Bisecting on official builds are only '
         'supported on these platforms: [%s].' % '|'.join(official_choices)))
  elif opts.official_builds and opts.archive in official_choices:
    print('Bisecting on continuous Chrome builds. If you would like '
          'to bisect on release builds, try running with -r option '
          'instead. Previous -o options is currently changed to -r option '
          'as continous official builds were added for bisect')
  if opts.asan:
    supported_platforms = ['linux', 'mac', 'win']
    if opts.archive not in supported_platforms:
      print(('Error: ASAN bisecting only supported on these platforms: [%s].' %
             ('|'.join(supported_platforms))))
      sys.exit(1)
    if opts.release_builds:
      raise NotImplementedError(
          'Do not yet support bisecting release ASAN builds.')

  return opts, args


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
  opts, args = ParseCommandLine()

  if not opts.bad:
    print('Please specify a bad version.')
    return 1

  if not opts.good:
    print('Please specify a good version.')
    return 1

  try:
    SetupEnvironment(opts)
  except BisectException as e:
    print(e)
    sys.exit(1)

  device = None
  if opts.archive in ['android-arm', 'android-arm64']:
    device = InitializeAndroidDevice(opts.device_id, opts.apk, args)
    if not device:
      raise BisectException('Failed to initialize device.')

  deploy_chrome_path = None
  if opts.archive in ['lacros64', 'lacros-arm32', 'lacros-arm64']:
    if not opts.device_id:
      raise BisectException('Please specify device id for a cros device.')
    device = opts.device_id
    if not opts.deploy_chrome_path:
      raise BisectException('Please specify deploy_chrome path.')
    deploy_chrome_path = opts.deploy_chrome_path
  # Create the context. Initialize 0 for the revisions as they are set below.
  context = PathContext(opts, device)

  if context.is_release:
    if not IsVersionNumber(opts.good):
      print('For release, you can only use chrome version to bisect.')
      return 1
  else:
    # For official and snapshot, we convert good and bad to commit position
    # as int.
    if IsVersionNumber(opts.good):
      context.good_revision = GetRevisionFromVersion(opts.good)
      context.bad_revision = GetRevisionFromVersion(opts.bad)
    else:
      context.good_revision = int(context.good_revision)
      context.bad_revision = int(context.bad_revision)

  context.deploy_chrome_path = deploy_chrome_path

  if opts.times < 1:
    print(('Number of times to run (%d) must be greater than or equal to 1.' %
           opts.times))
    parser.print_help()
    return 1

  if opts.not_interactive:
    evaluator = DidCommandSucceed
  elif opts.asan:
    evaluator = IsGoodASANBuild
  else:
    evaluator = AskIsGoodBuild

  # Save these revision numbers to compare when showing the changelog URL
  # after the bisect.
  good_rev = context.good_revision
  bad_rev = context.bad_revision

  (min_chromium_rev, max_chromium_rev,
   context) = Bisect(context, opts.times, opts.command, args, opts.profile,
                     evaluator, opts.verify_range, opts.archive)

  # Get corresponding blink revisions.
  try:
    min_blink_rev = GetBlinkRevisionForChromiumRevision(context,
                                                        min_chromium_rev)
    max_blink_rev = GetBlinkRevisionForChromiumRevision(context,
                                                        max_chromium_rev)
  except Exception:
    # Silently ignore the failure.
    min_blink_rev, max_blink_rev = 0, 0

  if opts.blink:
    # We're done. Let the user know the results in an official manner.
    if good_rev > bad_rev:
      print(DONE_MESSAGE_GOOD_MAX % (str(min_blink_rev), str(max_blink_rev)))
    else:
      print(DONE_MESSAGE_GOOD_MIN % (str(min_blink_rev), str(max_blink_rev)))

    print('BLINK CHANGELOG URL:')
    print('  ' + BLINK_CHANGELOG_URL % (max_blink_rev, min_blink_rev))

  else:
    # We're done. Let the user know the results in an official manner.
    if good_rev > bad_rev:
      print(DONE_MESSAGE_GOOD_MAX % (str(min_chromium_rev),
                                     str(max_chromium_rev)))
    else:
      print(DONE_MESSAGE_GOOD_MIN % (str(min_chromium_rev),
                                     str(max_chromium_rev)))
    if min_blink_rev != max_blink_rev:
      print ('NOTE: There is a Blink roll in the range, '
             'you might also want to do a Blink bisect.')

    print('CHANGELOG URL:')
    if opts.release_builds:
      print(RELEASE_CHANGELOG_URL % (min_chromium_rev, max_chromium_rev))
    else:
      if opts.official_builds:
        print('The script might not always return single CL as suspect '
              'as some perf builds might get missing due to failure.')
      PrintChangeLog(min_chromium_rev, max_chromium_rev)

if __name__ == '__main__':
  sys.exit(main())
