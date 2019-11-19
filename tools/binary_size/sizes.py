#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to extract size information for chrome.

For a list of command-line options, call this script with '--help'.
"""

from __future__ import print_function

import argparse
import errno
import glob
import json
import platform
import os
import re
import stat
import subprocess
import sys
import tempfile

SRC_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))

# Add Catapult to the path so we can import the chartjson-histogramset
# conversion.
sys.path.append(os.path.join(SRC_DIR, 'third_party', 'catapult', 'tracing'))
from tracing.value import convert_chart_json


class ResultsCollector(object):

  def __init__(self):
    self.results = {}

  def add_result(self, name, identifier, value, units):
    assert name not in self.results
    self.results[name] = {
        'identifier': identifier,
        'value': int(value),
        'units': units
    }

    # Legacy printing, previously used for parsing the text logs.
    print('RESULT %s: %s= %s %s' % (name, identifier, value, units))


def get_size(filename):
  return os.stat(filename)[stat.ST_SIZE]


def get_linux_stripped_size(filename):
  EU_STRIP_NAME = 'eu-strip'
  # Assumes |filename| is in out/Release
  # build/linux/bin/eu-strip'
  src_dir = os.path.dirname(os.path.dirname(os.path.dirname(filename)))
  eu_strip_path = os.path.join(src_dir, 'build', 'linux', 'bin', EU_STRIP_NAME)
  if (platform.architecture()[0] == '64bit'
      or not os.path.exists(eu_strip_path)):
    eu_strip_path = EU_STRIP_NAME

  with tempfile.NamedTemporaryFile() as stripped_file:
    strip_cmd = [eu_strip_path, '-o', stripped_file.name, filename]
    result = 0
    result, _ = run_process(result, strip_cmd)
    if result != 0:
      return (result, 0)
    return (result, get_size(stripped_file.name))


def run_process(result, command):
  p = subprocess.Popen(command, stdout=subprocess.PIPE)
  stdout = p.communicate()[0]
  if p.returncode != 0:
    print('ERROR from command "%s": %d' % (' '.join(command), p.returncode))
    if result == 0:
      result = p.returncode
  return result, stdout


def main_mac(output_directory, results_collector, size_path):
  """Print appropriate size information about built Mac targets.

  Returns the first non-zero exit status of any command it executes,
  or zero on success.
  """
  result = 0
  # Work with either build type.
  base_names = ('Chromium', 'Google Chrome')
  for base_name in base_names:
    app_bundle = base_name + '.app'
    framework_name = base_name + ' Framework'
    framework_bundle = framework_name + '.framework'
    framework_dsym_bundle = framework_name + '.dSYM'

    chromium_app_dir = os.path.join(output_directory, app_bundle)
    chromium_executable = os.path.join(chromium_app_dir, 'Contents', 'MacOS',
                                       base_name)

    chromium_framework_dir = os.path.join(output_directory, framework_bundle)
    chromium_framework_executable = os.path.join(chromium_framework_dir,
                                                 framework_name)

    chromium_framework_dsym_dir = os.path.join(output_directory,
                                               framework_dsym_bundle)
    chromium_framework_dsym = os.path.join(chromium_framework_dsym_dir,
                                           'Contents', 'Resources', 'DWARF',
                                           framework_name)
    if os.path.exists(chromium_executable):
      print_dict = {
          # Remove spaces in the names so any downstream processing is less
          # likely to choke.
          'app_name': re.sub(r'\s', '', base_name),
          'app_bundle': re.sub(r'\s', '', app_bundle),
          'framework_name': re.sub(r'\s', '', framework_name),
          'framework_bundle': re.sub(r'\s', '', framework_bundle),
          'app_size': get_size(chromium_executable),
          'framework_size': get_size(chromium_framework_executable),
          'framework_dsym_name': re.sub(r'\s', '', framework_name) + 'Dsym',
          'framework_dsym_size': get_size(chromium_framework_dsym),
      }

      # Collect the segment info out of the App
      result, stdout = run_process(result, [size_path, chromium_executable])
      print_dict['app_text'], print_dict['app_data'], print_dict['app_objc'] = \
          re.search(r'(\d+)\s+(\d+)\s+(\d+)', stdout).groups()

      # Collect the segment info out of the Framework
      result, stdout = run_process(result,
                                   [size_path, chromium_framework_executable])
      print_dict['framework_text'], print_dict['framework_data'], \
        print_dict['framework_objc'] = \
          re.search(r'(\d+)\s+(\d+)\s+(\d+)', stdout).groups()

      # Collect the whole size of the App bundle on disk (include the framework)
      result, stdout = run_process(result, ['du', '-s', '-k', chromium_app_dir])
      du_s = re.search(r'(\d+)', stdout).group(1)
      print_dict['app_bundle_size'] = (int(du_s) * 1024)

      results_collector.add_result(print_dict['app_name'],
                                   print_dict['app_name'],
                                   print_dict['app_size'], 'bytes')
      results_collector.add_result('%s-__TEXT' % print_dict['app_name'],
                                   '__TEXT', print_dict['app_text'], 'bytes')
      results_collector.add_result('%s-__DATA' % print_dict['app_name'],
                                   '__DATA', print_dict['app_data'], 'bytes')
      results_collector.add_result('%s-__OBJC' % print_dict['app_name'],
                                   '__OBJC', print_dict['app_objc'], 'bytes')
      results_collector.add_result(print_dict['framework_name'],
                                   print_dict['framework_name'],
                                   print_dict['framework_size'], 'bytes')
      results_collector.add_result('%s-__TEXT' % print_dict['framework_name'],
                                   '__TEXT', print_dict['framework_text'],
                                   'bytes')
      results_collector.add_result('%s-__DATA' % print_dict['framework_name'],
                                   '__DATA', print_dict['framework_data'],
                                   'bytes')
      results_collector.add_result('%s-__OBJC' % print_dict['framework_name'],
                                   '__OBJC', print_dict['framework_objc'],
                                   'bytes')
      results_collector.add_result(print_dict['app_bundle'],
                                   print_dict['app_bundle'],
                                   print_dict['app_bundle_size'], 'bytes')
      results_collector.add_result(print_dict['framework_dsym_name'],
                                   print_dict['framework_dsym_name'],
                                   print_dict['framework_dsym_size'], 'bytes')

      # Found a match, don't check the other base_names.
      return result
  # If no base_names matched, fail script.
  return 66


def check_linux_binary(binary_name, output_directory):
  """Collect appropriate size information about the built Linux binary given.

  Returns a tuple (result, sizes).  result is the first non-zero exit
  status of any command it executes, or zero on success.  sizes is a list
  of tuples (name, identifier, totals_identifier, value, units).
  The printed line looks like:
    name: identifier= value units
  When this same data is used for totals across all the binaries, then
  totals_identifier is the identifier to use, or '' to just use identifier.
  """
  binary_file = os.path.join(output_directory, binary_name)

  if not os.path.exists(binary_file):
    # Don't print anything for missing files.
    return 0, []

  result = 0
  sizes = []

  sizes.append((binary_name, binary_name, 'size', get_size(binary_file),
                'bytes'))

  result, stripped_size = get_linux_stripped_size(binary_file)
  sizes.append((binary_name + '-stripped', 'stripped', 'stripped',
                stripped_size, 'bytes'))

  result, stdout = run_process(result, ['size', binary_file])
  text, data, bss = re.search(r'(\d+)\s+(\d+)\s+(\d+)', stdout).groups()
  sizes += [
      (binary_name + '-text', 'text', '', text, 'bytes'),
      (binary_name + '-data', 'data', '', data, 'bytes'),
      (binary_name + '-bss', 'bss', '', bss, 'bytes'),
  ]

  # Determine if the binary has the DT_TEXTREL marker.
  result, stdout = run_process(result, ['readelf', '-Wd', binary_file])
  if re.search(r'\bTEXTREL\b', stdout) is None:
    # Nope, so the count is zero.
    count = 0
  else:
    # There are some, so count them.
    result, stdout = run_process(result, ['eu-findtextrel', binary_file])
    count = stdout.count('\n')
  sizes.append((binary_name + '-textrel', 'textrel', '', count, 'relocs'))

  return result, sizes


def main_linux(output_directory, results_collector, size_path):
  """Print appropriate size information about built Linux targets.

  Returns the first non-zero exit status of any command it executes,
  or zero on success.
  """
  assert size_path is None
  binaries = [
      'chrome',
      'nacl_helper',
      'nacl_helper_bootstrap',
      'libffmpegsumo.so',
      'libgcflashplayer.so',
      'libppGoogleNaClPluginChrome.so',
  ]

  result = 0

  totals = {}

  for binary in binaries:
    this_result, this_sizes = check_linux_binary(binary, output_directory)
    if result == 0:
      result = this_result
    for name, identifier, totals_id, value, units in this_sizes:
      results_collector.add_result(name, identifier, value, units)
      totals_id = totals_id or identifier, units
      totals[totals_id] = totals.get(totals_id, 0) + int(value)

  files = [
      'nacl_irt_x86_64.nexe',
      'resources.pak',
  ]

  for filename in files:
    path = os.path.join(output_directory, filename)
    try:
      size = get_size(path)
    except OSError, e:
      if e.errno == errno.ENOENT:
        continue  # Don't print anything for missing files.
      raise
    results_collector.add_result(filename, filename, size, 'bytes')
    totals['size', 'bytes'] += size

  # TODO(mcgrathr): This should all be refactored so the mac and win flavors
  # also deliver data structures rather than printing, and the logic for
  # the printing and the summing totals is shared across all three flavors.
  for (identifier, units), value in sorted(totals.iteritems()):
    results_collector.add_result('totals-%s' % identifier, identifier, value,
                                 units)

  return result


def check_android_binaries(binaries,
                           output_directory,
                           results_collector,
                           binaries_to_print=None):
  """Common method for printing size information for Android targets.

  Prints size information for each element of binaries in the output
  directory. If binaries_to_print is specified, the name of each binary from
  binaries is replaced with corresponding element of binaries_to_print
  in output. Returns the first non-zero exit status of any command it
  executes, or zero on success.
  """
  result = 0
  if not binaries_to_print:
    binaries_to_print = binaries

  for (binary, binary_to_print) in zip(binaries, binaries_to_print):
    this_result, this_sizes = check_linux_binary(binary, output_directory)
    if result == 0:
      result = this_result
    for name, identifier, _, value, units in this_sizes:
      name = name.replace('/', '_').replace(binary, binary_to_print)
      identifier = identifier.replace(binary, binary_to_print)
      results_collector.add_result(name, identifier, value, units)

  return result


def main_android_cronet(output_directory, results_collector, size_path):
  """Print appropriate size information about Android Cronet targets.

  Returns the first non-zero exit status of any command it executes,
  or zero on success.
  """
  assert size_path is None
  # Use version in binary file name, but not in printed output.
  binaries_with_paths = glob.glob(
      os.path.join(output_directory, 'libcronet.*.so'))
  num_binaries = len(binaries_with_paths)
  assert num_binaries == 1, "Got %d binaries: %s" % (
      num_binaries, ', '.join(binaries_with_paths))
  binaries = [os.path.basename(binaries_with_paths[0])]
  binaries_to_print = ['libcronet.so']

  return check_android_binaries(binaries, output_directory, results_collector,
                                binaries_to_print)


def main_win(output_directory, results_collector, size_path):
  """Print appropriate size information about built Windows targets.

  Returns the first non-zero exit status of any command it executes,
  or zero on success.
  """
  assert size_path is None
  files = [
      'chrome.dll',
      'chrome.dll.pdb',
      'chrome.exe',
      'chrome_child.dll',
      'chrome_child.dll.pdb',
      'chrome_elf.dll',
      'chrome_proxy.exe',
      'chrome_watcher.dll',
      'elevation_service.exe',
      'libEGL.dll',
      'libGLESv2.dll',
      'mini_installer.exe',
      'notification_helper.exe',
      'resources.pak',
      'setup.exe',
      'swiftshader\\libEGL.dll',
      'swiftshader\\libGLESv2.dll',
      'WidevineCdm\\_platform_specific\\win_x64\\widevinecdm.dll',
      'WidevineCdm\\_platform_specific\\win_x64\\widevinecdmadapter.dll',
      'WidevineCdm\\_platform_specific\\win_x86\\widevinecdm.dll',
      'WidevineCdm\\_platform_specific\\win_x86\\widevinecdmadapter.dll',
  ]

  for f in files:
    p = os.path.join(output_directory, f)
    if os.path.isfile(p):
      results_collector.add_result(f, f, get_size(p), 'bytes')

  return 0


def format_for_histograms_conversion(data):
  # We need to do two things to the provided data to make it compatible with the
  # conversion script:
  # 1. Add a top-level "benchmark_name" key.
  # 2. Pull out the "identifier" value to be the story name.
  formatted_data = {}
  for metric, metric_data in data.iteritems():
    story = metric_data['identifier']
    formatted_data[metric] = {story: metric_data.copy()}
    del formatted_data[metric][story]['identifier']
  return {'benchmark_name': 'sizes', 'charts': formatted_data}


def main():
  if sys.platform in ('win32', 'cygwin'):
    default_platform = 'win'
  elif sys.platform.startswith('darwin'):
    default_platform = 'mac'
  elif sys.platform == 'linux2':
    default_platform = 'linux'
  else:
    default_platform = None

  main_map = {
      'android-cronet': main_android_cronet,
      'linux': main_linux,
      'mac': main_mac,
      'win': main_win,
  }
  platforms = sorted(main_map.keys())

  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--output-directory',
      type=os.path.realpath,
      help='Chromium output directory, e.g. /path/to/src/out/Debug')
  parser.add_argument(
      '--platform',
      default=default_platform,
      help='specify platform (%s) [default: %%(default)s]' %
      ', '.join(platforms))
  parser.add_argument('--size-path', default=None, help='Path to size binary')

  # Accepted to conform to the isolated script interface, but ignored.
  parser.add_argument('--isolated-script-test-filter', help=argparse.SUPPRESS)
  parser.add_argument(
      '--isolated-script-test-perf-output', help=argparse.SUPPRESS)

  parser.add_argument(
      '--isolated-script-test-output',
      type=os.path.realpath,
      help='File to which simplified JSON results will be written.')

  args = parser.parse_args()

  real_main = main_map.get(args.platform)
  if not real_main:
    if args.platform is None:
      sys.stderr.write('Unsupported sys.platform %s.\n' % repr(sys.platform))
    else:
      sys.stderr.write('Unknown platform %s.\n' % repr(args.platform))
    msg = 'Use the --platform= option to specify a supported platform:\n'
    sys.stderr.write(msg + '    ' + ' '.join(platforms) + '\n')
    return 2

  isolated_script_output = {
      'valid': False,
      'failures': [],
      'version': 'simplified'
  }
  test_name = 'sizes'

  results_directory = None
  if args.isolated_script_test_output:
    results_directory = os.path.join(
        os.path.dirname(args.isolated_script_test_output), test_name)
    if not os.path.exists(results_directory):
      os.makedirs(results_directory)

  results_collector = ResultsCollector()
  try:
    rc = real_main(args.output_directory, results_collector, args.size_path)
    isolated_script_output = {
        'valid': True,
        'failures': [test_name] if rc else [],
        'version': 'simplified',
    }
  finally:
    if results_directory:
      results_path = os.path.join(results_directory, 'test_results.json')
      with open(results_path, 'w') as output_file:
        json.dump(isolated_script_output, output_file)

      histogram_path = os.path.join(results_directory, 'perf_results.json')
      # We need to add a bit more data to the results and rearrange some things,
      # otherwise the conversion fails due to the provided data being malformed.
      updated_results = format_for_histograms_conversion(
          results_collector.results)
      with open(histogram_path, 'w') as f:
        json.dump(updated_results, f)
      histogram_result = convert_chart_json.ConvertChartJson(histogram_path)
      if histogram_result.returncode != 0:
        sys.stderr.write(
            'chartjson conversion failed: %s\n' % histogram_result.stdout)
        rc = rc or histogram_result.returncode
      else:
        with open(histogram_path, 'w') as f:
          f.write(histogram_result.stdout)

  return rc


if '__main__' == __name__:
  sys.exit(main())
