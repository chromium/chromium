#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Snapshot Build Bisect Tool

This script bisects a snapshot archive using binary search. It starts at
a bad revision (it will try to guess HEAD) and asks for a last known-good
revision. It will then binary search across this revision range by downloading,
unzipping, and opening Chromium for you. After testing the specific revision,
it will ask you whether it is good or bad before continuing the search.
"""

from __future__ import print_function

# The base URL for stored build archives.
CHROMIUM_BASE_URL = ('http://commondatastorage.googleapis.com'
                     '/chromium-browser-snapshots')
WEBKIT_BASE_URL = ('http://commondatastorage.googleapis.com'
                   '/chromium-webkit-snapshots')
ASAN_BASE_URL = ('http://commondatastorage.googleapis.com'
                 '/chromium-browser-asan')

# URL template for viewing changelogs between revisions.
CHANGELOG_URL = ('https://chromium.googlesource.com/chromium/src/+log/%s..%s')

# URL to convert SVN revision to git hash.
CRREV_URL = ('https://cr-rev.appspot.com/_ah/api/crrev/v1/redirect/')

# DEPS file URL.
DEPS_FILE = ('https://chromium.googlesource.com/chromium/src/+/%s/DEPS')

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

# Search pattern to be matched in the JSON output from
# CHROMIUM_GITHASH_TO_SVN_URL to get the chromium revision (svn revision).
CHROMIUM_SEARCH_PATTERN_OLD = (
    r'.*git-svn-id: svn://svn.chromium.org/chrome/trunk/src@(\d+) ')
CHROMIUM_SEARCH_PATTERN = (
    r'Cr-Commit-Position: refs/heads/master@{#(\d+)}')

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

###############################################################################

import glob
import httplib
import json
import optparse
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import threading
import urllib
from distutils.version import LooseVersion
from xml.etree import ElementTree
import zipfile


class PathContext(object):
  """A PathContext is used to carry the information used to construct URLs and
  paths when dealing with the storage server and archives."""
  def __init__(self, base_url, platform, good_revision, bad_revision,
               is_asan, use_local_cache, flash_path = None):
    super(PathContext, self).__init__()
    # Store off the input parameters.
    self.base_url = base_url
    self.platform = platform  # What's passed in to the '-a/--archive' option.
    self.good_revision = good_revision
    self.bad_revision = bad_revision
    self.is_asan = is_asan
    self.build_type = 'release'
    self.flash_path = flash_path
    # Dictionary which stores svn revision number as key and it's
    # corresponding git hash as value. This data is populated in
    # _FetchAndParse and used later in GetDownloadURL while downloading
    # the build.
    self.githash_svn_dict = {}
    # The name of the ZIP file in a revision directory on the server.
    self.archive_name = None

    # Whether to cache and use the list of known revisions in a local file to
    # speed up the initialization of the script at the next run.
    self.use_local_cache = use_local_cache

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
    if self.platform in ('linux', 'linux64', 'linux-arm', 'chromeos'):
      self._binary_name = 'chrome'
    elif self.platform in ('mac', 'mac64'):
      self.archive_name = 'chrome-mac.zip'
      self._archive_extract_dir = 'chrome-mac'
    elif self.platform in ('win', 'win64'):
      # Note: changed at revision 591483; see GetDownloadURL and GetLaunchPath
      # below where these are patched.
      self.archive_name = 'chrome-win32.zip'
      self._archive_extract_dir = 'chrome-win32'
      self._binary_name = 'chrome.exe'
    else:
      raise Exception('Invalid platform: %s' % self.platform)

    if self.platform in ('linux', 'linux64', 'linux-arm', 'chromeos'):
      # Note: changed at revision 591483; see GetDownloadURL and GetLaunchPath
      # below where these are patched.
      self.archive_name = 'chrome-linux.zip'
      self._archive_extract_dir = 'chrome-linux'
      if self.platform == 'linux':
        self._listing_platform_dir = 'Linux/'
      elif self.platform == 'linux64':
        self._listing_platform_dir = 'Linux_x64/'
      elif self.platform == 'linux-arm':
        self._listing_platform_dir = 'Linux_ARM_Cross-Compile/'
      elif self.platform == 'chromeos':
        self._listing_platform_dir = 'Linux_ChromiumOS_Full/'
    elif self.platform in ('mac', 'mac64'):
      self._listing_platform_dir = 'Mac/'
      self._binary_name = 'Chromium.app/Contents/MacOS/Chromium'
    elif self.platform == 'win':
      self._listing_platform_dir = 'Win/'
    elif self.platform == 'win64':
      self._listing_platform_dir = 'Win_x64/'

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
    else:
      return (self.base_url + '/?delimiter=/&prefix=' +
              self._listing_platform_dir + marker_param)

  def GetDownloadURL(self, revision):
    """Gets the download URL for a build archive of a specific revision."""
    if self.is_asan:
      return '%s/%s-%s/%s-%d.zip' % (
          ASAN_BASE_URL, self.GetASANPlatformDir(), self.build_type,
          self.GetASANBaseName(), revision)
    if str(revision) in self.githash_svn_dict:
      revision = self.githash_svn_dict[str(revision)]
    archive_name = self.archive_name

    # At revision 591483, the names of two of the archives changed
    # due to: https://chromium-review.googlesource.com/#/q/1226086
    # See: http://crbug.com/789612
    if revision >= 591483:
      if self.platform == 'chromeos':
        archive_name = 'chrome-chromeos.zip'
      elif self.platform in ('win', 'win64'):
        archive_name = 'chrome-win.zip'

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
    else:
      extract_dir = self._archive_extract_dir

    # At revision 591483, the names of two of the archives changed
    # due to: https://chromium-review.googlesource.com/#/q/1226086
    # See: http://crbug.com/789612
    if revision >= 591483:
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
      handle = urllib.urlopen(url)
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
        # trailing slash to just have a number.
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
    response = urllib.urlopen(json_url)
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

  def GetRevList(self):
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
      revlist_all.extend(map(int, self.ParseDirectoryIndex(last_known_rev)))
      revlist_all = list(set(revlist_all))
      revlist_all.sort()
      _SaveBucketToCache()

    revlist = [x for x in revlist_all if x >= int(minrev) and x <= int(maxrev)]

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
      if not os.path.isdir(directory):
        os.makedirs(directory)
      out = open(name, 'wb')
      out.write(zf.read(name))
      out.close()
    # Set permissions. Permission info in external_attr is shifted 16 bits.
    os.chmod(name, info.external_attr >> 16L)
  os.chdir(cwd)


def FetchRevision(context, rev, filename, quit_event=None, progress_event=None):
  """Downloads and unzips revision |rev|.
  @param context A PathContext instance.
  @param rev The Chromium revision number/tag to download.
  @param filename The destination for the downloaded file.
  @param quit_event A threading.Event which will be set by the master thread to
                    indicate that the download should be aborted.
  @param progress_event A threading.Event which will be set by the master thread
                    to indicate that the progress of the download should be
                    displayed.
  """
  def ReportHook(blocknum, blocksize, totalsize):
    if quit_event and quit_event.isSet():
      raise RuntimeError('Aborting download of revision %s' % str(rev))
    if progress_event and progress_event.isSet():
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
    urllib.urlretrieve(download_url, filename, ReportHook)
    if progress_event and progress_event.isSet():
      print()

  except RuntimeError:
    pass


def CopyMissingFileFromCurrentSource(src_glob, dst):
  """Work around missing files in archives.
  This happens when archives of Chrome don't contain all of the files
  needed to build it. In many cases we can work around this using
  files from the current checkout. The source is in the form of a glob
  so that it can try to look for possible sources of the file in
  multiple locations, but we just arbitrarily try the first match.

  Silently fail if this doesn't work because we don't yet have clear
  markers for builds that require certain files or a way to test
  whether or not launching Chrome succeeded.
  """
  if not os.path.exists(dst):
    matches = glob.glob(src_glob)
    if matches:
      shutil.copy2(matches[0], dst)


def RunRevision(context, revision, zip_file, profile, num_runs, command, args):
  """Given a zipped revision, unzip it and run the test."""
  print('Trying revision %s...' % str(revision))

  # Create a temp directory and unzip the revision into it.
  cwd = os.getcwd()
  tempdir = tempfile.mkdtemp(prefix='bisect_tmp')
  UnzipFilenameToDir(zip_file, tempdir)

  # Hack: Some Chrome OS archives are missing some files; try to copy them
  # from the local directory.
  if context.platform == 'chromeos' and revision < 591483:
    CopyMissingFileFromCurrentSource('third_party/icu/common/icudtl.dat',
                                     '%s/chrome-linux/icudtl.dat' % tempdir)
    CopyMissingFileFromCurrentSource('*out*/*/libminigbm.so',
                                     '%s/chrome-linux/libminigbm.so' % tempdir)

  os.chdir(tempdir)

  # Run the build as many times as specified.
  testargs = ['--user-data-dir=%s' % profile] + args
  # The sandbox must be run as root on Official Chrome, so bypass it.
  if (context.flash_path and context.platform.startswith('linux')):
    testargs.append('--no-sandbox')
  if context.flash_path:
    testargs.append('--ppapi-flash-path=%s' % context.flash_path)
    # We have to pass a large enough Flash version, which currently needs not
    # be correct. Instead of requiring the user of the script to figure out and
    # pass the correct version we just spoof it.
    testargs.append('--ppapi-flash-version=99.9.999.999')

  runcommand = []
  for token in shlex.split(command):
    if token == '%a':
      runcommand.extend(testargs)
    else:
      runcommand.append(
          token.replace('%p', os.path.abspath(context.GetLaunchPath(revision))).
          replace('%s', ' '.join(testargs)))
  result = None
  try:
    for _ in range(num_runs):
      subproc = subprocess.Popen(
          runcommand,
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


# The arguments status, stdout and stderr are unused.
# They are present here because this function is passed to Bisect which then
# calls it with 5 arguments.
# pylint: disable=W0613
def AskIsGoodBuild(rev, exit_status, stdout, stderr):
  """Asks the user whether build |rev| is good or bad."""
  if exit_status:
    print('Chrome exit_status: %d. Use s to see output' % exit_status)
  # Loop until we get a response that we can parse.
  while True:
    response = raw_input('Revision %s is '
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
    os.unlink(self.zip_file)

  def WaitFor(self):
    """Prints a message and waits for the download to complete. The download
    must have been started previously."""
    assert self.thread, 'DownloadJob must be started before WaitFor is called.'
    print('Downloading revision %s...' % str(self.rev))
    self.progress_event.set()  # Display progress of download.
    try:
      while self.thread.isAlive():
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
    (exit_status, stdout, stderr) = RunRevision(
        context, rev, fetch.zip_file, profile, num_runs, command, try_args)
  except Exception, e:
    print(e, file=sys.stderr)
    raise SystemExit
  if (evaluate(rev, exit_status, stdout, stderr) != expected_answer):
    print('Unexpected result at a range boundary! Your range is not correct.')
    raise SystemExit


def Bisect(context,
           num_runs=1,
           command='%p %a',
           try_args=(),
           profile=None,
           evaluate=AskIsGoodBuild,
           verify_range=False):
  """Given known good and known bad revisions, run a binary search on all
  archived revisions to determine the last known good revision.

  @param context PathContext object initialized with user provided parameters.
  @param num_runs Number of times to run each build for asking good/bad.
  @param try_args A tuple of arguments to pass to the test application.
  @param profile The name of the user profile to run with.
  @param evaluate A function which returns 'g' if the argument build is good,
                  'b' if it's bad or 'u' if unknown.
  @param verify_range If true, tests the first and last revisions in the range
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

  if not profile:
    profile = 'profile'

  good_rev = context.good_revision
  bad_rev = context.bad_revision
  cwd = os.getcwd()

  print('Downloading list of known revisions...', end=' ')
  if not context.use_local_cache:
    print('(use --use-local-cache to cache and re-use the list of revisions)')
  else:
    print()
  _GetDownloadPath = lambda rev: os.path.join(cwd,
      '%s-%s' % (str(rev), context.archive_name))
  revlist = context.GetRevList()

  # Get a list of revisions to bisect across.
  if len(revlist) < 2:  # Don't have enough builds to bisect.
    msg = 'We don\'t have enough builds to bisect. revlist: %s' % revlist
    raise RuntimeError(msg)

  # Figure out our bookends and first pivot point; fetch the pivot revision.
  minrev = 0
  maxrev = len(revlist) - 1
  pivot = maxrev / 2
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
  while fetch and fetch.zip_file and maxrev - minrev > 1:
    if bad_rev < good_rev:
      min_str, max_str = 'bad', 'good'
    else:
      min_str, max_str = 'good', 'bad'
    print(
        'Bisecting range [%s (%s), %s (%s)], '
        'roughly %d steps left.' % (revlist[minrev], min_str, revlist[maxrev],
                                    max_str, int(maxrev - minrev).bit_length()))

    # Pre-fetch next two possible pivots
    #   - down_pivot is the next revision to check if the current revision turns
    #     out to be bad.
    #   - up_pivot is the next revision to check if the current revision turns
    #     out to be good.
    down_pivot = int((pivot - minrev) / 2) + minrev
    down_fetch = None
    if down_pivot != pivot and down_pivot != minrev:
      down_rev = revlist[down_pivot]
      down_fetch = DownloadJob(context, 'down_fetch', down_rev,
                               _GetDownloadPath(down_rev))
      down_fetch.Start()

    up_pivot = int((maxrev - pivot) / 2) + pivot
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
    except Exception, e:
      print(e, file=sys.stderr)

    # Call the evaluate function to see if the current revision is good or bad.
    # On that basis, kill one of the background downloads and complete the
    # other, as described in the comments above.
    try:
      answer = evaluate(rev, exit_status, stdout, stderr)
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
        pass  # Retry requires no changes.
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

  url = urllib.urlopen(DEPS_FILE % GetGitHashFromSVNRevision(rev))
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
  url = urllib.urlopen(file_url)
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
    latest_revision = urllib.urlopen(url).read()
    if latest_revision.isdigit():
      return int(latest_revision)
    return context.GetSVNRevisionFromGitHash(latest_revision)
  except Exception:
    print('Could not determine latest revision. This could be bad...')
    return 999999999

