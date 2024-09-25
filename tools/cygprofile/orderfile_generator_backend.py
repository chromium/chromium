#!/usr/bin/env vpython3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" A utility to generate an orderfile.

The orderfile is used by the linker to order symbols such that they
are placed consecutively. See //docs/orderfile.md.

Example usage:
  tools/cygprofile/orderfile_generator_backend.py --use-remoteexec \
    --target-arch=arm64
"""


import argparse
import ast
import csv
import hashlib
import json
import glob
import logging
import os
import pathlib
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from typing import Dict, List

import cluster
import process_profiles
import android_profile_tool

_SRC_PATH = pathlib.Path(__file__).resolve().parents[2]
sys.path.append(str(_SRC_PATH / 'third_party/catapult/devil'))
from devil.android import device_utils
from devil.android.sdk import version_codes

sys.path.append(str(_SRC_PATH / 'build/android'))
import devil_chromium

_OUT_PATH = _SRC_PATH / 'out'

# Architecture specific GN args. Trying to build an orderfile for an
# architecture not listed here will eventually throw.
_ARCH_GN_ARGS = {
    'arm': ['target_cpu="arm"'],
    # Does not work on the bot: https://crbug.com/41490637
    'arm64': ['target_cpu="arm64"'],
    'x86': ['target_cpu="x86"'],
    # Telemetry does not work with x64 yet: https://crbug.com/327791269
    'x64': ['target_cpu="x64"'],
}

_RESULTS_KEY_SPEEDOMETER = 'Speedometer2.0'


def _ReadNonEmptyStrippedFromFile(file_name):
  stripped_lines = []
  with open(file_name, 'r') as file:
    for line in file:
      stripped_line = line.strip()
      if stripped_line:
        stripped_lines.append(stripped_line)
  return stripped_lines


class CommandError(Exception):
  """Indicates that a dispatched shell command exited with a non-zero status."""

  def __init__(self, value):
    super().__init__()
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
  return None


class StepRecorder:
  """Records steps and timings."""

  def __init__(self):
    self.timings = []
    self._previous_step = ('', 0.0)
    self._error_recorded = False

  def BeginStep(self, name):
    """Marks a beginning of the next step in the generator.

    Args:
      name: The name of the step.
    """
    self.EndStep()
    self._previous_step = (name, time.time())
    logging.info('Running step: %s', name)

  def EndStep(self):
    """Records successful completion of the current step.

    This is optional if the step is immediately followed by another BeginStep.
    """
    if self._previous_step[0]:
      elapsed = time.time() - self._previous_step[1]
      logging.info('Step %s took %f seconds', self._previous_step[0], elapsed)
      self.timings.append((self._previous_step[0], elapsed))

    self._previous_step = ('', 0.0)

  def FailStep(self, message=None):
    """Marks that a particular step has failed.

    Also prints an optional failure message.

    Args:
      message: An optional explanation as to why the step failed.
    """
    logging.error('STEP FAILED!!')
    if message:
      logging.error(message)
    self._error_recorded = True
    self.EndStep()

  def ErrorRecorded(self):
    """True if FailStep has been called."""
    return self._error_recorded

  def RunCommand(self,
                 cmd: List[str],
                 cwd: pathlib.Path = _SRC_PATH,
                 raise_on_error: bool = True,
                 capture_output: bool = False) -> subprocess.CompletedProcess:
    """Execute a shell command.

    Args:
      cmd: A list of command strings.
      cwd: Directory in which the command should be executed, defaults to build
           root of script's location if not specified.
      raise_on_error: If true will raise a CommandError if the call doesn't
          succeed and mark the step as failed.

    Returns:
      A CompletedProcess instance.

    Raises:
      CommandError: An error executing the specified command.
    """
    logging.info('Executing %s in %s', ' '.join(cmd), cwd)
    process = subprocess.run(
        cmd,
        capture_output=capture_output,
        check=False,  # This allows for raise_on_error.
        text=True,
        cwd=cwd,
        env=os.environ)
    if raise_on_error and process.returncode != 0:
      if capture_output:
        self.FailStep(str(process.stdout) + str(process.stderr))
      else:
        self.FailStep()
      raise CommandError('Exception executing command %s' % ' '.join(cmd))
    if capture_output:
      logging.error('Output:\n%s', process.stdout)
    return process


class NativeLibraryBuildVariant:
  """Native library build versions.
  See https://chromium.googlesource.com/chromium/src/+/HEAD/docs/android_native_libraries.md # pylint: disable=line-too-long
  for more details.
  """
  MONOCHROME = 0
  TRICHROME = 1


class ClankCompiler:
  """Handles compilation of clank."""

  def __init__(self, out_dir: pathlib.Path, step_recorder: StepRecorder,
               options, orderfile_location, native_library_build_variant):
    self._out_dir = out_dir
    self._step_recorder = step_recorder
    self._options = options
    self._orderfile_location = orderfile_location
    self._native_library_build_variant = native_library_build_variant

    self._ninja_command = ['autoninja']
    if options.ninja_path:
      self._ninja_command = [options.ninja_path]
      if self._options.ninja_j:
        self._ninja_command += ['-j', options.ninja_j]
    self._ninja_command += ['-C']

    # WebView targets
    self._webview_target, webview_apk = self._GetWebViewTargetAndApk(
        native_library_build_variant, options.public, options.arch)
    self.webview_apk_path = str(out_dir / 'apks' / webview_apk)
    self.webview_installer_path = str(self._out_dir / 'bin' /
                                      self._webview_target)

    # Chrome targets
    self._chrome_target, chrome_apk = self._GetChromeTargetAndApk(
        options.public)
    self.chrome_apk_path = str(out_dir / 'apks' / chrome_apk)

    self._libchrome_target = self._GetLibchromeTarget(options.arch)
    self.lib_chrome_so = str(out_dir /
                             f'lib.unstripped/{self._libchrome_target}.so')

  def _GenerateGnArgs(self, instrumented):
    # Set the "Release Official" flavor, the parts affecting performance.
    gn_args = [
        'enable_resource_allowlist_generation=false',
        'is_chrome_branded=' + str(not self._options.public).lower(),
        'is_debug=false',
        'is_official_build=true',
        'symbol_level=1',  # to fit 30 GiB RAM on the bot when LLD is running
        'target_os="android"',
        'enable_proguard_obfuscation=false',  # More debuggable stacktraces.
        'use_siso=' + str(self._options.use_siso).lower(),
        'use_remoteexec=' + str(self._options.use_remoteexec).lower(),
        'use_order_profiling=' + str(instrumented).lower(),
        'devtools_instrumentation_dumping=' + str(instrumented).lower()
    ]
    gn_args += _ARCH_GN_ARGS[self._options.arch]

    if os.path.exists(self._orderfile_location):
      # GN needs the orderfile path to be source-absolute.
      src_abs_orderfile = os.path.relpath(self._orderfile_location, _SRC_PATH)
      gn_args += ['chrome_orderfile_path="//{}"'.format(src_abs_orderfile)]
    return gn_args

  def _Build(self, instrumented, target):
    """Builds the provided ninja target with or without order_profiling on.

    Args:
      instrumented: (bool) Whether we want to build an instrumented binary.
      target: (str) The name of the ninja target to build.
    """
    self._step_recorder.BeginStep('Compile %s' % target)
    gn_args = self._GenerateGnArgs(instrumented)
    self._step_recorder.RunCommand(
        ['gn', 'gen',
         str(self._out_dir), '--args=' + ' '.join(gn_args)])
    self._step_recorder.RunCommand(self._ninja_command +
                                   [str(self._out_dir), target])

  def _ForceRelink(self):
    """Forces libmonochrome.so to be re-linked.

    With partitioned libraries enabled, deleting these library files does not
    guarantee they'll be recreated by the linker (they may simply be
    re-extracted from a combined library). To be safe, touch a source file
    instead. See http://crbug.com/972701 for more explanation.
    """
    file_to_touch = _SRC_PATH / 'chrome/browser/chrome_browser_main_android.cc'
    assert file_to_touch.exists()
    self._step_recorder.RunCommand(['touch', str(file_to_touch)])

  def CompileWebViewApk(self, instrumented, force_relink=False):
    """Builds a SystemWebView.apk either with or without order_profiling on.

    Args:
      instrumented: (bool) Whether to build an instrumented apk.
      force_relink: Whether libchromeview.so should be re-created.
    """
    if force_relink:
      self._ForceRelink()
    self._Build(instrumented, self._webview_target)

  def CompileChromeApk(self, instrumented, force_relink=False):
    """Builds a Chrome.apk either with or without order_profiling on.

    Args:
      instrumented: (bool) Whether to build an instrumented apk.
      force_relink: Whether libchromeview.so should be re-created.
    """
    if force_relink:
      self._ForceRelink()
    self._Build(instrumented, self._chrome_target)

  def CompileLibchrome(self, instrumented, force_relink=False):
    """Builds a libmonochrome.so either with or without order_profiling on.

    Args:
      instrumented: (bool) Whether to build an instrumented apk.
      force_relink: (bool) Whether libmonochrome.so should be re-created.
    """
    if force_relink:
      self._ForceRelink()
    self._Build(instrumented, self._libchrome_target)

  @staticmethod
  def _MakePublicTarget(internal_target):
    if 'google' in internal_target:
      return internal_target.replace('_google', '')
    return internal_target.replace('_apk', '_public_apk')

  @staticmethod
  def _GetApkFromTarget(target):
    _camel_case = ''.join(x.capitalize() for x in target.lower().split('_'))
    _camel_case = _camel_case.replace('Webview', 'WebView')
    return _camel_case.replace('Apk', '.apk')

  @staticmethod
  def _GetWebViewTargetAndApk(native_library_build_variant, public, arch):
    target = 'trichrome_webview_google_apk'
    if native_library_build_variant == NativeLibraryBuildVariant.MONOCHROME:
      target = 'monochrome_apk'
    if public:
      target = ClankCompiler._MakePublicTarget(target)
    apk = ClankCompiler._GetApkFromTarget(target)
    if 'Trichrome' in apk and '64' in arch:
      # Trichrome has a 6432.apk suffix for arm64 and x64 builds.
      apk = apk.replace('.apk', '6432.apk')
    return target, apk

  @staticmethod
  def _GetChromeTargetAndApk(public):
    target = 'monochrome_apk'
    if public:
      target = ClankCompiler._MakePublicTarget(target)
    return target, ClankCompiler._GetApkFromTarget(target)

  @staticmethod
  def _GetLibchromeTarget(arch):
    target = 'libmonochrome'
    if '64' in arch:
      # Monochrome has a _64 suffix for arm64 and x64 builds.
      return target + '_64'
    return target


class OrderfileUpdater:
  """Handles uploading and committing a new orderfile in the repository.

  Only used for testing or on a bot.
  """

  _CLOUD_STORAGE_BUCKET_FOR_DEBUG = None
  _CLOUD_STORAGE_BUCKET = None
  _UPLOAD_TO_CLOUD_COMMAND = 'upload_to_google_storage.py'
  _UPLOAD_TO_NEW_CLOUD_COMMAND = 'upload_to_google_storage_first_class.py'

  def __init__(self, repository_root, step_recorder: StepRecorder):
    """Constructor.

    Args:
      repository_root: (str) Root of the target repository.
      step_recorder: (StepRecorder) Step recorder, for logging.
    """
    self._repository_root = repository_root
    self._step_recorder = step_recorder

  def UploadToCloudStorage(self,
                           filename,
                           use_debug_location,
                           use_new_cloud=False):
    """Uploads a file to cloud storage.

    Here's an example of what the JSON object looks like for the new cloud: # pylint: disable=line-too-long
    {
      "path": {
        "dep_type": "gcs",
        "bucket": "orderfile-test",
        "objects": [
          {
            "object_name": "e8e5ffb467e8cd784a7a7fbe8c4e840118306959c4b01c810eb6af9169b4c624",
            "sha256sum": "e8e5ffb467e8cd784a7a7fbe8c4e840118306959c4b01c810eb6af9169b4c624",
            "size_bytes": 32374172,
            "generation": 1715099523335361
          }
        ]
      }
    }
    See https://chromium.googlesource.com/chromium/src.git/+/refs/heads/main/docs/gcs_dependencies.md

    Args:
      filename: (str) File to upload.
      use_debug_location: (bool) Whether to use the debug location.
      use_new_cloud: (bool) Whether to use the new workflow and modify DEPS.
    """
    bucket = (self._CLOUD_STORAGE_BUCKET_FOR_DEBUG if use_debug_location
              else self._CLOUD_STORAGE_BUCKET)
    extension = _GetFileExtension(filename)
    cmd = [self._UPLOAD_TO_CLOUD_COMMAND, '--bucket', bucket]
    if extension:
      cmd.extend(['-z', extension])
    cmd.append(filename)
    # Keep both upload paths working as the upload script updates .sha1 files.
    self._step_recorder.RunCommand(cmd)
    if use_new_cloud:
      logging.info('Uploading using the new cloud:')
      bucket_name, prefix = bucket.split('/', 1)
      new_cmd = [
          self._UPLOAD_TO_NEW_CLOUD_COMMAND, '--bucket', bucket_name,
          '--prefix', prefix
      ]
      if extension:
        new_cmd.extend(['-z', extension])
      new_cmd.append(filename)
      stdout: str = self._step_recorder.RunCommand(new_cmd,
                                                   capture_output=True).stdout
      # The first line is "Uploading ... ", the rest of the lines is valid json.
      json_string = stdout.split('\n', 1)[1]
      logging.info(json_string)
      json_object = json.loads(json_string)['path']['objects'][0]
      logging.info(json_object)
      output_file = os.path.basename(filename)
      logging.info(output_file)
      # Load existing objects to avoid overwriting other arch's objects.
      getdep_cmd = ['gclient', 'getdep', '-r', 'orderfile_binaries']
      dep_str: str = self._step_recorder.RunCommand(getdep_cmd,
                                                    cwd=self._repository_root,
                                                    capture_output=True).stdout
      # dep_str is a python representation of the object, not valid JSON.
      dep_objects = ast.literal_eval(dep_str)
      values = []
      # Same set as depot_tools/gclient.py (CMDsetdep).
      allowed_keys = ['object_name', 'sha256sum', 'size_bytes', 'generation']
      # Order matters here, so preserve the order in dep_objects.
      for dep_object in dep_objects:
        if dep_object['output_file'] == output_file:
          # Use our newly uploaded info to update DEPS.
          dep_object = json_object
          # For 'gcs' deps `gclient setdep` only allows these specific keys.
        values.append(','.join(str(dep_object[k]) for k in allowed_keys))
      setdep_cmd = [
          'gclient', 'setdep', '-r', f'orderfile_binaries@{"?".join(values)}'
      ]
      self._step_recorder.RunCommand(setdep_cmd, cwd=self._repository_root)
    logging.info('Download: https://sandbox.google.com/storage/%s/%s', bucket,
                 _GenerateHash(filename))

  def _GitStash(self):
    """Git stash the current clank tree.

    Raises:
      NotImplementedError when the stash logic hasn't been overridden.
    """
    raise NotImplementedError

  def _CommitStashedFiles(self, expected_files_in_stash):
    """Commits stashed files.

    The local repository is updated and then the files to commit are taken from
    modified files from the git stash. The modified files should be a subset of
    |expected_files_in_stash|. If there are unexpected modified files, this
    function may raise. This is meant to be paired with _GitStash().

    Args:
      expected_files_in_stash: [str] paths to a possible superset of files
        expected to be stashed & committed.

    Raises:
      NotImplementedError when the commit logic hasn't been overridden.
    """
    raise NotImplementedError


class OrderfileGenerator:
  """A utility for generating a new orderfile for Clank.

  Builds an instrumented binary, profiles a run of the application, and
  generates an updated orderfile.
  """
  _CHECK_ORDERFILE_SCRIPT = _SRC_PATH / 'tools/cygprofile/check_orderfile.py'

  # Previous orderfile_generator debug files would be overwritten.
  _DIRECTORY_FOR_DEBUG_FILES = '/tmp/orderfile_generator_debug_files'

  _CLOUD_STORAGE_BUCKET_FOR_DEBUG = None

  def _PrepareOrderfilePaths(self):
    if self._options.public:
      self._clank_dir = _SRC_PATH
    else:
      self._clank_dir = _SRC_PATH / 'clank'
    self._orderfiles_dir = self._clank_dir / 'orderfiles'
    if self._options.profile_webview_startup:
      self._orderfiles_dir = self._orderfiles_dir / 'webview'
    self._orderfiles_dir.mkdir(exist_ok=True)

  def _GetPathToOrderfile(self):
    """Gets the path to the architecture-specific orderfile."""
    return str(self._orderfiles_dir / f'orderfile.{self._options.arch}.out')

  def _GetUnpatchedOrderfileFilename(self):
    """Gets the path to the architecture-specific unpatched orderfile."""
    arch = self._options.arch
    return str(self._orderfiles_dir / f'unpatched_orderfile.{arch}')

  def _SetDevice(self):
    """ Selects the device to be used by the script.

    Returns:
      (Device with given serial ID) : if the --device flag is set.
      (Some connected device) : Otherwise.

    Raises Error:
      If no device meeting the requirements has been found.
    """
    if self._options.device:
      return device_utils.DeviceUtils(self._options.device)

    devices = device_utils.DeviceUtils.HealthyDevices()
    assert devices, 'Expected at least one connected device'

    if (self._native_library_build_variant ==
        NativeLibraryBuildVariant.TRICHROME):
      for device in devices:
        if device.build_version_sdk >= version_codes.Q:
          return device
      raise Exception('No device running Android Q+ found to build trichrome.')

    return devices[0]


  def __init__(self, options, orderfile_updater_class):
    self._options = options
    self._native_library_build_variant = NativeLibraryBuildVariant.TRICHROME
    if options.native_lib_variant == 'monochrome':
      self._native_library_build_variant = NativeLibraryBuildVariant.MONOCHROME
    if options.use_common_out_dir_for_instrumented:
      assert options.buildbot, ('--use-common-out-dir-for-instrumented is only '
                                'meant to be used with --buildbot, otherwise '
                                'it will overwrite the local out/Release dir.')
      # This is used on the bot to save the directory for the stack tool. We
      # only save the instrumented out dir since it is needed to deobfuscate the
      # stack trace. The uninstrumented build is used to compare performance on
      # Speedometer with/without orderfile, which is less likely to fail.
      self._instrumented_out_dir = _OUT_PATH / 'Release'
    else:
      self._instrumented_out_dir = (
          _OUT_PATH / f'orderfile_{self._options.arch}_instrumented_out')

    self._uninstrumented_out_dir = (
        _OUT_PATH / f'orderfile_{self._options.arch}_uninstrumented_out')
    self._no_orderfile_out_dir = (
        _OUT_PATH / f'orderfile_{self._options.arch}_no_orderfile_out')

    self._PrepareOrderfilePaths()

    if options.profile:
      self._host_profile_root = _SRC_PATH / 'profile_data'
      device = self._SetDevice()
      self._profiler = android_profile_tool.AndroidProfileTool(
          str(self._host_profile_root),
          device,
          debug=self._options.streamline_for_debugging,
          verbosity=self._options.verbosity)
      if options.pregenerated_profiles:
        self._profiler.SetPregeneratedProfiles(
            glob.glob(options.pregenerated_profiles))
    else:
      assert not options.pregenerated_profiles, (
          '--pregenerated-profiles cannot be used with --skip-profile')
      assert not options.profile_save_dir, (
          '--profile-save-dir cannot be used with --skip-profile')

    self._output_data = {}
    self._step_recorder = StepRecorder()
    self._compiler = None
    if orderfile_updater_class is None:
      orderfile_updater_class = OrderfileUpdater
    assert issubclass(orderfile_updater_class, OrderfileUpdater)
    self._orderfile_updater = orderfile_updater_class(self._clank_dir,
                                                      self._step_recorder)
    assert _SRC_PATH.is_dir(), 'No src directory found'

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

    if self._options.profile_save_dir:
      # The directory must not preexist, to ensure purity of data. Check
      # before profiling to save time.
      if os.path.exists(self._options.profile_save_dir):
        raise Exception('Profile save directory must not pre-exist')

    assert self._compiler is not None, (
        'A valid compiler is needed to generate profiles.')
    if self._options.profile_webview_startup:
      self._profiler.InstallAndSetWebViewProvider(
          self._compiler.webview_installer_path)
      files = self._profiler.CollectWebViewStartupProfile(
          self._compiler.webview_apk_path)
    elif self._options.arch == 'arm64':
      files = self._profiler.CollectSpeedometerProfile(
          self._compiler.chrome_apk_path)
    else:
      files = self._profiler.CollectSystemHealthProfile(
          self._compiler.chrome_apk_path)
    self._MaybeSaveProfile()
    try:
      self._ProcessPhasedOrderfile(files)
    except Exception:
      for f in files:
        self._SaveForDebugging(f)
      self._SaveForDebugging(self._compiler.lib_chrome_so)
      raise
    finally:
      self._profiler.Cleanup()
    logging.getLogger().setLevel(logging.INFO)

  def _ProcessPhasedOrderfile(self, files):
    """Process the phased orderfiles produced by system health benchmarks.

    The offsets will be placed in _GetUnpatchedOrderfileFilename().

    Args:
      file: Profile files pulled locally.
    """
    self._step_recorder.BeginStep('Process Phased Orderfile')
    assert self._compiler is not None
    profiles = process_profiles.ProfileManager(files)
    processor = process_profiles.SymbolOffsetProcessor(
        self._compiler.lib_chrome_so)
    ordered_symbols = cluster.ClusterOffsets(profiles, processor)
    if not ordered_symbols:
      raise Exception('Failed to get ordered symbols')
    for sym in ordered_symbols:
      assert not sym.startswith('OUTLINED_FUNCTION_'), (
          'Outlined function found in instrumented function, very likely '
          'something has gone very wrong!')
    self._output_data['offsets_kib'] = processor.SymbolsSize(
            ordered_symbols) / 1024
    with open(self._GetUnpatchedOrderfileFilename(), 'w') as orderfile:
      orderfile.write('\n'.join(ordered_symbols))

  def _MaybeSaveProfile(self):
    """Saves profiles in self._host_profile_root to --profile-save-dir"""
    if self._options.profile_save_dir:
      logging.info('Saving profiles to %s', self._options.profile_save_dir)
      shutil.copytree(self._host_profile_root, self._options.profile_save_dir)
      logging.info('Saved profiles')

  def _AddDummyFunctions(self):
    # TODO(crbug.com/340534475): Stop writing the `unpatched_orderfile` and
    # uploading it to the cloud storage.
    self._step_recorder.BeginStep('Add dummy functions')
    assert self._compiler is not None
    symbols = _ReadNonEmptyStrippedFromFile(
        self._GetUnpatchedOrderfileFilename())
    with open(self._GetPathToOrderfile(), 'w') as f:
      # Make sure the anchor functions are located in the right place, here and
      # after everything else.
      # See the comment in //base/android/library_loader/anchor_functions.cc.
      f.write('dummy_function_start_of_ordered_text\n')
      for sym in symbols:
        f.write(sym + '\n')
      f.write('dummy_function_end_of_ordered_text\n')

  def _VerifySymbolOrder(self):
    self._step_recorder.BeginStep('Verify Symbol Order')
    assert self._compiler is not None
    cmd = [
        str(self._CHECK_ORDERFILE_SCRIPT), self._compiler.lib_chrome_so,
        self._GetPathToOrderfile()
    ]
    return_code = self._step_recorder.RunCommand(
        cmd, raise_on_error=False).returncode
    if return_code:
      self._step_recorder.FailStep('Orderfile check returned %d.' % return_code)
    return return_code == 0

  def _RecordHash(self, file_name):
    """Records the hash of the file into the output_data dictionary."""
    self._output_data[os.path.basename(file_name) + '.sha1'] = _GenerateHash(
        file_name)

  def _SaveFileLocally(self, file_name, file_sha1):
    """Saves the file to a temporary location and prints the sha1sum."""
    if not os.path.exists(self._DIRECTORY_FOR_DEBUG_FILES):
      os.makedirs(self._DIRECTORY_FOR_DEBUG_FILES)
    shutil.copy(file_name, self._DIRECTORY_FOR_DEBUG_FILES)
    logging.info('File: %s, saved in: %s, sha1sum: %s', file_name,
                 self._DIRECTORY_FOR_DEBUG_FILES, file_sha1)

  def _SaveForDebugging(self, filename: str):
    """Uploads the file to cloud storage or saves to a temporary location."""
    if not self._options.buildbot:
      file_sha1 = _GenerateHash(filename)
      self._SaveFileLocally(filename, file_sha1)
    else:
      logging.info('Uploading file for debugging: %s', filename)
      self._orderfile_updater.UploadToCloudStorage(filename,
                                                   use_debug_location=True)

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
      logging.info('Uploading file for debugging: %s, sha1sum: %s', file_name,
                   file_sha1)
      upload_location = '%s/%s' % (self._CLOUD_STORAGE_BUCKET_FOR_DEBUG,
                                   os.path.basename(file_name))
      self._step_recorder.RunCommand(
          ['gsutil.py', 'cp', file_name, 'gs://' + upload_location])
      logging.info('Uploaded to: https://sandbox.google.com/storage/%s',
                   upload_location)

  def _MaybeArchiveOrderfile(self, filename, use_new_cloud: bool = False):
    """In buildbot configuration, uploads the generated orderfile to
    Google Cloud Storage.

    Args:
      filename: (str) Orderfile to upload.
      use_new_cloud: (bool) Whether to upload using the new flow.
    """
    # First compute hashes so that we can download them later if we need to.
    self._step_recorder.BeginStep('Compute hash for ' + filename)
    self._RecordHash(filename)
    if self._options.buildbot:
      self._step_recorder.BeginStep('Archive ' + filename)
      self._orderfile_updater.UploadToCloudStorage(filename,
                                                   use_debug_location=False,
                                                   use_new_cloud=use_new_cloud)

  def UploadReadyOrderfiles(self):
    self._step_recorder.BeginStep('Upload Ready Orderfiles')
    for file_name in [self._GetUnpatchedOrderfileFilename(),
        self._GetPathToOrderfile()]:
      self._orderfile_updater.UploadToCloudStorage(
          file_name, use_debug_location=False)

  def _WebViewStartupBenchmark(self, apk: str):
    """Runs system_health.webview_startup benchmark.
    Args:
      apk: Path to the apk.
    """
    self._step_recorder.BeginStep("Running system_health.webview_startup")
    try:
      chromium_out_dir = os.path.abspath(
          os.path.join(os.path.dirname(apk), '..'))
      browser = self._profiler._GetBrowserFromApk(apk)
      out_dir = tempfile.mkdtemp()
      self._profiler._RunCommand([
          'tools/perf/run_benchmark', '--device', self._profiler._device.serial,
          '--browser', browser, '--output-format=csv', '--output-dir', out_dir,
          '--chromium-output-directory', chromium_out_dir, '--reset-results',
          'system_health.webview_startup'
      ])
      out_file_path = os.path.join(out_dir, 'results.csv')
      if not os.path.exists(out_file_path):
        raise Exception('Results file not found!')
      results = {}
      with open(out_file_path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
          if not row['name'].startswith('webview_startup'):
            continue
          results.setdefault(row['name'], {}).setdefault(row['stories'],
                                                         []).append(row['avg'])

      if not results:
        raise Exception('Could not find relevant results')

      return results

    except Exception as e:
      return 'Error: ' + str(e)

    finally:
      shutil.rmtree(out_dir)

  def _NativeCodeMemoryBenchmark(self, apk: str):
    """Runs system_health.memory_mobile to assess native code memory footprint.

    Args:
      apk: Path to the apk.

    Returns:
      results: ([int]) Values of native code memory footprint in bytes from the
                       benchmark results.
    """
    self._step_recorder.BeginStep("Running orderfile.memory_mobile")
    try:
      out_dir = tempfile.mkdtemp()
      cmd = [
          'tools/perf/run_benchmark', '--device', self._profiler._device.serial,
          '--browser=exact', '--output-format=csv', '--output-dir', out_dir,
          '--reset-results', '--browser-executable', apk,
          'orderfile.memory_mobile'
      ] + ['-v'] * self._options.verbosity
      self._profiler._RunCommand(cmd)

      out_file_path = os.path.join(out_dir, 'results.csv')
      if not os.path.exists(out_file_path):
        raise Exception('Results file not found!')

      results = {}
      with open(out_file_path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
          if not row['name'].endswith('NativeCodeResidentMemory'):
            continue
          # Note: NativeCodeResidentMemory records a single sample from each
          # story run, so this average (reported as 'avg') is exactly the value
          # of that one sample. Each story is run multiple times, so this loop
          # will accumulate into a list all values for all runs of each story.
          results.setdefault(row['name'], {}).setdefault(
              row['stories'], []).append(row['avg'])

      if not results:
        raise Exception('Could not find relevant results')

      return results

    except Exception as e:
      return 'Error: ' + str(e)

    finally:
      shutil.rmtree(out_dir)

  def _PerformanceBenchmark(self, apk: str) -> List[float]:
    """Runs Speedometer2.0 to assess performance.

    Args:
      apk: Path to the apk.

    Returns:
      results: Speedometer2.0 results samples in milliseconds.
    """
    self._step_recorder.BeginStep("Running Speedometer2.0.")
    try:
      out_dir = tempfile.mkdtemp()
      cmd = [
          'tools/perf/run_benchmark', '--device', self._profiler._device.serial,
          '--browser=exact', '--output-format=histograms', '--output-dir',
          out_dir, '--reset-results', '--browser-executable', apk,
          'speedometer2'
      ] + ['-v'] * self._options.verbosity

      self._profiler._RunCommand(cmd)
      out_file_path = os.path.join(out_dir, 'histograms.json')
      if not os.path.exists(out_file_path):
        raise Exception('Results file not found!')

      with open(out_file_path, 'r') as f:
        results = json.load(f)

      if not results:
        raise Exception('Results file is empty.')

      for el in results:
        if 'name' in el and el['name'] == 'Total' and 'sampleValues' in el:
          return el['sampleValues']

      raise Exception('Unexpected results format.')

    except Exception as e:
      return 'Error: ' + str(e)

    finally:
      shutil.rmtree(out_dir)


  def RunBenchmark(self,
                   out_directory: pathlib.Path,
                   no_orderfile: bool = False) -> Dict:
    """Builds chrome apk and runs performance and memory benchmarks.

    Builds a non-instrumented version of chrome.
    Installs chrome apk on the device.
    Runs Speedometer2.0 benchmark to assess performance.
    Runs system_health.memory_mobile to evaluate memory footprint.

    Args:
      out_directory: Path to out directory for this build.
      no_orderfile: True if chrome to be built without orderfile.

    Returns:
      benchmark_results: Results extracted from benchmarks.
    """
    benchmark_results = {}
    try:
      self._compiler = ClankCompiler(out_directory, self._step_recorder,
                                     self._options, self._GetPathToOrderfile(),
                                     self._native_library_build_variant)

      if no_orderfile:
        orderfile_path = self._GetPathToOrderfile()
        backup_orderfile = orderfile_path + '.backup'
        shutil.move(orderfile_path, backup_orderfile)
        open(orderfile_path, 'w').close()

      # Build APK to be installed on the device.
      self._compiler.CompileChromeApk(instrumented=False,
                                      force_relink=True)
      benchmark_results[_RESULTS_KEY_SPEEDOMETER] = self._PerformanceBenchmark(
          self._compiler.chrome_apk_path)
      benchmark_results['orderfile.memory_mobile'] = (
          self._NativeCodeMemoryBenchmark(self._compiler.chrome_apk_path))
      if self._options.profile_webview_startup:
        self._compiler.CompileWebViewApk(instrumented=False,
                                         force_relink=True)
        self._profiler.InstallAndSetWebViewProvider(
            self._compiler.webview_installer_path)
        benchmark_results[
            'system_health.webview_startup'] = self._WebViewStartupBenchmark(
                self._compiler.webview_apk_path)

    except Exception as e:
      benchmark_results['Error'] = str(e)

    finally:
      if no_orderfile and os.path.exists(backup_orderfile):
        shutil.move(backup_orderfile, orderfile_path)

    return benchmark_results

  def _SaveBenchmarkResultsToOutput(self, with_orderfile_results,
                                    no_orderfile_results):
    self._output_data['orderfile_benchmark_results'] = with_orderfile_results
    self._output_data['no_orderfile_benchmark_results'] = no_orderfile_results
    with_orderfile_samples = with_orderfile_results[_RESULTS_KEY_SPEEDOMETER]
    no_orderfile_samples = no_orderfile_results[_RESULTS_KEY_SPEEDOMETER]
    self._output_data['orderfile_median_speedup'] = (
        statistics.median(no_orderfile_samples) /
        statistics.median(with_orderfile_samples))

    def RelativeStdev(samples):
      return statistics.stdev(samples) / statistics.median(samples)

    self._output_data['orderfile_benchmark_stdev_relative'] = RelativeStdev(
        with_orderfile_samples)
    self._output_data['no_orderfile_benchmark_stdev_relative'] = RelativeStdev(
        no_orderfile_samples)

  def Generate(self):
    """Generates and maybe upload an order."""
    if self._options.clobber:
      assert self._options.buildbot, '--clobber is intended for the buildbot.'
      # This is useful on the bot when we need to start from scratch to rebuild.
      if _OUT_PATH.exists():
        logging.info('Clobbering %s...', _OUT_PATH)
        shutil.rmtree(_OUT_PATH, ignore_errors=True)
        # The bot assumes that `out/Release` is always available.
        out_release_path = _OUT_PATH / 'Release'
        logging.info('mkdir %s', out_release_path)
        out_release_path.mkdir(parents=True)

    if self._options.profile:
      self._compiler = ClankCompiler(self._instrumented_out_dir,
                                     self._step_recorder, self._options,
                                     self._GetPathToOrderfile(),
                                     self._native_library_build_variant)
      if not self._options.pregenerated_profiles:
        # If there are pregenerated profiles, the instrumented build should
        # not be changed to avoid invalidating the pregenerated profile
        # offsets.
        if self._options.profile_webview_startup:
          self._compiler.CompileWebViewApk(instrumented=True)
        else:
          self._compiler.CompileChromeApk(instrumented=True)
      self._GenerateAndProcessProfile()
      self._MaybeArchiveOrderfile(self._GetUnpatchedOrderfileFilename())

    if self._options.profile:
      self._RemoveBlanks(self._GetUnpatchedOrderfileFilename(),
                         self._GetPathToOrderfile())
    self._compiler = ClankCompiler(self._uninstrumented_out_dir,
                                   self._step_recorder, self._options,
                                   self._GetPathToOrderfile(),
                                   self._native_library_build_variant)
    self._AddDummyFunctions()
    self._compiler.CompileLibchrome(instrumented=False, force_relink=False)
    if self._VerifySymbolOrder():
      self._MaybeArchiveOrderfile(self._GetPathToOrderfile(),
                                  use_new_cloud=True)
    else:
      self._SaveForDebugging(self._GetPathToOrderfile())

    if self._options.benchmark:
      self._SaveBenchmarkResultsToOutput(
          self.RunBenchmark(self._uninstrumented_out_dir),
          self.RunBenchmark(self._no_orderfile_out_dir, no_orderfile=True))

    if self._options.buildbot:
      self._orderfile_updater._GitStash()
    self._step_recorder.EndStep()
    return not self._step_recorder.ErrorRecorded()

  def GetReportingData(self):
    """Get a dictionary of reporting data (timings, output hashes)"""
    self._output_data['timings'] = self._step_recorder.timings
    return self._output_data

  def CommitStashedOrderfileHashes(self):
    """Commit any orderfile hash files in the current checkout.

    Only possible if running on the buildbot.

    Returns: true on success.
    """
    if not self._options.buildbot:
      logging.error('Trying to commit when not running on the buildbot')
      return False
    paths = [
        filename + '.sha1'
        for filename in (self._GetUnpatchedOrderfileFilename(),
                         self._GetPathToOrderfile())
    ]
    # DEPS is updated as well in the new cloud flow.
    paths.append(str(self._clank_dir / 'DEPS'))
    self._orderfile_updater._CommitStashedFiles(paths)
    return True


def CreateArgumentParser():
  """Creates and returns the argument parser."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--no-benchmark', action='store_false', dest='benchmark',
                      default=True, help='Disables running benchmarks.')
  parser.add_argument(
      '--buildbot', action='store_true',
      help='If true, the script expects to be run on a buildbot')
  parser.add_argument(
      '--device', default=None, type=str,
      help='Device serial number on which to run profiling.')
  parser.add_argument(
      '--verify', action='store_true',
      help='If true, the script only verifies the current orderfile')
  parser.add_argument('--target-arch',
                      action='store',
                      dest='arch',
                      default='arm',
                      choices=list(_ARCH_GN_ARGS.keys()),
                      help='The target architecture for which to build.')
  parser.add_argument('--output-json', action='store', dest='json_file',
                      help='Location to save stats in json format')
  parser.add_argument(
      '--skip-profile',
      action='store_false',
      dest='profile',
      default=True,
      help='Don\'t generate a profile on the device. Only patch from the '
      'existing profile.')
  parser.add_argument('--use-remoteexec',
                      action='store_true',
                      help='Enable remoteexec. see //build/toolchain/rbe.gni.',
                      default=False)
  parser.add_argument('--ninja-path',
                      help='Path to the ninja binary. If given, use this'
                      'instead of autoninja.')
  parser.add_argument('--ninja-j',
                      help='-j value passed to ninja.'
                      'pass -j to ninja. no need to set this when '
                      '--ninja-path is not specified.')
  parser.add_argument('--adb-path', help='Path to the adb binary.')

  parser.add_argument('--public',
                      action='store_true',
                      help='Build non-internal APK and change the orderfile '
                      'location. Required if your checkout is non-internal.',
                      default=False)
  parser.add_argument(
      '--native-lib-variant',
      choices=['monochrome', 'trichrome'],
      default='monochrome',
      help=(
          'The shared library build variant for chrome on android. See '
          'https://chromium.googlesource.com/chromium/src/+/HEAD/docs/android_native_libraries.md '  # pylint: disable=line-too-long
          'for more details.'))
  parser.add_argument('--profile-webview-startup',
                      action='store_true',
                      default=False,
                      help='Use the webview startup benchmark profiles to '
                      'generate the orderfile.')
  parser.add_argument('--pregenerated-profiles', default=None, type=str,
                      help=('Pregenerated profiles to use instead of running '
                            'profile step. Cannot be used with '
                            '--skip-profiles.'))
  parser.add_argument('--profile-save-dir', default=None, type=str,
                      help=('Directory to save any profiles created. These can '
                            'be used with --pregenerated-profiles.  Cannot be '
                            'used with --skip-profiles.'))
  parser.add_argument('--upload-ready-orderfiles',
                      action='store_true',
                      help=('Skip orderfile generation and manually upload '
                            'the orderfile from its normal location in '
                            'the tree to the cloud storage. '
                            'DANGEROUS! USE WITH CARE!'))
  parser.add_argument('--streamline-for-debugging',
                      action='store_true',
                      help=('Streamline where possible the run for faster '
                            'iteration while debugging. The orderfile '
                            'generated will be valid and nontrivial, but '
                            'may not be based on a representative profile '
                            'or other such considerations. Use with caution.'))
  parser.add_argument('--commit-hashes', action='store_true',
                      help=('Commit any orderfile hash files in the current '
                            'checkout; performs no other action'))
  parser.add_argument('--use-common-out-dir-for-instrumented',
                      action='store_true',
                      help='Use out/Release for the instrumented out dir so '
                      'that the stack tool works on the bot.')
  parser.add_argument('--clobber',
                      action='store_true',
                      default=False,
                      help='Set this to clear the entire out/ directory prior '
                      'to running any builds. This helps to clear the build '
                      'cache and restart with empty build dirs.')
  parser.add_argument('--use-siso',
                      action='store_true',
                      default=False,
                      help='Set this to turn on using siso.')
  parser.add_argument('-v',
                      '--verbose',
                      dest='verbosity',
                      action='count',
                      default=0,
                      help='>=1 to print debug logging, this will also be '
                      'passed to run_benchmark calls.')
  return parser


