# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Bisect repackage tool for Linux.

This script repacakges chrome builds for manual bisect script.
"""

from __future__ import print_function

from functools import partial
import json
import logging
from multiprocessing import Pool
import optparse
import os
import re
import sys
import tempfile
import threading
import urllib
import bisect_repackage_utils
import re
# This script uses cloud_storage module which contains gsutils wrappers.
# cloud_storage module is a part of catapult repo, so please make sure
# catapult is checked out before running this script.
_PY_UTILS_PATH = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', 'third_party', 'catapult',
    'common', 'py_utils'))
if _PY_UTILS_PATH not in sys.path:
  sys.path.insert(1, _PY_UTILS_PATH)
from py_utils import cloud_storage

# Declares required files to run manual bisect script on chrome Linux
# builds in perf. Binary files that should be stripped to reduce zip file
# size are declared. The file list was gotten from the local chrome
# executable path. (This can be retrieved by typing 'chrome://version'
# in chrome and following the executable path. The list needs to be updated if
# future chrome versions require additional files.
CHROME_REQUIRED_FILES = {
    'arm': ['apks/'],
    'arm64': ['apks/'],
    'linux': [
        'chrome',
        'chrome_100_percent.pak',
        'chrome_200_percent.pak',
        'chromedriver',
        'chrome_crashpad_handler',
        'default_apps/',
        'icudtl.dat',
        'ClearKeyCdm/',
        'WidevineCdm/',
        'locales/',
        'nacl_helper',
        'nacl_helper_bootstrap',
        'nacl_helper_nonsfi',
        'nacl_irt_x86_64.nexe',
        'pnacl/',
        'product_logo_48.png',
        'resources/',
        'resources.pak',
        'v8_context_snapshot.bin',
        'xdg-mime',
        'xdg-settings'
    ],
    'win64': [
        'chrome.dll',
        'chrome.exe',
        'chrome_100_percent.pak',
        'chrome_200_percent.pak',
        'chrome_child.dll',
        'chrome_elf.dll',
        'chrome_watcher.dll',
        'chromedriver.exe',
        'default_apps',
        'd3dcompiler_47.dll',
        'icudtl.dat',
        'libEGL.dll',
        'libGLESv2.dll',
        'locales',
        'nacl_irt_x86_64.nexe',
        'PepperFlash',
        'resources.pak',
        'SecondaryTile.png',
        'v8_context_snapshot.bin'
    ],
    'mac': [
        'chromedriver',
        'Google Chrome.app'
    ]
}

CHROME_WHITELIST_FILES = {
    'win64': '^\d+\.\d+\.\d+\.\d+\.manifest$',
}
# No stripping symbols from android, windows or mac archives
CHROME_STRIP_LIST = {
    'linux': [
        'chrome',
        'chrome_crashpad_handler',
        'nacl_helper'
    ]
}

# API to convert Githash to Commit position number.
CHROMIUM_GITHASH_TO_SVN_URL = (
    'https://cr-rev.appspot.com/_ah/api/crrev/v1/commit/%s')
CHROMIUM_CP_TO_GITHASH = (
'https://cr-rev.appspot.com/_ah/api/crrev/v1/redirect/%s')

REVISION_MAP_FILE = 'revision_map.json'

BUILDER_NAME = {
    'arm': 'Android Builder',
    'arm64': 'Android arm64 Builder',
    'linux': 'Linux Builder',
    'mac': 'Mac Builder',
    'win32': 'Win Builder',
    'win64': 'Win x64 Builder'
}

ARCHIVE_PREFIX = {
    'arm': 'full-build-linux',
    'arm64': 'full-build-linux',
    'linux': 'full-build-linux',
    'mac': 'full-build-mac',
    'win32': 'full-build-win32',
    'win64': 'full-build-win32'
}

CHROME_TEST_BUCKET_SUBFOLDER = 'official-by-commit'


class ChromeExecutionError(Exception):
  """Raised when Chrome execution fails."""
  pass

class GitConversionError(Exception):
  """Raised when Chrome execution fails."""
  pass

class PathContext(object):
  """Stores information to repackage from a bucket to another.

  A PathContext is used to carry the information used to construct URLs and
  paths when dealing with the storage server and archives.
  """

  def __init__(self, source_bucket, repackage_bucket,
               archive, revision_file=REVISION_MAP_FILE):
    super(PathContext, self).__init__()
    self.archive = archive
    self.builder_name = BUILDER_NAME[archive]
    self.original_gs_bucket = source_bucket
    self.original_remote_path = BUILDER_NAME[archive]
    self.repackage_gs_bucket = repackage_bucket
    self.repackage_remote_path = '%s/%s' % (CHROME_TEST_BUCKET_SUBFOLDER,
                                            BUILDER_NAME[archive])

    self.file_prefix = ARCHIVE_PREFIX[archive]
    self.revision_file = os.path.join(os.getcwd(), revision_file)

  def GetExtractedDir(self):
    # Perf builders archives the binaries in out/Release directory.
    if self.archive in ['arm', 'arm64']:
      return os.path.join('out', 'Release')
    return self.file_prefix


def get_cp_from_hash(git_hash):
  """Converts a git hash to commit position number."""
  json_url = CHROMIUM_GITHASH_TO_SVN_URL % git_hash
  response = urllib.urlopen(json_url)
  if response.getcode() == 200:
    try:
      data = json.loads(response.read())
    except Exception,e:
      logging.warning('JSON URL: %s, Error Message: %s' % json_url, e)
      raise GitConversionError
  else:
      logging.warning('JSON URL: %s, Error Message: %s' % json_url, e)
      raise GitConversionError
  if 'number' in data:
    return data['number']
  logging.warning('JSON URL: %s, Error Message: %s' % json_url, e)
  raise GitConversionError


def create_cp_from_hash_map(hash_list):
  """Returns dict used for conversion of hash list.

  Creates a dictionary that maps from Commit position number
  to corresponding GitHash.
  """
  hash_map = {}
  for git_hash in hash_list:
    try:
      cp_num = get_cp_from_hash(git_hash)
      hash_map[cp_num] = git_hash
    except GitConversionError:
      pass
  return hash_map


def get_list_of_suffix(bucket_address, prefix, filter_function):
  """Gets the list of suffixes in files in a google storage bucket.

  Example: a google storage bucket containing one file
  'full-build-linux_20983' will return ['20983'] if prefix is
  provided as 'full-build-linux'. Google Storage bucket
  containing multiple files will return multiple suffixes.

  Args:
    bucket_address(String): Bucket URL to examine files from.
    prefix(String): The prefix used in creating build file names
    filter_function: A function that returns true if the extracted
      suffix is in correct format and false otherwise. It allows
      only proper suffix to be extracted and returned.

  Returns:
    (List) list of proper suffixes in the bucket.
  """
  file_list = cloud_storage.List(bucket_address)
  suffix_list = []
  extract_suffix = '.*?%s_(.*?)\.zip' %(prefix)
  for file in file_list:
    match = re.match(extract_suffix, file)
    if match and filter_function(match.groups()[0]):
      suffix_list.append(match.groups()[0])
  return suffix_list


def download_build(cp_num, revision_map, zip_file_name, context):
  """Download a single build corresponding to the cp_num and context."""
  remote_file_path = '%s/%s_%s.zip' % (context.original_remote_path,
                                       context.file_prefix,
                                       revision_map[cp_num])
  try:
    cloud_storage.Get(context.original_gs_bucket,
                      remote_file_path, zip_file_name)
  except Exception, e:
    logging.warning('Failed to download: %s, error: %s', zip_file_name, e)
    return False
  return True


def upload_build(zip_file, context):
  """Uploads a single build in zip_file to the repackage_gs_url in context."""
  cloud_storage.Insert(
      context.repackage_gs_bucket, context.repackage_remote_path, zip_file)


def download_revision_map(context):
  """Downloads the revision map in original_gs_url in context."""
  download_file = '%s/%s' % (context.repackage_remote_path, REVISION_MAP_FILE)
  cloud_storage.Get(context.repackage_gs_bucket, download_file,
                    context.revision_file)

def get_revision_map(context):
  """Downloads and returns the revision map in repackage_gs_url in context."""
  bisect_repackage_utils.RemoveFile(context.revision_file)
  download_revision_map(context)
  with open(context.revision_file, 'r') as revision_file:
    revision_map = json.load(revision_file)
  bisect_repackage_utils.RemoveFile(context.revision_file)
  return revision_map


def upload_revision_map(revision_map, context):
  """Upload the given revision_map to the repackage_gs_url in context."""
  with open(context.revision_file, 'w') as revision_file:
    json.dump(revision_map, revision_file)
  cloud_storage.Insert(context.repackage_gs_bucket,
                       context.repackage_remote_path,
                       context.revision_file)
  bisect_repackage_utils.RemoveFile(context.revision_file)


def create_upload_revision_map(context):
  """Creates and uploads a dictionary that maps from GitHash to CP number."""
  gs_base_url = '%s/%s' % (context.original_gs_bucket,
                           context.original_remote_path)
  hash_list = get_list_of_suffix(gs_base_url, context.file_prefix,
                                 bisect_repackage_utils.IsGitCommitHash)
  cp_num_to_hash_map = create_cp_from_hash_map(hash_list)
  upload_revision_map(cp_num_to_hash_map, context)


def update_upload_revision_map(context):
  """Updates and uploads a dictionary that maps from GitHash to CP number."""
  gs_base_url = '%s/%s' % (context.original_gs_bucket,
                           context.original_remote_path)
  revision_map = get_revision_map(context)
  hash_list = get_list_of_suffix(gs_base_url, context.file_prefix,
                                 bisect_repackage_utils.IsGitCommitHash)
  hash_list = list(set(hash_list)-set(revision_map.values()))
  cp_num_to_hash_map = create_cp_from_hash_map(hash_list)
  merged_dict = dict(cp_num_to_hash_map.items() + revision_map.items())
  upload_revision_map(merged_dict, context)


def make_lightweight_archive(file_archive, archive_name, files_to_archive,
                             context, staging_dir, ignore_sub_folder):
  """Repackages and strips the archive.

  Repacakges and strips according to CHROME_REQUIRED_FILES and
  CHROME_STRIP_LIST.
  """
  strip_list = CHROME_STRIP_LIST.get(context.archive)
  tmp_archive = os.path.join(staging_dir, 'tmp_%s' % archive_name)
  (zip_dir, zip_file) = bisect_repackage_utils.MakeZip(
      tmp_archive, archive_name, files_to_archive, file_archive,
      dir_in_zip=context.GetExtractedDir(),
      raise_error=False, strip_files=strip_list,
      ignore_sub_folder=ignore_sub_folder)
  return (zip_dir, zip_file, tmp_archive)


def remove_created_files_and_path(files, paths):
  """Removes all the files and paths passed in."""
  for file in files:
    bisect_repackage_utils.RemoveFile(file)
  for path in paths:
    bisect_repackage_utils.RemovePath(path)


def verify_chrome_run(zip_dir):
  """This function executes chrome executable in zip_dir.

  Currently, it is only supported for Linux Chrome builds.
  Raises error if the execution fails for any reason.
  """
  try:
    command = [os.path.join(zip_dir, 'chrome')]
    code = bisect_repackage_utils.RunCommand(command)
    if code != 0:
      raise ChromeExecutionError('An error occurred when executing Chrome')
  except ChromeExecutionError,e:
    print(str(e))


def get_whitelist_files(extracted_folder, archive):
  """Gets all the files & directories matching whitelisted regex."""
  whitelist_files = []
  all_files = os.listdir(extracted_folder)
  for file in all_files:
    if re.match(CHROME_WHITELIST_FILES.get(archive), file):
      whitelist_files.append(file)
  return whitelist_files


def repackage_single_revision(revision_map, verify_run, staging_dir,
                              context, cp_num):
  """Repackages a single Chrome build for manual bisect."""
  archive_name = '%s_%s' %(context.file_prefix, cp_num)
  file_archive = os.path.join(staging_dir, archive_name)
  zip_file_name = '%s.zip' % (file_archive)
  if not download_build(cp_num, revision_map, zip_file_name, context):
    return

  extract_dir = os.path.join(staging_dir, archive_name)
  is_android = context.archive in ['arm', 'arm64']
  files_to_include = CHROME_REQUIRED_FILES.get(context.archive)

  dir_path_in_zip = context.GetExtractedDir()
  extract_file_list = []
  # Only extract required files and directories.
  # And when there is no pattern checking for files.
  if not CHROME_WHITELIST_FILES.get(context.archive):
    for f in files_to_include:
      if f.endswith('/'):
        f += '*'
      extract_file_list.append(os.path.join(dir_path_in_zip, f))

  bisect_repackage_utils.ExtractZip(
      zip_file_name, extract_dir, extract_file_list)
  extracted_folder = os.path.join(extract_dir, dir_path_in_zip)

  if CHROME_WHITELIST_FILES.get(context.archive):
    whitelist_files = get_whitelist_files(extracted_folder, context.archive)
    files_to_include += whitelist_files

  (zip_dir, zip_file, tmp_archive) = make_lightweight_archive(extracted_folder,
                                                              archive_name,
                                                              files_to_include,
                                                              context,
                                                              staging_dir,
                                                              is_android)

  if verify_run:
    verify_chrome_run(zip_dir)
  upload_build(zip_file, context)
  with open('upload_revs.json', 'r+') as rfile:
    update_map = json.load(rfile)
    update_map[str(cp_num)] = 'Done'
    rfile.seek(0)
    json.dump(update_map, rfile)
    rfile.truncate()
  # Removed temporary files created during repackaging process.
  remove_created_files_and_path([zip_file_name],
                                [zip_dir, extract_dir, tmp_archive])


def repackage_revisions(revisions, revision_map, verify_run, staging_dir,
                        context, quit_event=None, progress_event=None):
  """Repackages all Chrome builds listed in revisions.

  This function calls 'repackage_single_revision' with multithreading pool.
  """
  p = Pool(3)
  func = partial(repackage_single_revision, revision_map, verify_run,
                 staging_dir, context)
  p.imap(func, revisions)
  p.close()
  p.join()


def get_uploaded_builds(context):
  """Returns already uploaded revisions in original bucket."""
  gs_base_url = '%s/%s' % (context.repackage_gs_bucket,
                           context.repackage_remote_path)
  return get_list_of_suffix(gs_base_url, context.file_prefix,
                            bisect_repackage_utils.IsCommitPosition)


def get_revisions_to_package(revision_map, context):
  """Returns revisions that need to be repackaged.

  It subtracts revisions that are already packaged from all revisions that
  need to be packaged. The revisions will be sorted in descending order.
  """
  already_packaged = get_uploaded_builds(context)
  not_already_packaged = list(set(revision_map.keys())-set(already_packaged))
  revisions_to_package = sorted(not_already_packaged, reverse=True)
  return revisions_to_package


def get_hash_from_cp(cp_num):
  """Converts a commit position number to git hash."""
  json_url = CHROMIUM_CP_TO_GITHASH % cp_num
  response = urllib.urlopen(json_url)
  if response.getcode() == 200:
    try:
      data = json.loads(response.read())
      if 'git_sha' in data:
        return data['git_sha']
    except Exception, e:
      logging.warning('Failed to fetch git_hash: %s, error: %s' % json_url, e)
  else:
      logging.warning('Failed to fetch git_hash: %s, CP: %s' % json_url, cp_num)
  return None


def get_revision_map_for_range(start_rev, end_rev):
  revision_map = {}
  for cp_num in range(start_rev, end_rev + 1 ):
    git_hash = get_hash_from_cp(cp_num)
    if git_hash:
      revision_map[cp_num] = git_hash
  return revision_map

def get_overwrite_revisions(revision_map):
  return sorted(revision_map.keys(), reverse=True)


class RepackageJob(object):

  def __init__(self, name, revisions_to_package, revision_map, verify_run,
               staging_dir, context):
    super(RepackageJob, self).__init__()
    self.name = name
    self.revisions_to_package = revisions_to_package
    self.revision_map = revision_map
    self.verify_run = verify_run
    self.staging_dir = staging_dir
    self.context = context
    self.quit_event = threading.Event()
    self.progress_event = threading.Event()
    self.thread = None

  def Start(self):
    """Starts the download."""
    fetchargs = (self.revisions_to_package,
                 self.revision_map,
                 self.verify_run,
                 self.staging_dir,
                 self.context,
                 self.quit_event,
                 self.progress_event)
    self.thread = threading.Thread(target=repackage_revisions,
                                   name=self.name,
                                   args=fetchargs)
    self.thread.start()

  def Stop(self):
    """Stops the download which must have been started previously."""
    assert self.thread, 'DownloadJob must be started before Stop is called.'
    self.quit_event.set()
    self.thread.join()

  def WaitFor(self):
    """Prints a message and waits for the download to complete."""
    assert self.thread, 'DownloadJob must be started before WaitFor is called.'
    self.progress_event.set()  # Display progress of download.  def Stop(self):
    assert self.thread, 'DownloadJob must be started before Stop is called.'
    self.quit_event.set()
    self.thread.join()


def main(argv):
  option_parser = optparse.OptionParser()

  choices = ['mac', 'win32', 'win64', 'linux', 'arm', 'arm64']

  option_parser.add_option('-a', '--archive',
                           choices=choices,
                           help='Builders to repacakge from [%s].' %
                           '|'.join(choices))

  # Verifies that the chrome executable runs
  option_parser.add_option('-v', '--verify',
                           action='store_true',
                           help='Verifies that the Chrome executes normally'
                                'without errors')

  # This option will update the revision map.
  option_parser.add_option('-u', '--update',
                           action='store_true',
                           help='Updates the list of revisions to repackage')

  # This option will creates the revision map.
  option_parser.add_option('-c', '--create',
                           action='store_true',
                           help='Creates the list of revisions to repackage')

  # Original bucket that contains perf builds
  option_parser.add_option('-o', '--original',
                           type='str',
                           help='Google storage bucket name containing original'
                                'Chrome builds')

  # Bucket that should archive lightweight perf builds
  option_parser.add_option('-r', '--repackage',
                           type='str',
                           help='Google storage bucket name '
                                 'to re-archive Chrome builds')

  # Overwrites build archives for a given range.
  option_parser.add_option('-w', '--overwrite',
                           action='store_true',
                           dest='overwrite',
                           help='Overwrite build archives')

  # Start revision for build overwrite.
  option_parser.add_option('-s', '--start_rev',
                           type='str',
                           dest='start_rev',
                           help='Start revision for overwrite')

  # Start revision for build overwrite.
  option_parser.add_option('-e', '--end_rev',
                           type='str',
                           dest='end_rev',
                           help='end revision for overwrite')



  verify_run = False
  (opts, args) = option_parser.parse_args()
  if opts.archive is None:
    print('Error: missing required parameter: --archive')
    option_parser.print_help()
    return 1
  if not opts.original or not opts.repackage:
    raise ValueError('Need to specify original gs bucket url and'
                     'repackage gs bucket url')
  context = PathContext(opts.original, opts.repackage, opts.archive)

  if opts.create:
    create_upload_revision_map(context)

  if opts.update:
    update_upload_revision_map(context)

  if opts.verify:
    verify_run = True

  if opts.overwrite:
    if not opts.start_rev or not opts.end_rev:
      raise ValueError('Need to specify overwrite range start (-s) and end (-e)'
                       ' revision.')
    revision_map = get_revision_map_for_range(
        int(opts.start_rev), int(opts.end_rev))
    backward_rev = get_overwrite_revisions(revision_map)
    with open('upload_revs.json', 'w') as revision_file:
      json.dump(revision_map, revision_file)
  else:
    revision_map = get_revision_map(context)
    backward_rev = get_revisions_to_package(revision_map, context)

  base_dir = os.path.join('.', context.archive)
  # Clears any uncleared staging directories and create one
  bisect_repackage_utils.RemovePath(base_dir)
  bisect_repackage_utils.MaybeMakeDirectory(base_dir)
  staging_dir = os.path.abspath(tempfile.mkdtemp(prefix='staging',
                                                 dir=base_dir))
  repackage = RepackageJob('backward_fetch', backward_rev, revision_map,
                           verify_run, staging_dir, context)
  # Multi-threading is not currently being used. But it can be used in
  # cases when the repackaging needs to be quicker.
  try:
    repackage.Start()
    repackage.WaitFor()
  except (KeyboardInterrupt, SystemExit):
    print('Cleaning up...')
    bisect_repackage_utils.RemovePath(staging_dir)
  print('Cleaning up...')
  bisect_repackage_utils.RemovePath(staging_dir)


if '__main__' == __name__:
  sys.exit(main(sys.argv))