def GetGitHashFromSVNRevision(svn_revision):
  crrev_url = CRREV_URL + str(svn_revision)
  url = urllib.urlopen(crrev_url)
  if url.getcode() == 200:
    data = json.loads(url.read())
    if 'git_sha' in data:
      return data['git_sha']

def PrintChangeLog(min_chromium_rev, max_chromium_rev):
  """Prints the changelog URL."""

  print('  ' + CHANGELOG_URL % (GetGitHashFromSVNRevision(min_chromium_rev),
                                GetGitHashFromSVNRevision(max_chromium_rev)))


def error_internal_option(option, opt, value, parser):
  raise optparse.OptionValueError(
        'The -o and -r options are only\navailable in the internal version of '
        'this script. Google\nemployees should visit http://go/bisect-builds '
        'for\nconfiguration instructions.')

def main():
  usage = ('%prog [options] [-- chromium-options]\n'
           'Perform binary search on the snapshot builds to find a minimal\n'
           'range of revisions where a behavior change happened. The\n'
           'behaviors are described as "good" and "bad".\n'
           'It is NOT assumed that the behavior of the later revision is\n'
           'the bad one.\n'
           '\n'
           'Revision numbers should use\n'
           '  SVN revisions (e.g. 123456) for chromium builds, from trunk.\n'
           '    Use base_trunk_revision from http://omahaproxy.appspot.com/\n'
           '    for earlier revs.\n'
           '    Chrome\'s about: build number and omahaproxy branch_revision\n'
           '    are incorrect, they are from branches.\n'
           '\n'
           'Tip: add "-- --no-first-run" to bypass the first run prompts.')
  parser = optparse.OptionParser(usage=usage)
  # Strangely, the default help output doesn't include the choice list.
  choices = ['mac', 'mac64', 'win', 'win64', 'linux', 'linux64', 'linux-arm',
             'chromeos']
  parser.add_option('-a', '--archive',
                    choices=choices,
                    help='The buildbot archive to bisect [%s].' %
                         '|'.join(choices))
  parser.add_option('-b', '--bad',
                    type='str',
                    help='A bad revision to start bisection. '
                         'May be earlier or later than the good revision. '
                         'Default is HEAD.')
  parser.add_option('-f', '--flash_path',
                    type='str',
                    help='Absolute path to a recent Adobe Pepper Flash '
                         'binary to be used in this bisection (e.g. '
                         'on Windows C:\...\pepflashplayer.dll and on Linux '
                         '/opt/google/chrome/PepperFlash/'
                         'libpepflashplayer.so).')
  parser.add_option('-g', '--good',
                    type='str',
                    help='A good revision to start bisection. ' +
                         'May be earlier or later than the bad revision. ' +
                         'Default is 0.')
  parser.add_option('-p', '--profile', '--user-data-dir',
                    type='str',
                    default='profile',
                    help='Profile to use; this will not reset every run. '
                         'Defaults to a clean profile.')
  parser.add_option('-t', '--times',
                    type='int',
                    default=1,
                    help='Number of times to run each build before asking '
                         'if it\'s good or bad. Temporary profiles are reused.')
  parser.add_option('-c', '--command',
                    type='str',
                    default='%p %a',
                    help='Command to execute. %p and %a refer to Chrome '
                         'executable and specified extra arguments '
                         'respectively. Use %s to specify all extra arguments '
                         'as one string. Defaults to "%p %a". Note that any '
                         'extra paths specified should be absolute.')
  parser.add_option('-l', '--blink',
                    action='store_true',
                    help='Use Blink bisect instead of Chromium. ')
  parser.add_option('', '--not-interactive',
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
  parser.add_option("-r", action="callback", callback=error_internal_option)
  parser.add_option("-o", action="callback", callback=error_internal_option)

  (opts, args) = parser.parse_args()

  if opts.archive is None:
    print('Error: missing required parameter: --archive')
    print()
    parser.print_help()
    return 1

  if opts.asan:
    supported_platforms = ['linux', 'mac', 'win']
    if opts.archive not in supported_platforms:
      print('Error: ASAN bisecting only supported on these platforms: [%s].' %
            ('|'.join(supported_platforms)))
      return 1

  if opts.asan:
    base_url = ASAN_BASE_URL
  elif opts.blink:
    base_url = WEBKIT_BASE_URL
  else:
    base_url = CHROMIUM_BASE_URL

  # Create the context. Initialize 0 for the revisions as they are set below.
  context = PathContext(base_url, opts.archive, opts.good, opts.bad,
                        opts.asan, opts.use_local_cache,
                        opts.flash_path)

  # Pick a starting point, try to get HEAD for this.
  if not opts.bad:
    context.bad_revision = '999.0.0.0'
    context.bad_revision = GetChromiumRevision(
        context, context.GetLastChangeURL())

  # Find out when we were good.
  if not opts.good:
    context.good_revision = 0

  if opts.flash_path:
    msg = 'Could not find Flash binary at %s' % opts.flash_path
    assert os.path.exists(opts.flash_path), msg

  context.good_revision = int(context.good_revision)
  context.bad_revision = int(context.bad_revision)

  if opts.times < 1:
    print('Number of times to run (%d) must be greater than or equal to 1.' %
          opts.times)
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

  (min_chromium_rev, max_chromium_rev, context) = Bisect(
      context, opts.times, opts.command, args, opts.profile,
      evaluator, opts.verify_range)

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
    PrintChangeLog(min_chromium_rev, max_chromium_rev)


if __name__ == '__main__':
  sys.exit(main())
