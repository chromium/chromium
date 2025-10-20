#!/usr/bin/env vpython3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" A utility to generate an orderfile.

The orderfile is used by the linker to order symbols such that they
are placed consecutively. See //docs/orderfile.md.

Example usage:
  tools/cygprofile/generate_orderfile_full.py --use-remoteexec \
    --target-arch=arm64
"""

import argparse
import csv
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
from typing import Dict, List, Union

import android_profile_tool
import check_orderfile
import orderfile_shared

_SRC_PATH = pathlib.Path(__file__).resolve().parents[2]
sys.path.append(str(_SRC_PATH / 'third_party/catapult/devil'))
from devil.android import device_utils
from devil.android.sdk import version_codes

sys.path.append(str(_SRC_PATH / 'build/android'))
import devil_chromium

_OUT_PATH = _SRC_PATH / 'out'
# use depot_tools/gn to find actual binary path for any platforms.
_GN_PATH = _SRC_PATH / 'third_party/depot_tools/gn.py'

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


class StepRecorder:
  """Records steps and timings."""

  def __init__(self):
    self._error_recorded = False

  def FailStep(self, message=None):
    """Marks that a particular step has failed.

    Also prints an optional failure message.

    Args:
      message: An optional explanation as to why the step failed.
    """
    if message:
      logging.error(message)
    self._error_recorded = True

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
      raise Exception('Exception executing command %s' % ' '.join(cmd))
    if capture_output:
      logging.error('Output:\n%s', process.stdout)
    return process


def _GetApkFromTarget(target):
  _camel_case = ''.join(x.capitalize() for x in target.lower().split('_'))
  _camel_case = _camel_case.replace('Webview', 'WebView')
  return _camel_case.replace('Apk', '.apk')


def _GetWebViewTargetAndApk(arch):
  # Always use public targets since the bots only use public targets.
  target = 'system_webview_apk'
  apk = _GetApkFromTarget(target)
  if '64' in arch:
    target = target.replace('_apk', '_64_32_apk')
    apk = apk.replace('.apk', '6432.apk')
  return target, apk


def _GetChromeTargetAndBrowserName(arch):
  # Always use public targets since the bots only use public targets.
  target = 'trichrome_chrome_bundle'
  if arch == 'arm64':
    target = 'trichrome_chrome_64_32_bundle'
  # e.g. trichrome_chrome_bundle -> android-trichrome-chrome-bundle
  return target, 'android-' + target.replace('_', '-')


def _RemoveBlanks(src_file, dest_file):
  """A utility to remove blank lines from a file.

  Args:
    src_file: The name of the file to remove the blanks from.
    dest_file: The name of the file to write the output without blanks.
  """
  assert src_file != dest_file, 'Source and destination need to be distinct'
  with open(src_file) as src, open(dest_file, 'w') as dest:
    dest.writelines(line for line in src if line.strip())


class ClankCompiler:
  """Handles compilation of clank."""

  def __init__(self, out_dir: pathlib.Path, step_recorder: StepRecorder,
               options, orderfile_location):
    self._out_dir = out_dir
    self._step_recorder = step_recorder
    self._options = options
    self._orderfile_location = orderfile_location

    self._ninja_command = ['autoninja']
    # use ninja_path if it is explicitly given.

    self._ninja_command += ['-C']

    # WebView targets
    self._webview_target, webview_apk = _GetWebViewTargetAndApk(
        options.arch)
    self.webview_apk_path = str(out_dir / 'apks' / webview_apk)
    self.webview_installer_path = str(self._out_dir / 'bin' /
                                      self._webview_target)

    # Chrome targets
    self._chrome_target, self.chrome_browser_name = (
        _GetChromeTargetAndBrowserName(options.arch))

    self._libchrome_target = orderfile_shared.GetLibchromeTarget(
        options.arch, options.profile_webview)
    self.lib_chrome_so = orderfile_shared.GetLibchromeSoPath(
        out_dir, options.arch, options.profile_webview)

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
    logging.info('Compile %s' % target)
    gn_args = self._GenerateGnArgs(instrumented)
    self._step_recorder.RunCommand([
        sys.executable,
        str(_GN_PATH), 'gen',
        str(self._out_dir), '--args=' + ' '.join(gn_args)
    ])
    # At times there is a cyclic dependency, so if the initial ninja command
    # fails, we can retry after cleaning the output directory.
    process = self._step_recorder.RunCommand(self._ninja_command +
                                             [str(self._out_dir), target],
                                             raise_on_error=False)
    if process.returncode == 0:
      return
    # The first ninja command failed, try cleaning and re-running.
    self._step_recorder.RunCommand(
        [sys.executable,
         str(_GN_PATH), 'clean',
         str(self._out_dir)])
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


class OrderfileGenerator:
  """A utility for generating a new orderfile for Clank.

  Builds an instrumented binary, profiles a run of the application, and
  generates an updated orderfile.
  """
  _CHECK_ORDERFILE_SCRIPT = _SRC_PATH / 'tools/cygprofile/check_orderfile.py'

  # Previous orderfile_generator debug files would be overwritten.
  _DIRECTORY_FOR_DEBUG_FILES = '/tmp/orderfile_generator_debug_files'

  def __init__(self, options):
    self._options = options
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
    assert _SRC_PATH.is_dir(), 'No src directory found'

  def _PrepareOrderfilePaths(self):
    if self._options.public:
      self._clank_dir = _SRC_PATH
    else:
      self._clank_dir = _SRC_PATH / 'clank'
    self._orderfiles_dir = self._clank_dir / 'orderfiles'
    if self._options.profile_webview:
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

    for device in devices:
      if device.build_version_sdk >= version_codes.Q:
        return device
    raise Exception('No device running Android Q+ found to build trichrome.')


  def _GenerateAndProcessProfile(self):
    """Invokes a script to merge the per-thread traces into one file.

    The produced list of offsets is saved in
    self._GetUnpatchedOrderfileFilename().
    """
    logging.info('Generate Profile Data')
    files = []
    logging.getLogger().setLevel(logging.DEBUG)

    if self._options.profile_save_dir:
      # The directory must not preexist, to ensure purity of data. Check
      # before profiling to save time.
      if os.path.exists(self._options.profile_save_dir):
        raise Exception('Profile save directory must not pre-exist')

    assert self._compiler is not None, (
        'A valid compiler is needed to generate profiles.')
    if self._options.profile_webview:
      apk_or_browser = self._compiler.webview_apk_path
    else:
      apk_or_browser = self._compiler.chrome_browser_name
    files = orderfile_shared.CollectProfiles(
        self._profiler,
        self._options.profile_webview, self._options.arch, apk_or_browser,
        str(self._instrumented_out_dir), self._compiler.webview_installer_path)
    self._MaybeSaveProfile()
    self._ProcessPhasedOrderfile(files)
    if not self._options.save_profile_data:
      self._profiler.Cleanup()
    logging.getLogger().setLevel(logging.INFO)

  def _ProcessPhasedOrderfile(self, files):
    """Process the phased orderfiles produced by system health benchmarks.

    The offsets will be placed in _GetUnpatchedOrderfileFilename().

    Args:
      file: Profile files pulled locally.
    """
    logging.info('Process Phased Orderfile')
    assert self._compiler is not None
    ordered_symbols, symbols_size = orderfile_shared.ProcessProfiles(
        files, self._compiler.lib_chrome_so)
    self._output_data['offsets_kib'] = symbols_size / 1024
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
    # saving it locally.
    logging.info('Add dummy functions')
    orderfile_shared.AddDummyFunctions(self._GetUnpatchedOrderfileFilename(),
                                       self._GetPathToOrderfile())

  def _VerifySymbolOrder(self):
    logging.info('Verify Symbol Order')
    assert self._compiler is not None
    return check_orderfile.ExtractAndVerifySymbolOrder(
        self._compiler.lib_chrome_so, self._GetPathToOrderfile())

  def _WebViewStartupBenchmark(self, apk: str):
    """Runs system_health.webview_startup benchmark.
    Args:
      apk: Path to the apk.
    """
    logging.info('Running system_health.webview_startup')
    try:
      chromium_out_dir = os.path.abspath(
          os.path.join(os.path.dirname(apk), '..'))
      browser = 'android-webview-standalone'
      out_dir = tempfile.mkdtemp()
      android_profile_tool.RunCommand([
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
    logging.info('Running orderfile.memory_mobile')
    try:
      out_dir = tempfile.mkdtemp()
      cmd = [
          'tools/perf/run_benchmark', '--device', self._profiler._device.serial,
          '--browser=exact', '--output-format=csv', '--output-dir', out_dir,
          '--reset-results', '--browser-executable', apk,
          'orderfile.memory_mobile'
      ] + ['-v'] * self._options.verbosity
      android_profile_tool.RunCommand(cmd)

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
          results.setdefault(row['name'], {}).setdefault(row['stories'],
                                                         []).append(row['avg'])

      if not results:
        raise Exception('Could not find relevant results')

      return results

    except Exception as e:
      return 'Error: ' + str(e)

    finally:
      shutil.rmtree(out_dir)

  def _PerformanceBenchmark(self, apk: str) -> Union[List[float], str]:
    """Runs Speedometer2.0 to assess performance.

    Args:
      apk: Path to the apk.

    Returns:
      results: Speedometer2.0 results samples in milliseconds.
    """
    logging.info('Running Speedometer2.0.')
    try:
      out_dir = tempfile.mkdtemp()
      cmd = [
          'tools/perf/run_benchmark', '--device', self._profiler._device.serial,
          '--browser=exact', '--output-format=histograms', '--output-dir',
          out_dir, '--reset-results', '--browser-executable', apk,
          'speedometer2'
      ] + ['-v'] * self._options.verbosity

      android_profile_tool.RunCommand(cmd)
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
                                     self._options, self._GetPathToOrderfile())

      if no_orderfile:
        orderfile_path = self._GetPathToOrderfile()
        backup_orderfile = orderfile_path + '.backup'
        shutil.move(orderfile_path, backup_orderfile)
        open(orderfile_path, 'w').close()

      # Build APK to be installed on the device.
      self._compiler.CompileChromeApk(instrumented=False, force_relink=True)
      benchmark_results[_RESULTS_KEY_SPEEDOMETER] = self._PerformanceBenchmark(
          self._compiler.chrome_browser_name)
      benchmark_results['orderfile.memory_mobile'] = (
          self._NativeCodeMemoryBenchmark(self._compiler.chrome_browser_name))
      if self._options.profile_webview:
        self._compiler.CompileWebViewApk(instrumented=False, force_relink=True)
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
    if self._options.profile:
      self._compiler = ClankCompiler(self._instrumented_out_dir,
                                     self._step_recorder, self._options,
                                     self._GetPathToOrderfile())
      if not self._options.pregenerated_profiles:
        # If there are pregenerated profiles, the instrumented build should
        # not be changed to avoid invalidating the pregenerated profile
        # offsets.
        if self._options.profile_webview:
          self._compiler.CompileWebViewApk(instrumented=True)
        else:
          self._compiler.CompileChromeApk(instrumented=True)
      self._GenerateAndProcessProfile()

    if self._options.profile:
      _RemoveBlanks(self._GetUnpatchedOrderfileFilename(),
                    self._GetPathToOrderfile())
    self._AddDummyFunctions()
    if not self.CompileAndVerify():
      return False

    if self._options.benchmark:
      self._SaveBenchmarkResultsToOutput(
          self.RunBenchmark(self._uninstrumented_out_dir),
          self.RunBenchmark(self._no_orderfile_out_dir, no_orderfile=True))

    return not self._step_recorder.ErrorRecorded()

  def CompileAndVerify(self):
    """Compiles and verifies the orderfile."""
    self._compiler = ClankCompiler(self._uninstrumented_out_dir,
                                   self._step_recorder, self._options,
                                   self._GetPathToOrderfile())
    self._compiler.CompileLibchrome(instrumented=False, force_relink=False)
    return self._VerifySymbolOrder()

  def GetReportingData(self):
    """Get a dictionary of reporting data."""
    return self._output_data


def CreateArgumentParser():
  """Creates and returns the argument parser."""
  parser = argparse.ArgumentParser()
  orderfile_shared.AddCommonArguments(parser)
  parser.add_argument(
      "--no-benchmark",
      action="store_false",
      dest="benchmark",
      default=True,
      help="Disables running benchmarks.",
  )
  parser.add_argument(
      "--device",
      default=None,
      type=str,
      help="Device serial number on which to run profiling.",
  )

  parser.add_argument(
      "--skip-profile",
      action="store_false",
      dest="profile",
      default=True,
      help="Don't generate a profile on the device. Only patch from the "
      "existing profile.",
  )
  parser.add_argument(
      "--use-remoteexec",
      action="store_true",
      help="Enable remoteexec. see //build/toolchain/rbe.gni.",
      default=False,
  )
  parser.add_argument("--adb-path", help="Path to the adb binary.")

  parser.add_argument(
      "--public",
      action="store_true",
      help="Build non-internal APK and change the orderfile location. "
      "Required if your checkout is non-internal.",
      default=False,
  )
  parser.add_argument(
      "--pregenerated-profiles",
      default=None,
      type=str,
      help="Pregenerated profiles to use instead of running the profile step. "
      "Cannot be used with --skip-profiles.",
  )
  parser.add_argument(
      "--profile-save-dir",
      default=None,
      type=str,
      help="Directory to save any profiles created. These can be used with "
      "--pregenerated-profiles. Cannot be used with --skip-profiles.",
  )
  parser.add_argument(
      "--verify",
      action="store_true",
      help="If true, the script avoids generation, only compiles the library "
      "and verifies the current orderfile.",
  )
  return parser


def main():
  parser = CreateArgumentParser()
  options = parser.parse_args()
  if options.verbosity >= 1:
    level = logging.DEBUG
  else:
    level = logging.INFO
  logging.basicConfig(level=level)
  devil_chromium.Initialize(adb_path=options.adb_path)

  generator = OrderfileGenerator(options)
  if options.verify:
    if not generator.CompileAndVerify():
      sys.exit(1)
  else:
    if not generator.Generate():
      sys.exit(1)


if __name__ == '__main__':
  main()
