#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tool to download and run a specific Chromium/Chrome build.

This script uses the downloading and launching capabilities from bisect-builds.py
to download a specific Chromium or Chrome build (snapshot, release, official, cft,
or asan) and execute it with optional command line flags or a custom command template (-c).

Docs: https://www.chromium.org/developers/bisect-builds-py/
"""

import argparse
import os
import subprocess
import sys
import tempfile
import traceback
import urllib.error

# Import bisect-builds module from the same directory.
_TOOLS_DIR = os.path.abspath(os.path.dirname(__file__))
if _TOOLS_DIR not in sys.path:
  sys.path.insert(0, _TOOLS_DIR)

bisect_builds = __import__('bisect-builds')


def _CreateCommandLineParser():
  """Creates a parser with run-build options.

  Returns:
    An instance of argparse.ArgumentParser.
  """
  description = """
Downloads and runs a specific Chromium/Chrome build.

Revision/Version numbers can be:
  a) Release versions: (e.g. 120.0.6099.109) for release builds (-r).
  b) Commit Positions: (e.g. 123456) for snapshot (-s), official (-o), or cft (--cft) builds.
  c) Milestones: (e.g. M120) or versions (e.g. 120.0.6099.109) which will be resolved to commit positions.

Tip: add "-- --no-first-run" to bypass the first run prompts.

Examples:
  # Run a snapshot build for commit position 1217362
  tools/run-build.py 1217362

  # Run a snapshot build passing Chrome arguments
  tools/run-build.py 1217362 -- --no-first-run https://chromium.org

  # Run a release build for version 120.0.6099.109
  tools/run-build.py -r 120.0.6099.109

  # Run a continuous official build
  tools/run-build.py -o 1217362

  # Run Chrome for Testing
  tools/run-build.py --cft 1217362

  # Run with a custom command
  tools/run-build.py -c "%p --headless --dump-dom https://example.com" 1217362
