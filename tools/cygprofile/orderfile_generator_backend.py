#!/usr/bin/env vpython
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" A utility to generate an up-to-date orderfile.

The orderfile is used by the linker to order text sections such that the
sections are placed consecutively in the order specified. This allows us
to page in less code during start-up.

Example usage:
  tools/cygprofile/orderfile_generator_backend.py -l 20 -j 1000 --use-goma \
    --target-arch=arm
"""

import argparse
import hashlib
import json
import glob
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

import cyglog_to_orderfile
import cygprofile_utils
import patch_orderfile
import phased_orderfile
import process_profiles
import profile_android_startup
import symbol_extractor


_SRC_PATH = os.path.join(os.path.dirname(os.path.realpath(__file__)),
                         os.pardir, os.pardir)
sys.path.append(os.path.join(_SRC_PATH, 'build', 'android'))
import devil_chromium
from pylib import constants


# Needs to happen early for GetBuildType()/GetOutDirectory() to work correctly
constants.SetBuildType('Release')


class CommandError(Exception):
  """Indicates that a dispatched shell command exited with a non-zero status."""

  def __init__(self, value):
    super(CommandError, self).__init__()
    self.value = value

  def __str__(self):
    return repr(self.value)


def _GenerateHash(file_path):
  """Calculates and returns the hash of the file at file_path."""
  sha1 = hashlib.sha1()
  with open(file_path, 'rb') as f:
    while True:
      # Read in 1mb chunks, so it doesn't all have to be loaded into memory.
      chunk = f.read(1024 * 1024)
      if not chunk:
        break
      sha1.update(chunk)
  return sha1.hexdigest()


def _GetFileExtension(file_name):
  """Calculates the file extension from a file name.

  Args:
    file_name: The source file name.
  Returns:
    The part of file_name after the dot (.) or None if the file has no
    extension.
    Examples: /home/user/foo.bar     -> bar
              /home/user.name/foo    -> None
              /home/user/.foo        -> None
              /home/user/foo.bar.baz -> baz
  """
  file_name_parts = os.path.basename(file_name).split('.')
  if len(file_name_parts) > 1:
    return file_name_parts[-1]
  else:
    return None


def _StashOutputDirectory(buildpath):
  """Takes the output directory and stashes it in the default output directory.

  This allows it to be used for incremental builds next time (after unstashing)
  by keeping it in a place that isn't deleted normally, while also ensuring
  that it is properly clobbered when appropriate.

  This is a dirty hack to deal with the needs of clobbering while also handling
  incremental builds and the hardcoded relative paths used in some of the
  project files.

  Args:
    buildpath: The path where the building happens.  If this corresponds to the
               default output directory, no action is taken.
  """
  if os.path.abspath(buildpath) == os.path.abspath(os.path.dirname(
      constants.GetOutDirectory())):
    return
  name = os.path.basename(buildpath)
  stashpath = os.path.join(constants.GetOutDirectory(), name)
  if not os.path.exists(buildpath):
    return
  if os.path.exists(stashpath):
    shutil.rmtree(stashpath, ignore_errors=True)
  shutil.move(buildpath, stashpath)


def _UnstashOutputDirectory(buildpath):
  """Inverse of _StashOutputDirectory.

  Moves the output directory stashed within the default output directory
  (out/Release) to the position where the builds can actually happen.

  This is a dirty hack to deal with the needs of clobbering while also handling
  incremental builds and the hardcoded relative paths used in some of the
  project files.

  Args:
    buildpath: The path where the building happens.  If this corresponds to the
               default output directory, no action is taken.
  """
  if os.path.abspath(buildpath) == os.path.abspath(os.path.dirname(
      constants.GetOutDirectory())):
    return
  name = os.path.basename(buildpath)
  stashpath = os.path.join(constants.GetOutDirectory(), name)
  if not os.path.exists(stashpath):
    return
  if os.path.exists(buildpath):
    shutil.rmtree(buildpath, ignore_errors=True)
  shutil.move(stashpath, buildpath)


class StepRecorder(object):
  """Records steps and timings."""

  def __init__(self, buildbot):
    self.timings = []
    self._previous_step = ('', 0.0)
    self._buildbot = buildbot
    self._error_recorded = False

  def BeginStep(self, name):
    """Marks a beginning of the next step in the script.

    On buildbot, this prints a specially formatted name that will show up
    in the waterfall. Otherwise, just prints the step name.

    Args:
      name: The name of the step.
    """
    self.EndStep()
    self._previous_step = (name, time.time())
    print 'Running step: ', name

  def EndStep(self):
    """Records successful completion of the current step.

    This is optional if the step is immediately followed by another BeginStep.
    """
    if self._previous_step[0]:
      elapsed = time.time() - self._previous_step[1]
      print 'Step %s took %f seconds' % (self._previous_step[0], elapsed)
      self.timings.append((self._previous_step[0], elapsed))

    self._previous_step = ('', 0.0)

  def FailStep(self, message=None):
    """Marks that a particular step has failed.

    On buildbot, this will mark the current step as failed on the waterfall.
    Otherwise we will just print an optional failure message.

    Args:
      message: An optional explanation as to why the step failed.
    """
    print 'STEP FAILED!!'
    if message:
      print message
    self._error_recorded = True
    self.EndStep()

  def ErrorRecorded(self):
    """True if FailStep has been called."""
    return self._error_recorded

  def RunCommand(self, cmd, cwd=constants.DIR_SOURCE_ROOT, raise_on_error=True,
                 stdout=None):
    """Execute a shell command.

    Args:
      cmd: A list of command strings.
      cwd: Directory in which the command should be executed, defaults to build
           root of script's location if not specified.
      raise_on_error: If true will raise a CommandError if the call doesn't
          succeed and mark the step as failed.
      stdout: A file to redirect stdout for the command to.

    Returns:
      The process's return code.

    Raises:
      CommandError: An error executing the specified command.
    """
    print 'Executing %s in %s' % (' '.join(cmd), cwd)
    process = subprocess.Popen(cmd, stdout=stdout, cwd=cwd, env=os.environ)
    process.wait()
    if raise_on_error and process.returncode != 0:
      self.FailStep()
      raise CommandError('Exception executing command %s' % ' '.join(cmd))
    return process.returncode


class ClankCompiler(object):
  """Handles compilation of clank."""

  def __init__(self, out_dir, step_recorder, arch, jobs, max_load, use_goma,
               goma_dir, system_health_profiling, monochrome):
    self._out_dir = out_dir
    self._step_recorder = step_recorder
    self._arch = arch
    self._jobs = jobs
    self._max_load = max_load
    self._use_goma = use_goma
    self._goma_dir = goma_dir
    self._system_health_profiling = system_health_profiling
    if monochrome:
      self._apk = 'Monochrome.apk'
      self._apk_target = 'monochrome_apk'
      self._libname = 'libmonochrome'
      self._libchrome_target = 'monochrome'
    else:
      self._apk = 'Chrome.apk'
      self._apk_target = 'chrome_apk'
      self._libname = 'libchrome'
      self._libchrome_target = 'libchrome'

    self.obj_dir = os.path.join(self._out_dir, 'Release', 'obj')
    self.lib_chrome_so = os.path.join(
        self._out_dir, 'Release', 'lib.unstripped',
        '{}.so'.format(self._libname))
    self.chrome_apk = os.path.join(self._out_dir, 'Release', 'apks', self._apk)

  def Build(self, instrumented, target):
    """Builds the provided ninja target with or without order_profiling on.

    Args:
      instrumented: (bool) Whether we want to build an instrumented binary.
      target: (str) The name of the ninja target to build.
    """
    self._step_recorder.BeginStep('Compile %s' % target)

    # Set the "Release Official" flavor, the parts affecting performance.
    args = [
        'enable_resource_whitelist_generation=false',
        'is_chrome_branded=true',
        'is_debug=false',
        'is_official_build=true',
        # We have to build with no symbols if profiling and minimal symbols
        # otherwise for libchrome.so to fit under the 4 GB limit.
        # crbug.com/574476
        'symbol_level=' + ('0' if instrumented else '1'),
        'target_cpu="' + self._arch + '"',
        'target_os="android"',
        'use_goma=' + str(self._use_goma).lower(),
        'use_order_profiling=' + str(instrumented).lower(),
    ]
    if self._goma_dir:
      args += ['goma_dir="%s"' % self._goma_dir]
    if self._system_health_profiling:
      args += ['devtools_instrumentation_dumping = ' +
               str(instrumented).lower()]

    self._step_recorder.RunCommand(
        ['gn', 'gen', os.path.join(self._out_dir, 'Release'),
         '--args=' + ' '.join(args)])

    self._step_recorder.RunCommand(
        ['ninja', '-C', os.path.join(self._out_dir, 'Release'),
         '-j' + str(self._jobs), '-l' + str(self._max_load), target])

  def CompileChromeApk(self, instrumented, force_relink=False):
    """Builds a Chrome.apk either with or without order_profiling on.

    Args:
      instrumented: (bool) Whether to build an instrumented apk.
      force_relink: Whether libchromeview.so should be re-created.
    """
    if force_relink:
      self._step_recorder.RunCommand(['rm', '-rf', self.lib_chrome_so])
    self.Build(instrumented, self._apk_target)

  def CompileLibchrome(self, instrumented, force_relink=False):
    """Builds a libchrome.so either with or without order_profiling on.

    Args:
      instrumented: (bool) Whether to build an instrumented apk.
      force_relink: (bool) Whether libchrome.so should be re-created.
    """
    if force_relink:
      self._step_recorder.RunCommand(['rm', '-rf', self.lib_chrome_so])
    self.Build(instrumented, self._libchrome_target)


class OrderfileUpdater(object):
  """Handles uploading and committing a new orderfile in the repository.

  Only used for testing or on a bot.
  """

  _CLOUD_STORAGE_BUCKET_FOR_DEBUG = None
  _CLOUD_STORAGE_BUCKET = None
  _UPLOAD_TO_CLOUD_COMMAND = 'upload_to_google_storage.py'

  def __init__(self, repository_root, step_recorder, branch, netrc):
    """Constructor.

    Args:
      repository_root: (str) Root of the target repository.
      step_recorder: (StepRecorder) Step recorder, for logging.
      branch: (str) Branch to commit to.
      netrc: (str) Path to the .netrc file to use.
    """
    self._repository_root = repository_root
    self._step_recorder = step_recorder
    self._branch = branch
    self._netrc = netrc

  def CommitFileHashes(self, unpatched_orderfile_filename, orderfile_filename):
    """Commits unpatched and patched orderfiles hashes, if provided.

    Files must have been successfilly uploaded to cloud storage first.

    Args:
      unpatched_orderfile_filename: (str or None) Unpatched orderfile path.
      orderfile_filename: (str or None) Orderfile path.

    Raises:
      NotImplementedError when the commit logic hasn't been overriden.
    """
    files_to_commit = []
    commit_message_lines = ['Update Orderfile.']
    for filename in [unpatched_orderfile_filename, orderfile_filename]:
      if not filename:
        continue
      (relative_path, sha1) = self._GetHashFilePathAndContents(filename)
      commit_message_lines.append('Profile: %s: %s' % (
          os.path.basename(relative_path), sha1))
      files_to_commit.append(relative_path)
    if files_to_commit:
      self._CommitFiles(files_to_commit, commit_message_lines)

  def UploadToCloudStorage(self, filename, use_debug_location):
    """Uploads a file to cloud storage.

    Args:
      filename: (str) File to upload.
      use_debug_location: (bool) Whether to use the debug location.
    """
    bucket = (self._CLOUD_STORAGE_BUCKET_FOR_DEBUG if use_debug_location
              else self._CLOUD_STORAGE_BUCKET)
    extension = _GetFileExtension(filename)
    cmd = [self._UPLOAD_TO_CLOUD_COMMAND, '--bucket', bucket]
    if extension:
      cmd.extend(['-z', extension])
    cmd.append(filename)
    self._step_recorder.RunCommand(cmd)
    print 'Download: https://sandbox.google.com/storage/%s/%s' % (
        bucket, _GenerateHash(filename))

  def _GetHashFilePathAndContents(self, filename):
    """Gets the name and content of the hash file created from uploading the
    given file.

    Args:
      filename: (str) The file that was uploaded to cloud storage.

    Returns:
      A tuple of the hash file name, relative to the reository root, and the
      content, which should be the sha1 hash of the file
      ('base_file.sha1', hash)
    """
    abs_hash_filename = filename + '.sha1'
    rel_hash_filename = os.path.relpath(
        abs_hash_filename, self._repository_root)
    with open(abs_hash_filename, 'r') as f:
      return (rel_hash_filename, f.read())

  def _CommitFiles(self, files_to_commit, commit_message_lines):
    """Commits a list of files, with a given message."""
    raise NotImplementedError


class OrderfileGenerator(object):
  """A utility for generating a new orderfile for Clank.

  Builds an instrumented binary, profiles a run of the application, and
  generates an updated orderfile.
  """
  _CLANK_REPO = os.path.join(constants.DIR_SOURCE_ROOT, 'clank')
  _CHECK_ORDERFILE_SCRIPT = os.path.join(
      constants.DIR_SOURCE_ROOT, 'tools', 'cygprofile', 'check_orderfile.py')
  _BUILD_ROOT = os.path.abspath(os.path.dirname(os.path.dirname(
      constants.GetOutDirectory())))  # Normally /path/to/src

  _UNPATCHED_ORDERFILE_FILENAME = os.path.join(
      _CLANK_REPO, 'orderfiles', 'unpatched_orderfile.%s')

  _PATH_TO_ORDERFILE = os.path.join(_CLANK_REPO, 'orderfiles',
                                    'orderfile.%s.out')

  # Previous orderfile_generator debug files would be overwritten.
  _DIRECTORY_FOR_DEBUG_FILES = '/tmp/orderfile_generator_debug_files'

  def _GetPathToOrderfile(self):
    """Gets the path to the architecture-specific orderfile."""
    return self._PATH_TO_ORDERFILE % self._options.arch

  def _GetUnpatchedOrderfileFilename(self):
    """Gets the path to the architecture-specific unpatched orderfile."""
    return self._UNPATCHED_ORDERFILE_FILENAME % self._options.arch

  def __init__(self, options, orderfile_updater_class):
    self._options = options

    self._instrumented_out_dir = os.path.join(
        self._BUILD_ROOT, self._options.arch + '_instrumented_out')
    self._uninstrumented_out_dir = os.path.join(
        self._BUILD_ROOT, self._options.arch + '_uninstrumented_out')

    if options.profile:
      output_directory = os.path.join(self._instrumented_out_dir, 'Release')
      host_profile_dir = os.path.join(output_directory, 'profile_data')
      urls = [profile_android_startup.AndroidProfileTool.TEST_URL]
      use_wpr = True
      simulate_user = False
      urls = options.urls
      use_wpr = not options.no_wpr
      simulate_user = options.simulate_user
      self._profiler = profile_android_startup.AndroidProfileTool(
          output_directory, host_profile_dir, use_wpr, urls, simulate_user,
          device=options.device)
      if options.pregenerated_profiles:
        self._profiler.SetPregeneratedProfiles(
            glob.glob(options.pregenerated_profiles))
    else:
      assert not options.pregenerated_profiles, (
          '--pregenerated-profiles cannot be used with --skip-profile')
      assert not options.profile_save_dir, (
          '--profile-save-dir cannot be used with --skip-profile')

    self._output_data = {}
    self._step_recorder = StepRecorder(options.buildbot)
    self._compiler = None
    assert issubclass(orderfile_updater_class, OrderfileUpdater)
    self._orderfile_updater = orderfile_updater_class(self._CLANK_REPO,
                                                      self._step_recorder,
                                                      options.branch,
                                                      options.netrc)
    assert os.path.isdir(constants.DIR_SOURCE_ROOT), 'No src directory found'
    symbol_extractor.SetArchitecture(options.arch)

  @staticmethod
  def _RemoveBlanks(src_file, dest_file):
    """A utility to remove blank lines from a file.

    Args:
      src_file: The name of the file to remove the blanks from.
      dest_file: The name of the file to write the output without blanks.
    """
    assert src_file != dest_file, 'Source and destination need to be distinct'

    try:
      src = open(src_file, 'r')
      dest = open(dest_file, 'w')
      for line in src:
        if line and not line.isspace():
          dest.write(line)
    finally:
      src.close()
      dest.close()

  def _GenerateAndProcessProfile(self):
    """Invokes a script to merge the per-thread traces into one file.

    The produced list of offsets is saved in
    self._GetUnpatchedOrderfileFilename().
    """
    self._step_recorder.BeginStep('Generate Profile Data')
    files = []
    logging.getLogger().setLevel(logging.DEBUG)
    if self._options.system_health_orderfile:
      files = self._profiler.CollectSystemHealthProfile(
          self._compiler.chrome_apk)
      self._MaybeSaveProfile(files)
      try:
        self._ProcessPhasedOrderfile(files)
      except Exception:
        for f in files:
          self._SaveForDebugging(f)
        self._SaveForDebugging(self._compiler.lib_chrome_so)
        raise
      finally:
        self._profiler.Cleanup()
    else:
      self._CollectLegacyProfile()
    logging.getLogger().setLevel(logging.INFO)

  def _ProcessPhasedOrderfile(self, files):
    """Process the phased orderfiles produced by system health benchmarks.

    The offsets will be placed in _GetUnpatchedOrderfileFilename().

    Args:
      file: Profile files pulled locally.
    """
    self._step_recorder.BeginStep('Process Phased Orderfile')
    profiles = process_profiles.ProfileManager(files)
    processor = process_profiles.SymbolOffsetProcessor(
        self._compiler.lib_chrome_so)
    phaser = phased_orderfile.PhasedAnalyzer(profiles, processor)
    if self._options.offsets_for_memory:
      profile_offsets = phaser.GetOffsetsForMemoryFootprint()
    else:
      profile_offsets = phaser.GetOffsetsForStartup()
    self._output_data['orderfile_size'] = {
        'startup_kib': processor.OffsetsPrimarySize(
            profile_offsets.startup) / 1024,
        'common_kib': processor.OffsetsPrimarySize(
            profile_offsets.common) / 1024,
        'interaction_kib': processor.OffsetsPrimarySize(
            profile_offsets.interaction) / 1024}

    offsets_list = (profile_offsets.startup +
                    profile_offsets.common +
                    profile_offsets.interaction)
    ordered_symbols = processor.GetOrderedSymbols(offsets_list)
    if not ordered_symbols:
      raise Exception('Failed to get ordered symbols')
    with open(self._GetUnpatchedOrderfileFilename(), 'w') as orderfile:
      orderfile.write('\n'.join(ordered_symbols))

  def _CollectLegacyProfile(self):
    files = []
    try:
      files = self._profiler.CollectProfile(
          self._compiler.chrome_apk,
          constants.PACKAGE_INFO['chrome'])
      self._MaybeSaveProfile(files)
      self._step_recorder.BeginStep('Process profile')
      assert os.path.exists(self._compiler.lib_chrome_so)
      offsets = process_profiles.GetReachedOffsetsFromDumpFiles(
          files, self._compiler.lib_chrome_so)
      if not offsets:
        raise Exception('No profiler offsets found in {}'.format(
            '\n'.join(files)))
      processor = process_profiles.SymbolOffsetProcessor(
          self._compiler.lib_chrome_so)
      ordered_symbols = processor.GetOrderedSymbols(offsets)
      if not ordered_symbols:
        raise Exception('No symbol names from  offsets found in {}'.format(
            '\n'.join(files)))
      with open(self._GetUnpatchedOrderfileFilename(), 'w') as orderfile:
        orderfile.write('\n'.join(ordered_symbols))
    except Exception:
      for f in files:
        self._SaveForDebugging(f)
      raise
    finally:
      self._profiler.Cleanup()

  def _MaybeSaveProfile(self, files):
    if self._options.profile_save_dir:
      logging.info('Saving profiles to %s', self._options.profile_save_dir)
      for f in files:
        shutil.copy(f, self._options.profile_save_dir)
        logging.info('Saved profile %s', f)

  def _PatchOrderfile(self):
    """Patches the orderfile using clean version of libchrome.so."""
    self._step_recorder.BeginStep('Patch Orderfile')
    patch_orderfile.GeneratePatchedOrderfile(
        self._GetUnpatchedOrderfileFilename(), self._compiler.lib_chrome_so,
        self._GetPathToOrderfile())

  def _VerifySymbolOrder(self):
    self._step_recorder.BeginStep('Verify Symbol Order')
    return_code = self._step_recorder.RunCommand(
        [self._CHECK_ORDERFILE_SCRIPT, self._compiler.lib_chrome_so,
         self._GetPathToOrderfile(),
         '--target-arch=' + self._options.arch],
        constants.DIR_SOURCE_ROOT,
        raise_on_error=False)
    if return_code:
      self._step_recorder.FailStep('Orderfile check returned %d.' % return_code)

  def _RecordHash(self, file_name):
    """Records the hash of the file into the output_data dictionary."""
    self._output_data[os.path.basename(file_name) + '.sha1'] = _GenerateHash(
        file_name)

  def _SaveFileLocally(self, file_name, file_sha1):
    """Saves the file to a temporary location and prints the sha1sum."""
    if not os.path.exists(self._DIRECTORY_FOR_DEBUG_FILES):
      os.makedirs(self._DIRECTORY_FOR_DEBUG_FILES)
    shutil.copy(file_name, self._DIRECTORY_FOR_DEBUG_FILES)
    print 'File: %s, saved in: %s, sha1sum: %s' % (
        file_name, self._DIRECTORY_FOR_DEBUG_FILES, file_sha1)

  def _SaveForDebugging(self, filename):
    """Uploads the file to cloud storage or saves to a temporary location."""
    file_sha1 = _GenerateHash(filename)
    if not self._options.buildbot:
      self._SaveFileLocally(filename, file_sha1)
    else:
      print 'Uploading file for debugging: ' + filename
      self._orderfile_updater.UploadToCloudStorage(
          filename, use_debug_location=True)

  def _SaveForDebuggingWithOverwrite(self, file_name):
    """Uploads and overwrites the file in cloud storage or copies locally.

    Should be used for large binaries like lib_chrome_so.

    Args:
      file_name: (str) File to upload.
    """
    file_sha1 = _GenerateHash(file_name)
    if not self._options.buildbot:
      self._SaveFileLocally(file_name, file_sha1)
    else:
      print 'Uploading file for debugging: %s, sha1sum: %s' % (
          file_name, file_sha1)
      upload_location = '%s/%s' % (
          self._CLOUD_STORAGE_BUCKET_FOR_DEBUG, os.path.basename(file_name))
      self._step_recorder.RunCommand([
          'gsutil.py', 'cp', file_name, 'gs://' + upload_location])
      print ('Uploaded to: https://sandbox.google.com/storage/' +
             upload_location)

  def _MaybeArchiveOrderfile(self, filename):
    """In buildbot configuration, uploads the generated orderfile to
    Google Cloud Storage.

    Args:
      filename: (str) Orderfile to upload.
    """
    # First compute hashes so that we can download them later if we need to
    self._step_recorder.BeginStep('Compute hash for ' + filename)
    self._RecordHash(filename)
    if self._options.buildbot:
      self._step_recorder.BeginStep('Archive ' + filename)
      self._orderfile_updater.UploadToCloudStorage(
          filename, use_debug_location=False)

  def _GetHashFilePathAndContents(self, base_file):
    """Gets the name and content of the hash file created from uploading the
    given file.

    Args:
      base_file: The file that was uploaded to cloud storage.

    Returns:
      A tuple of the hash file name, relative to the clank repo path, and the
      content, which should be the sha1 hash of the file
      ('base_file.sha1', hash)
    """
    abs_file_name = base_file + '.sha1'
    rel_file_name = os.path.relpath(abs_file_name, self._CLANK_REPO)
    with open(abs_file_name, 'r') as f:
      return (rel_file_name, f.read())

  def Generate(self):
    """Generates and maybe upload an order."""
    profile_uploaded = False
    orderfile_uploaded = False

    assert (bool(self._options.profile) ^
            bool(self._options.manual_symbol_offsets))
    if self._options.system_health_orderfile and not self._options.profile:
      raise AssertionError('--system_health_orderfile must be not be used '
                           'with --skip-profile')
    if (self._options.manual_symbol_offsets and
        not self._options.system_health_orderfile):
      raise AssertionError('--manual-symbol-offsets must be used with '
                           '--system_health_orderfile.')

    if self._options.profile:
      try:
        _UnstashOutputDirectory(self._instrumented_out_dir)
        self._compiler = ClankCompiler(
            self._instrumented_out_dir,
            self._step_recorder, self._options.arch, self._options.jobs,
            self._options.max_load, self._options.use_goma,
            self._options.goma_dir, self._options.system_health_orderfile,
            self._options.monochrome)
        self._compiler.CompileChromeApk(True)
        self._GenerateAndProcessProfile()
        self._MaybeArchiveOrderfile(self._GetUnpatchedOrderfileFilename())
        profile_uploaded = True
      finally:
        _StashOutputDirectory(self._instrumented_out_dir)
    elif self._options.manual_symbol_offsets:
      assert self._options.manual_libname
      assert self._options.manual_objdir
      with file(self._options.manual_symbol_offsets) as f:
        symbol_offsets = [int(x) for x in f.xreadlines()]
      processor = process_profiles.SymbolOffsetProcessor(
          self._options.manual_libname)
      generator = cyglog_to_orderfile.OffsetOrderfileGenerator(
          processor, cyglog_to_orderfile.ObjectFileProcessor(
              self._options.manual_objdir))
      ordered_sections = generator.GetOrderedSections(symbol_offsets)
      if not ordered_sections:  # Either None or empty is a problem.
        raise Exception('Failed to get ordered sections')
      with open(self._GetUnpatchedOrderfileFilename(), 'w') as orderfile:
        orderfile.write('\n'.join(ordered_sections))

    if self._options.patch:
      if self._options.profile:
        self._RemoveBlanks(self._GetUnpatchedOrderfileFilename(),
                           self._GetPathToOrderfile())
      try:
        _UnstashOutputDirectory(self._uninstrumented_out_dir)
        self._compiler = ClankCompiler(
            self._uninstrumented_out_dir, self._step_recorder,
            self._options.arch, self._options.jobs, self._options.max_load,
            self._options.use_goma, self._options.goma_dir,
            self._options.system_health_orderfile, self._options.monochrome)
        self._compiler.CompileLibchrome(False)
        self._PatchOrderfile()
        # Because identical code folding is a bit different with and without
        # the orderfile build, we need to re-patch the orderfile with code
        # folding as close to the final version as possible.
        self._compiler.CompileLibchrome(False, force_relink=True)
        self._PatchOrderfile()
        self._compiler.CompileLibchrome(False, force_relink=True)
        self._VerifySymbolOrder()
        self._MaybeArchiveOrderfile(self._GetPathToOrderfile())
      finally:
        _StashOutputDirectory(self._uninstrumented_out_dir)
      orderfile_uploaded = True

    if (self._options.buildbot and self._options.netrc
        and not self._step_recorder.ErrorRecorded()):
      unpatched_orderfile_filename = (
          self._GetUnpatchedOrderfileFilename() if profile_uploaded else None)
      orderfile_filename = (
          self._GetPathToOrderfile() if orderfile_uploaded else None)
      self._orderfile_updater.CommitFileHashes(
          unpatched_orderfile_filename, orderfile_filename)

    self._step_recorder.EndStep()
    return not self._step_recorder.ErrorRecorded()

  def GetReportingData(self):
    """Get a dictionary of reporting data (timings, output hashes)"""
    self._output_data['timings'] = self._step_recorder.timings
    return self._output_data


def CreateArgumentParser():
  """Creates and returns the argument parser."""
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--buildbot', action='store_true',
      help='If true, the script expects to be run on a buildbot')
  parser.add_argument(
      '--device', default=None, type=str,
      help='Device serial number on which to run profiling.')
  parser.add_argument(
      '--verify', action='store_true',
      help='If true, the script only verifies the current orderfile')
  parser.add_argument('--target-arch', action='store', dest='arch',
                      default=cygprofile_utils.DetectArchitecture(),
                      choices=['arm', 'arm64', 'x86', 'x86_64', 'x64', 'mips'],
                      help='The target architecture for which to build')
  parser.add_argument('--output-json', action='store', dest='json_file',
                      help='Location to save stats in json format')
  parser.add_argument(
      '--skip-profile', action='store_false', dest='profile', default=True,
      help='Don\'t generate a profile on the device. Only patch from the '
      'existing profile.')
  parser.add_argument(
      '--skip-patch', action='store_false', dest='patch', default=True,
      help='Only generate the raw (unpatched) orderfile, don\'t patch it.')
  parser.add_argument(
      '--netrc', action='store',
      help='A custom .netrc file to use for git checkin. Only used on bots.')
  parser.add_argument(
      '--branch', action='store', default='master',
      help='When running on buildbot with a netrc, the branch orderfile '
      'hashes get checked into.')
  # Note: -j50 was causing issues on the bot.
  parser.add_argument(
      '-j', '--jobs', action='store', default=20,
      help='Number of jobs to use for compilation.')
  parser.add_argument(
      '-l', '--max-load', action='store', default=4, help='Max cpu load.')
  parser.add_argument('--goma-dir', help='GOMA directory.')
  parser.add_argument(
      '--use-goma', action='store_true', help='Enable GOMA.', default=False)
  parser.add_argument('--adb-path', help='Path to the adb binary.')

  parser.add_argument('--system-health-orderfile', action='store_true',
                      dest='system_health_orderfile', default=False,
                      help=('Create an orderfile based on an about:blank '
                            'startup benchmark instead of system health '
                            'benchmarks.'))
  parser.add_argument('--monochrome', action='store_true',
                      help=('Compile and instrument monochrome (for post-N '
                            'devices).'))
  parser.add_argument('--offsets-for-memory', action='store_true',
                      help=('Favor memory savings in the orderfile. Used '
                            'with --system-health-orderfile.'),
                      default=False)

  parser.add_argument('--manual-symbol-offsets', default=None, type=str,
                      help=('File of list of ordered symbol offsets generated '
                            'by manual profiling. Must set other --manual* '
                            'flags if this is used, and must --skip-profile.'))
  parser.add_argument('--manual-libname', default=None, type=str,
                      help=('Library filename corresponding to '
                            '--manual-symbol-offsets.'))
  parser.add_argument('--manual-objdir', default=None, type=str,
                      help=('Root of object file directory corresponding to '
                            '--manual-symbol-offsets.'))
  parser.add_argument('--pregenerated-profiles', default=None, type=str,
                      help=('Pregenerated profiles to use instead of running '
                            'profile step. Cannot be used with '
                            '--skip-profiles.'))
  parser.add_argument('--profile-save-dir', default=None, type=str,
                      help=('Directory to save any profiles created. These can '
                            'be used with --pregenerated-profiles.  Cannot be '
                            'used with --skip-profiles.'))

  profile_android_startup.AddProfileCollectionArguments(parser)
  return parser


def CreateOrderfile(options, orderfile_updater_class):
  """Creates an oderfile.

  Args:
    options: As returned from optparse.OptionParser.parse_args()
    orderfile_updater_class: (OrderfileUpdater) subclass of OrderfileUpdater.

  Returns:
    True iff success.
  """
  logging.basicConfig(level=logging.INFO)
  devil_chromium.Initialize(adb_path=options.adb_path)

  generator = OrderfileGenerator(options, orderfile_updater_class)
  try:
    if options.verify:
      generator._VerifySymbolOrder()
    else:
      return generator.Generate()
  finally:
    json_output = json.dumps(generator.GetReportingData(),
                             indent=2) + '\n'
    if options.json_file:
      with open(options.json_file, 'w') as f:
        f.write(json_output)
    print json_output
  return False


def main():
  parser = CreateArgumentParser()
  options = parser.parse_args()
  return 0 if CreateOrderfile(options, OrderfileUpdater) else 1


if __name__ == '__main__':
  sys.exit(main())