def CreateOrderfile(options, orderfile_updater_class=None):
  """Creates an orderfile.

  Args:
    options: As returned from optparse.OptionParser.parse_args()
    orderfile_updater_class: (OrderfileUpdater) subclass of OrderfileUpdater.

  Returns:
    True iff success.
  """
  if options.verbosity >= 1:
    level = logging.DEBUG
  else:
    level = logging.INFO
  logging.basicConfig(level=level)
  devil_chromium.Initialize(adb_path=options.adb_path)

  generator = OrderfileGenerator(options, orderfile_updater_class)
  try:
    if options.verify:
      generator._VerifySymbolOrder()
    elif options.commit_hashes:
      return generator.CommitStashedOrderfileHashes()
    elif options.upload_ready_orderfiles:
      return generator.UploadReadyOrderfiles()
    else:
      return generator.Generate()
  except Exception:
    logging.exception('Generator failure')
  finally:
    json_output = json.dumps(generator.GetReportingData(),
                             indent=2) + '\n'
    if options.json_file:
      with open(options.json_file, 'w') as f:
        f.write(json_output)
    logging.info('\n%s\n', json_output)
  return False


def main():
  parser = CreateArgumentParser()
  options = parser.parse_args()
  return 0 if CreateOrderfile(options) else 1


if __name__ == '__main__':
  sys.exit(main())
