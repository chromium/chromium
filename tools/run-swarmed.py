#!/usr/bin/env python3

# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs a gtest-based test on Swarming, optionally many times, collecting the
output of the runs into a directory. Useful for flake checking, and faster than
using trybots by avoiding repeated bot_update, compile, archive, etc. and
allowing greater parallelism.

To use, run in a new shell (it blocks until all Swarming jobs complete):

  tools/run-swarmed.py out/rel base_unittests

The logs of the runs will be stored in results/ (or specify a results directory
with --results=some_dir). You can then do something like `grep -L SUCCESS
results/*` to find the tests that failed or otherwise process the log files.

See //docs/workflow/debugging-with-swarming.md for more details.
"""



import argparse
import hashlib
import json
import multiprocessing.dummy
import os
import shutil
import subprocess
import sys
import traceback

CHROMIUM_ROOT = os.path.join(os.path.dirname(__file__), os.pardir)
BUILD_DIR = os.path.join(CHROMIUM_ROOT, 'build')

if BUILD_DIR not in sys.path:
  sys.path.insert(0, BUILD_DIR)
import gn_helpers

INTERNAL_ERROR_EXIT_CODE = -1000

DEFAULT_ANDROID_DEVICE_TYPE = "walleye"


def _Spawn(args):
  """Triggers a swarming job. The arguments passed are:
  - The index of the job;
  - The command line arguments object;
  - The digest of test files.

  The return value is passed to a collect-style map() and consists of:
  - The index of the job;
  - The json file created by triggering and used to collect results;
  - The command line arguments object.
  """
  try:
    return _DoSpawn(args)
  except Exception as e:
    traceback.print_exc()
    return None


def _DoSpawn(args):
  index, args, cas_digest, swarming_command = args
  runner_args = []
  json_file = os.path.join(args.results, '%d.json' % index)
  trigger_args = [
      'tools/luci-go/swarming',
      'trigger',
      '-S',
      'https://chromium-swarm.appspot.com',
      '-digest',
      cas_digest,
      '-dump-json',
      json_file,
      '-tag=purpose:user-debug-run-swarmed',
  ]
  if args.target_os == 'fuchsia':
    trigger_args += [
        '-d',
        'kvm=1',
    ]
    if args.gpu is None:
      trigger_args += [
          '-d',
          'gpu=none',
      ]
  elif args.target_os == 'android':
    if args.arch == 'x86':
      # No x86 Android devices are available in swarming. So assume we want to
      # run on emulators when building for x86 on Android.
      args.swarming_os = 'Linux'
      args.pool = 'chromium.tests.avd'
      # generic_android28 == Android P emulator. See //tools/android/avd/proto/
      # for other options.
      runner_args.append(
          '--avd-config=../../tools/android/avd/proto/generic_android28.textpb')
    elif args.device_type is None and args.device_os is None:
      # The aliases for device type are stored here:
      # luci/appengine/swarming/ui2/modules/alias.js
      # for example 'blueline' = 'Pixel 3'
      trigger_args += ['-d', 'device_type=' + DEFAULT_ANDROID_DEVICE_TYPE]
  elif args.target_os == 'ios':
    print('WARNING: iOS support is quite limited.\n'
          '1) --gtest_filter does not work with unit tests.\n' +
          '2) Wildcards do not work with EG tests (--gtest_filter=Foo*).\n' +
          '3) Some arguments are hardcoded (e.g. xcode version) and will ' +
          'break over time. \n')

    runner_args.append('--xcode-build-version=14c18')
    runner_args.append('--xctest')
    runner_args.append('--xcode-parallelization')
    runner_args.append('--out-dir=./test-data')
    runner_args.extend(['--platform', 'iPhone 13'])
    runner_args.append('--version=15.5')
    trigger_args.extend([
        '-service-account',
        'chromium-tester@chops-service-accounts.iam.gserviceaccount.com'
    ])
    trigger_args.extend(['-named-cache', 'runtime_ios_15_5=Runtime-ios-15.5'])
    trigger_args.extend(['-named-cache', 'xcode_ios_14c18=Xcode.app'])
    trigger_args.extend([
        '-cipd-package', '.:infra/tools/mac_toolchain/${platform}=' +
        'git_revision:59ddedfe3849abf560cbe0b41bb8e431041cd2bb'
    ])

  if args.arch != 'detect':
    trigger_args += [
        '-d',
        'cpu=' + args.arch,
    ]

  if args.device_type:
    trigger_args += ['-d', 'device_type=' + args.device_type]

  if args.device_os:
    trigger_args += ['-d', 'device_os=' + args.device_os]

  if args.gpu:
    trigger_args += ['-d', 'gpu=' + args.gpu]

  if not args.no_test_flags:
    # These flags are recognized by our test runners, but do not work
    # when running custom scripts.
    runner_args += [
        '--test-launcher-summary-output=${ISOLATED_OUTDIR}/output.json'
    ]
    if 'junit' not in args.target_name:
      runner_args += ['--system-log-file=${ISOLATED_OUTDIR}/system_log']
  if args.gtest_filter:
    runner_args.append('--gtest_filter=' + args.gtest_filter)
  if args.gtest_repeat:
    runner_args.append('--gtest_repeat=' + args.gtest_repeat)
  if args.test_launcher_shard_index and args.test_launcher_total_shards:
    runner_args.append('--test-launcher-shard-index=' +
                       args.test_launcher_shard_index)
    runner_args.append('--test-launcher-total-shards=' +
                       args.test_launcher_total_shards)
  elif args.target_os == 'fuchsia':
    filter_file = \
        'testing/buildbot/filters/fuchsia.' + args.target_name + '.filter'
    if os.path.isfile(filter_file):
      runner_args.append('--test-launcher-filter-file=../../' + filter_file)

  runner_args.extend(args.runner_args)

  trigger_args.extend(['-d', 'os=' + args.swarming_os])
  trigger_args.extend(['-d', 'pool=' + args.pool])
  trigger_args.extend(['--relative-cwd', args.out_dir, '--'])
  trigger_args.extend(swarming_command)
  trigger_args.extend(runner_args)

  with open(os.devnull, 'w') as nul:
    subprocess.check_call(trigger_args, stdout=nul)
  return (index, json_file, args)


def _Collect(spawn_result):
  if spawn_result is None:
    return 1

  index, json_file, args = spawn_result
  with open(json_file) as f:
    task_json = json.load(f)
  task_ids = [task['task_id'] for task in task_json['tasks']]

  for t in task_ids:
    print('Task {}: https://chromium-swarm.appspot.com/task?id={}'.format(
        index, t))
  p = subprocess.Popen(
      [
          'tools/luci-go/swarming',
          'collect',
          '-S',
          'https://chromium-swarm.appspot.com',
          '--task-output-stdout=console',
      ] + task_ids,
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT)
  stdout = p.communicate()[0]
  if p.returncode != 0 and len(stdout) < 2**10 and 'Internal error!' in stdout:
    exit_code = INTERNAL_ERROR_EXIT_CODE
    file_suffix = '.INTERNAL_ERROR'
  else:
    exit_code = p.returncode
    file_suffix = '' if exit_code == 0 else '.FAILED'
  filename = '%d%s.stdout.txt' % (index, file_suffix)
  with open(os.path.join(args.results, filename), 'wb') as f:
    f.write(stdout)
  return exit_code


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--swarming-os', help='OS specifier for Swarming.')
  parser.add_argument('--target-os', default='detect', help='gn target_os')
  parser.add_argument('--arch', '-a', default='detect',
                      help='CPU architecture of the test binary.')
  parser.add_argument('--build',
                      dest='build',
                      action='store_true',
                      help='Build before isolating.')
  parser.add_argument('--no-build',
                      dest='build',
                      action='store_false',
                      help='Do not build, just isolate (default).')
  parser.add_argument('--isolate-map-file', '-i',
                      help='path to isolate map file if not using default')
  parser.add_argument('--copies', '-n', type=int, default=1,
                      help='Number of copies to spawn.')
  parser.add_argument(
      '--device-os', help='Run tests on the given version of Android.')
  parser.add_argument('--device-type',
                      help='device_type specifier for Swarming'
                      ' from https://chromium-swarm.appspot.com/botlist .')
  parser.add_argument('--gpu',
                      help='gpu specifier for Swarming'
                      ' from https://chromium-swarm.appspot.com/botlist .')
  parser.add_argument('--pool',
                      default='chromium.tests',
                      help='Use the given swarming pool.')
  parser.add_argument('--results', '-r', default='results',
                      help='Directory in which to store results.')
  parser.add_argument(
      '--gtest_filter',
      help='Deprecated. Pass as test runner arg instead, like \'-- '
      '--gtest_filter="*#testFoo"\'')
  parser.add_argument(
      '--gtest_repeat',
      help='Deprecated. Pass as test runner arg instead, like \'-- '
      '--gtest_repeat=99\'')
  parser.add_argument(
      '--test-launcher-shard-index',
      help='Shard index to run. Use with --test-launcher-total-shards.')
  parser.add_argument('--test-launcher-total-shards',
                      help='Number of shards to split the test into. Use with'
                      ' --test-launcher-shard-index.')
  parser.add_argument('--no-test-flags', action='store_true',
                      help='Do not add --test-launcher-summary-output and '
                           '--system-log-file flags to the comment.')
  parser.add_argument('out_dir', type=str, help='Build directory.')
  parser.add_argument('target_name', type=str, help='Name of target to run.')
  parser.add_argument(
      'runner_args',
      nargs='*',
      type=str,
      help='Arguments to pass to the test runner, e.g. gtest_filter and '
      'gtest_repeat.')

  args = parser.parse_intermixed_args()

  with open(os.path.join(args.out_dir, 'args.gn')) as f:
    gn_args = gn_helpers.FromGNArgs(f.read())

  if args.target_os == 'detect':
    if 'target_os' in gn_args:
      args.target_os = gn_args['target_os'].strip('"')
    else:
      args.target_os = {
          'darwin': 'mac',
          'linux': 'linux',
          'win32': 'win'
      }[sys.platform]

  if args.swarming_os is None:
    args.swarming_os = {
      'mac': 'Mac',
      'ios': 'Mac-13',
      'win': 'Windows',
      'linux': 'Linux',
      'android': 'Android',
      'fuchsia': 'Linux'
    }[args.target_os]

  if args.target_os == 'win' and args.target_name.endswith('.exe'):
    # The machinery expects not to have a '.exe' suffix.
    args.target_name = os.path.splitext(args.target_name)[0]

  # Determine the CPU architecture of the test binary, if not specified.
  if args.arch == 'detect':
    if args.target_os == 'ios':
      print('iOS must specify --arch. Probably arm64 or x86-64.')
      return 1
    if args.target_os not in ('android', 'mac', 'win'):
      executable_info = subprocess.check_output(
          ['file', os.path.join(args.out_dir, args.target_name)], text=True)
      if 'ARM aarch64' in executable_info:
        args.arch = 'arm64',
      else:
        args.arch = 'x86-64'
    elif args.target_os == 'android':
      args.arch = gn_args.get('target_cpu', 'detect')

  mb_cmd = [sys.executable, 'tools/mb/mb.py', 'isolate']
  if not args.build:
    mb_cmd.append('--no-build')
  if args.isolate_map_file:
    mb_cmd += ['--isolate-map-file', args.isolate_map_file]
  mb_cmd += ['//' + args.out_dir, args.target_name]
  subprocess.check_call(mb_cmd, shell=os.name == 'nt')

  print('If you get authentication errors, follow:')
  print(
      '  https://chromium.googlesource.com/chromium/src/+/HEAD/docs/workflow/debugging-with-swarming.md#authenticating'
  )

  print('Uploading to isolate server, this can take a while...')
  isolate = os.path.join(args.out_dir, args.target_name + '.isolate')
  archive_json = os.path.join(args.out_dir, args.target_name + '.archive.json')
  subprocess.check_output([
      'tools/luci-go/isolate', 'archive', '-cas-instance', 'chromium-swarm',
      '-isolate', isolate, '-dump-json', archive_json
  ])
  with open(archive_json) as f:
    cas_digest = json.load(f).get(args.target_name)

  mb_cmd = [
      sys.executable, 'tools/mb/mb.py', 'get-swarming-command', '--as-list'
  ]
  if not args.build:
    mb_cmd.append('--no-build')
  if args.isolate_map_file:
    mb_cmd += ['--isolate-map-file', args.isolate_map_file]
  mb_cmd += ['//' + args.out_dir, args.target_name]
  mb_output = subprocess.check_output(mb_cmd, shell=os.name == 'nt')
  swarming_cmd = json.loads(mb_output)

  if os.path.isdir(args.results):
    shutil.rmtree(args.results)
  os.makedirs(args.results)

  try:
    print('Triggering %d tasks...' % args.copies)
    # Use dummy since threadpools give better exception messages
    # than process pools do, and threads work fine for what we're doing.
    pool = multiprocessing.dummy.Pool()
    spawn_args = [(i, args, cas_digest, swarming_cmd)
                  for i in range(args.copies)]
    spawn_results = pool.imap_unordered(_Spawn, spawn_args)

    exit_codes = []
    collect_results = pool.imap_unordered(_Collect, spawn_results)
    for result in collect_results:
      exit_codes.append(result)
      successes = sum(1 for x in exit_codes if x == 0)
      errors = sum(1 for x in exit_codes if x == INTERNAL_ERROR_EXIT_CODE)
      failures = len(exit_codes) - successes - errors
      clear_to_eol = '\033[K'
      print(
          '\r[%d/%d] collected: '
          '%d successes, %d failures, %d bot errors...%s' %
          (len(exit_codes), args.copies, successes, failures, errors,
           clear_to_eol),
          end=' ')
      sys.stdout.flush()

    print()
    print('Results logs collected into', os.path.abspath(args.results) + '.')
  finally:
    pool.close()
    pool.join()
  return 0


if __name__ == '__main__':
  sys.exit(main())