"""

  parser = argparse.ArgumentParser(
      formatter_class=argparse.RawTextHelpFormatter, description=description)

  desktop_archives = sorted(
      set(arch for build in bisect_builds.PATH_CONTEXT
          for arch in bisect_builds.PATH_CONTEXT[build]
          if not arch.startswith(('android', 'ios'))))
  parser.add_argument(
      '-a',
      '--archive',
      choices=desktop_archives,
      metavar='ARCHIVE',
      help='The buildbot platform to run {%s}.' % ','.join(desktop_archives),
  )

  build_type_group = parser.add_mutually_exclusive_group()
  build_type_group.add_argument(
      '-s',
      dest='build_type',
      action='store_const',
      const='snapshot',
      default='snapshot',
      help='Run Chromium snapshot archive (default).',
  )
  build_type_group.add_argument(
      '-r',
      dest='build_type',
      action='store_const',
      const='release',
      help='Run release Chrome build (internal only).',
  )
  build_type_group.add_argument(
      '-o',
      dest='build_type',
      action='store_const',
      const='official',
      help='Run continuous perf official Chrome build (internal only).',
  )
  build_type_group.add_argument(
      '-cft',
      '--cft',
      dest='build_type',
      action='store_const',
      const='cft',
      help='Run Chrome for Testing (publicly accessible) archive.',
  )
  build_type_group.add_argument(
      '--asan',
      dest='build_type',
      action='store_const',
      const='asan',
      help='Run ASAN build.',
  )

  parser.add_argument(
      '--version',
      '--revision',
      dest='version',
      type=str,
      metavar='VERSION',
      help='The version, commit position, or milestone to run.',
  )
  parser.add_argument(
      '-p',
      '--profile',
      '--user-data-dir',
      type=str,
      default='%t/profile',
      help='Profile to use. Defaults to a new, clean profile.',
  )
  parser.add_argument(
      '-c',
      '--command',
      type=str,
      default=r'%p %a',
      help='Command to execute. %%p and %%a refer to Chrome executable and '
      'specified extra arguments respectively. Use %%t for tempdir where '
      'Chrome extracted. Defaults to "%%p %%a". Note that any extra paths specified '
      'should be absolute.',
  )
  parser.add_argument(
      '-v',
      '--verbose',
      action='store_true',
      help='Log more verbose information.',
  )
  parser.add_argument(
      'args',
      nargs='*',
      metavar='args',
      help='Target version/revision (if not specified with --version) followed '
      'by additional options passed to the process.',
  )
  return parser


def ParseCommandLine(args=None):
  """Parses the command line for run-build options."""
  parser = _CreateCommandLineParser()
  opts = parser.parse_args(args)

  if not opts.version:
    if opts.args:
      opts.version = opts.args[0]
      opts.args = opts.args[1:]
    else:
      parser.error('Please specify a version or revision to run.')

  # Set internal properties expected by bisect-builds ArchiveBuild classes
  # and _DetectArchive
  opts.good = opts.version
  opts.bad = opts.version
  opts.use_local_cache = False
  opts.times = 1
  opts.chromedriver = False
  opts.signed = False
  opts.apk = None
  opts.ipa = None
  opts.device_id = None

  if opts.archive is None:
    archive = bisect_builds._DetectArchive(opts)
    if archive:
      if opts.verbose:
        print('The buildbot archive (-a/--archive) detected as:', archive)
      opts.archive = archive
    else:
      parser.error('Error: Missing required parameter: --archive')

  if opts.archive not in bisect_builds.PATH_CONTEXT[opts.build_type]:
    supported_build_types = [
        "%s(%s)" %
        (b, bisect_builds.BuildTypeToCommandLineArgument(b, omit_default=False))
        for b, context in bisect_builds.PATH_CONTEXT.items()
        if opts.archive in context
    ]
    parser.error(
        f'Running on {opts.build_type} is only supported on these '
        'platforms (-a/--archive): '
        f'{{{",".join(bisect_builds.PATH_CONTEXT[opts.build_type].keys())}}}\n'
        f'To run for {opts.archive}, please choose from '
        f'{", ".join(supported_build_types)}')

  if opts.build_type == 'release':
    if not bisect_builds.IsVersionNumber(opts.version):
      parser.error(
          'For release (-r), you must use a Chrome version (e.g. 120.0.6099.109).'
      )

  return opts


def main(args=None):
  opts = ParseCommandLine(args)

  try:
    bisect_builds.SetupEnvironment(opts)
  except bisect_builds.BisectException as e:
    print(e)
    return 1

  try:
    archive_build = bisect_builds.create_archive_build(opts)
  except bisect_builds.BisectException as e:
    print(e)
    return 1

  target_revision = archive_build.good_revision
  download_job = archive_build.get_download_job(target_revision)
  try:
    download = download_job.wait_for()
  except urllib.error.HTTPError as e:
    if e.code == 404:
      print(f'Revision/version {target_revision} was not found at '
            f'{download_job.urls}.')
      return 1
    print(f'HTTP error downloading revision {target_revision}: {e}')
    return 1
  except Exception as e:
    print(f'Error downloading revision {target_revision}: {e}')
    return 1

  exit_status = 0
  with tempfile.TemporaryDirectory(prefix='run_build_tmp') as tempdir:
    if sys.platform == 'win32' and sys.getwindowsversion().build >= 19041:
      icacls_cmd = ['icacls', tempdir, '/grant', '*S-1-15-2-2:(OI)(CI)(RX)']
      proc = subprocess.Popen(icacls_cmd,
                              bufsize=0,
                              stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE)
      proc.communicate()

    if opts.verbose:
      print(f'Running revision {target_revision} in {tempdir}')

    try:
      returncode, stdout, stderr = archive_build.run_revision(
          download, tempdir, opts.args)
      exit_status = returncode
    except SystemExit as e:
      exit_status = e.code if isinstance(e.code, int) else 1
    except Exception:
      traceback.print_exc(file=sys.stderr)
      exit_status = 1
    finally:
      download_job.stop()

  return exit_status


if __name__ == '__main__':
  sys.exit(main())
