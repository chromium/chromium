#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tool for finding the cause of binary size bloat.

See //tools/binary_size/README.md for example usage.

Note: this tool will perform gclient sync/git checkout on your local repo.
"""

from __future__ import print_function

import atexit
import argparse
import collections
from contextlib import contextmanager
import distutils.spawn
import json
import logging
import multiprocessing
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

_COMMIT_COUNT_WARN_THRESHOLD = 15
_ALLOWED_CONSECUTIVE_FAILURES = 2
_SRC_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))
_DEFAULT_ARCHIVE_DIR = os.path.join(_SRC_ROOT, 'out', 'binary-size-results')
_DEFAULT_OUT_DIR = os.path.join(_SRC_ROOT, 'out', 'binary-size-build')
_BINARY_SIZE_DIR = os.path.join(_SRC_ROOT, 'tools', 'binary_size')
_RESOURCE_SIZES_PATH = os.path.join(
    _SRC_ROOT, 'build', 'android', 'resource_sizes.py')
_LLVM_TOOLS_DIR = os.path.join(
    _SRC_ROOT, 'third_party', 'llvm-build', 'Release+Asserts', 'bin')
_CLANG_UPDATE_PATH = os.path.join(_SRC_ROOT, 'tools', 'clang', 'scripts',
                                  'update.py')
_GN_PATH = os.path.join(_SRC_ROOT, 'third_party', 'depot_tools', 'gn')
_NINJA_PATH = os.path.join(_SRC_ROOT, 'third_party', 'depot_tools', 'ninja')


_DiffResult = collections.namedtuple('DiffResult', ['name', 'value', 'units'])


class BaseDiff(object):
  """Base class capturing binary size diffs."""
  def __init__(self, name):
    self.name = name
    self.banner = '\n' + '*' * 30 + name + '*' * 30

  def AppendResults(self, logfiles):
    """Print and write diff results to an open |logfile|."""
    full, short = logfiles
    _WriteToFile(full, self.banner)
    _WriteToFile(short, self.banner)

    for s in self.Summary():
      _WriteToFile(short, s)
    _WriteToFile(short, '')

    for s in self.DetailedResults():
      full.write(s + '\n')

  @property
  def summary_stat(self):
    """Returns a tuple of (name, value, units) for the most important metric."""
    raise NotImplementedError()

  def Summary(self):
    """A short description that summarizes the source of binary size bloat."""
    raise NotImplementedError()

  def DetailedResults(self):
    """An iterable description of the cause of binary size bloat."""
    raise NotImplementedError()

  def ProduceDiff(self, before_dir, after_dir):
    """Prepare a binary size diff with ready to print results."""
    raise NotImplementedError()

  def RunDiff(self, logfiles, before_dir, after_dir):
    logging.info('Creating: %s', self.name)
    self.ProduceDiff(before_dir, after_dir)
    self.AppendResults(logfiles)


class NativeDiff(BaseDiff):
  # E.g.: Section Sizes (Total=1.2 kb (1222 bytes)):
  _RE_SUMMARY_STAT = re.compile(
      r'Section Sizes \(Total=(?P<value>-?[0-9\.]+) ?(?P<units>\w+)')
  _SUMMARY_STAT_NAME = 'Native Library Delta'

  def __init__(self, size_name, supersize_path):
    self._size_name = size_name
    self._supersize_path = supersize_path
    self._diff = []
    super(NativeDiff, self).__init__('Native Diff')

  @property
  def summary_stat(self):
    m = NativeDiff._RE_SUMMARY_STAT.search(self._diff)
    if m:
      return _DiffResult(
          NativeDiff._SUMMARY_STAT_NAME, m.group('value'), m.group('units'))
    raise Exception('Could not extract total from:\n' + self._diff)

  def DetailedResults(self):
    return self._diff.splitlines()

  def Summary(self):
    return self.DetailedResults()[:100]

  def ProduceDiff(self, before_dir, after_dir):
    before_size = os.path.join(before_dir, self._size_name)
    after_size = os.path.join(after_dir, self._size_name)
    cmd = [self._supersize_path, 'diff', before_size, after_size]
    self._diff = _RunCmd(cmd)[0].replace('{', '{{').replace('}', '}}')


class ResourceSizesDiff(BaseDiff):
  # Ordered by output appearance.
  _SUMMARY_SECTIONS = (
      'Specifics', 'InstallSize', 'InstallBreakdown', 'Dex')
  # Sections where it makes sense to sum subsections into a section total.
  _AGGREGATE_SECTIONS = (
      'InstallBreakdown', 'Breakdown', 'MainLibInfo', 'Uncompressed')

  def __init__(self, apk_name, filename='results-chart.json'):
    self._apk_name = apk_name
    self._diff = None  # Set by |ProduceDiff()|
    self._filename = filename
    super(ResourceSizesDiff, self).__init__('Resource Sizes Diff')

  @property
  def summary_stat(self):
    for section_name, results in self._diff.iteritems():
      for subsection_name, value, units in results:
        if 'normalized' in subsection_name:
          full_name = '{} {}'.format(section_name, subsection_name)
          return _DiffResult(full_name, value, units)
    raise Exception('Could not find "normalized" in: ' + repr(self._diff))

  def DetailedResults(self):
    return self._ResultLines()

  def Summary(self):
    footer_lines = [
        '',
        'For an explanation of these metrics, see:',
        ('https://chromium.googlesource.com/chromium/src/+/master/docs/speed/'
         'binary_size/metrics.md#Metrics-for-Android')]
    return self._ResultLines(
        include_sections=ResourceSizesDiff._SUMMARY_SECTIONS) + footer_lines

  def ProduceDiff(self, before_dir, after_dir):
    before = self._LoadResults(before_dir)
    after = self._LoadResults(after_dir)
    self._diff = collections.defaultdict(list)
    for section, section_dict in after.iteritems():
      for subsection, v in section_dict.iteritems():
        # Ignore entries when resource_sizes.py chartjson format has changed.
        if (section not in before or
            subsection not in before[section] or
            v['units'] != before[section][subsection]['units']):
          logging.warning(
              'Found differing dict structures for resource_sizes.py, '
              'skipping %s %s', section, subsection)
        else:
          self._diff[section].append(_DiffResult(
              subsection,
              v['value'] - before[section][subsection]['value'],
              v['units']))

  def _ResultLines(self, include_sections=None):
    """Generates diff lines for the specified sections (defaults to all)."""
    section_lines = collections.defaultdict(list)
    for section_name, section_results in self._diff.iteritems():
      if not include_sections or section_name in include_sections:
        subsection_lines = []
        section_sum = 0
        units = ''
        for name, value, units in section_results:
          # Omit subsections with no changes for summaries.
          if value == 0 and include_sections:
            continue
          section_sum += value
          subsection_lines.append('{:>+14,} {} {}'.format(value, units, name))
        section_header = section_name
        if section_name in ResourceSizesDiff._AGGREGATE_SECTIONS:
          section_header += ' ({:+,} {})'.format(section_sum, units)
        section_header += ':'
        # Omit sections with empty subsections.
        if subsection_lines:
          section_lines[section_name].append(section_header)
          section_lines[section_name].extend(subsection_lines)
    if not section_lines:
      return ['Empty ' + self.name]
    ret = []
    for k in include_sections or sorted(section_lines):
      ret.extend(section_lines[k])
    return ret

  def _LoadResults(self, archive_dir):
    chartjson_file = os.path.join(archive_dir, self._filename)
    with open(chartjson_file) as f:
      chartjson = json.load(f)
    charts = chartjson['charts']
    # Older versions of resource_sizes.py prefixed the apk onto section names.
    ret = {}
    for section, section_dict in charts.iteritems():
      section_no_target = re.sub(r'^.*_', '', section)
      ret[section_no_target] = section_dict
    return ret


class _BuildHelper(object):
  """Helper class for generating and building targets."""
  def __init__(self, args):
    self.clean = args.clean
    self.enable_chrome_android_internal = args.enable_chrome_android_internal
    self.extra_gn_args_str = args.gn_args
    self.apply_patch = args.extra_rev
    self.max_jobs = args.max_jobs
    self.max_load_average = args.max_load_average
    self.output_directory = args.output_directory
    self.target = args.target
    self.target_os = args.target_os
    self.use_goma = args.use_goma
    self._SetDefaults()
    self.is_bundle = 'minimal' in self.target

  @property
  def abs_apk_path(self):
    return os.path.join(self.output_directory, self.apk_path)

  @property
  def abs_mapping_path(self):
    return os.path.join(self.output_directory, self.mapping_path)

  @property
  def apk_name(self):
    # my_great_apk -> MyGreat.apk
    apk_name = ''.join(s.title() for s in self.target.split('_')[:-1]) + '.apk'
    if self.is_bundle:
      # my_great_minimal_apks -> MyGreatMinimal.apk -> MyGreat.minimal.apks
      apk_name = apk_name.replace('Minimal.apk', '.minimal.apks')
    return apk_name.replace('Webview', 'WebView')

  @property
  def apk_path(self):
    return os.path.join('apks', self.apk_name)

  @property
  def mapping_path(self):
    if self.is_bundle:
      return self.apk_path.replace('.minimal.apks', '.aab') + '.mapping'
    else:
      return self.apk_path + '.mapping'

  @property
  def main_lib_path(self):
    # TODO(agrieve): Could maybe extract from .apk or GN?
    if self.IsLinux():
      return 'chrome'
    if 'monochrome' in self.target or 'trichrome' in self.target:
      ret = 'lib.unstripped/libmonochrome.so'
    elif 'webview' in self.target:
      ret = 'lib.unstripped/libwebviewchromium.so'
    else:
      ret = 'lib.unstripped/libchrome.so'
    return ret

  @property
  def abs_main_lib_path(self):
    return os.path.join(self.output_directory, self.main_lib_path)

  @property
  def map_file_path(self):
    return self.main_lib_path + '.map.gz'

  @property
  def size_name(self):
    if self.IsLinux():
      return os.path.basename(self.main_lib_path) + '.size'
    return self.apk_name + '.size'

  def _SetDefaults(self):
    has_goma_dir = os.path.exists(os.path.join(os.path.expanduser('~'), 'goma'))
    self.use_goma = self.use_goma and has_goma_dir
    self.max_load_average = (self.max_load_average or
                             str(multiprocessing.cpu_count()))

    has_internal = os.path.exists(
        os.path.join(os.path.dirname(_SRC_ROOT), 'src-internal'))
    if has_internal:
      self.extra_gn_args_str = (
          'is_chrome_branded=true ' + self.extra_gn_args_str)
    else:
      self.extra_gn_args_str = (
          'ffmpeg_branding="Chrome" proprietary_codecs=true' +
          self.extra_gn_args_str)
    if self.IsLinux():
      self.extra_gn_args_str = (
          'is_cfi=false generate_linker_map=true ' + self.extra_gn_args_str)
    self.extra_gn_args_str = ' ' + self.extra_gn_args_str.strip()

    if not self.max_jobs:
      if self.use_goma:
        self.max_jobs = '10000'
      elif has_internal:
        self.max_jobs = '500'
      else:
        self.max_jobs = '50'

    if not self.target:
      if self.IsLinux():
        self.target = 'chrome'
      elif self.enable_chrome_android_internal:
        self.target = 'monochrome_minimal_apks'
      else:
        self.target = 'monochrome_public_minimal_apks'

  def _GenGnCmd(self):
    gn_args = 'is_official_build=true'
    gn_args += ' android_channel="stable"'
    # Variables often become unused when experimenting with macros to reduce
    # size, so don't fail on warnings.
    gn_args += ' treat_warnings_as_errors=false'
    # Speed things up a bit by skipping lint & errorprone.
    gn_args += ' disable_android_lint=true'
    gn_args += ' use_errorprone_java_compiler=false'
    gn_args += ' use_goma=%s' % str(self.use_goma).lower()
    gn_args += ' target_os="%s"' % self.target_os
    if self.IsAndroid():
      gn_args += (' enable_chrome_android_internal=%s' %
                  str(self.enable_chrome_android_internal).lower())
    gn_args += self.extra_gn_args_str
    return [_GN_PATH, 'gen', self.output_directory, '--args=%s' % gn_args]

  def _GenNinjaCmd(self):
    cmd = [_NINJA_PATH, '-C', self.output_directory]
    cmd += ['-j', self.max_jobs] if self.max_jobs else []
    cmd += ['-l', self.max_load_average] if self.max_load_average else []
    cmd += [self.target]
    return cmd

  def Run(self):
    """Run GN gen/ninja build and return the process returncode."""
    logging.info('Building %s within %s (this might take a while).',
                 self.target, os.path.relpath(self.output_directory))
    if self.clean:
      _RunCmd([_GN_PATH, 'clean', self.output_directory])
    retcode = _RunCmd(
        self._GenGnCmd(), verbose=True, exit_on_failure=False)[1]
    if retcode:
      return retcode
    return _RunCmd(
        self._GenNinjaCmd(), verbose=True, exit_on_failure=False)[1]

  def IsAndroid(self):
    return self.target_os == 'android'

  def IsLinux(self):
    return self.target_os == 'linux'


class _BuildArchive(object):
  """Class for managing a directory with build results and build metadata."""
  def __init__(self, rev, base_archive_dir, build, subrepo, slow_options,
               save_unstripped):
    self.build = build
    self.dir = os.path.join(base_archive_dir, rev)
    metadata_path = os.path.join(self.dir, 'metadata.txt')
    self.rev = rev
    self.metadata = _Metadata([self], build, metadata_path, subrepo)
    self._slow_options = slow_options
    self._save_unstripped = save_unstripped

  def ArchiveBuildResults(self, supersize_path, tool_prefix=None):
    """Save build artifacts necessary for diffing."""
    logging.info('Saving build results to: %s', self.dir)
    _EnsureDirsExist(self.dir)
    if self.build.IsAndroid():
      self._ArchiveFile(self.build.abs_apk_path)
      self._ArchiveFile(self.build.abs_mapping_path)
      self._ArchiveResourceSizes()
    self._ArchiveSizeFile(supersize_path, tool_prefix)
    if self._save_unstripped:
      self._ArchiveFile(self.build.abs_main_lib_path)
    self.metadata.Write()
    assert self.Exists()

  def Exists(self):
    ret = self.metadata.Exists() and os.path.exists(self.archived_size_path)
    if self._save_unstripped:
      ret = ret and os.path.exists(self.archived_unstripped_path)
    return ret

  @property
  def archived_unstripped_path(self):
    return os.path.join(self.dir, os.path.basename(self.build.main_lib_path))

  @property
  def archived_size_path(self):
    return os.path.join(self.dir, self.build.size_name)

  def _ArchiveResourceSizes(self):
    cmd = [
        _RESOURCE_SIZES_PATH, self.build.abs_apk_path, '--output-dir', self.dir,
        '--chartjson', '--chromium-output-dir', self.build.output_directory
    ]
    if self._slow_options:
      cmd += ['--estimate-patch-size', '--dump-static-initializers']
    _RunCmd(cmd)

  def _ArchiveFile(self, filename):
    if not os.path.exists(filename):
      _Die('missing expected file: %s', filename)
    shutil.copy(filename, self.dir)

  def _ArchiveSizeFile(self, supersize_path, tool_prefix):
    existing_size_file = self.build.abs_apk_path + '.size'
    if os.path.exists(existing_size_file):
      logging.info('Found existing .size file')
      shutil.copy(existing_size_file, self.archived_size_path)
    else:
      supersize_cmd = [
          supersize_path, 'archive', self.archived_size_path, '--elf-file',
          self.build.abs_main_lib_path, '--output-directory',
          self.build.output_directory
      ]
      if tool_prefix:
        supersize_cmd += ['--tool-prefix', tool_prefix]
      if self.build.IsAndroid():
        supersize_cmd += ['-f', self.build.abs_apk_path]
      logging.info('Creating .size file')
      _RunCmd(supersize_cmd)


class _DiffArchiveManager(object):
  """Class for maintaining BuildArchives and their related diff artifacts."""
  def __init__(self, revs, archive_dir, diffs, build, subrepo, slow_options,
               save_unstripped):
    self.archive_dir = archive_dir
    self.build = build
    self.build_archives = [
        _BuildArchive(rev, archive_dir, build, subrepo, slow_options,
                      save_unstripped)
        for rev in revs
    ]
    self.diffs = diffs
    self.subrepo = subrepo
    self._summary_stats = []

  def MaybeDiff(self, before_id, after_id):
    """Perform diffs given two build archives."""
    before = self.build_archives[before_id]
    after = self.build_archives[after_id]
    diff_path, short_diff_path = self._DiffFilePaths(before, after)
    if not self._CanDiff(before, after):
      logging.info(
          'Skipping diff for %s due to missing build archives.', diff_path)
      return

    metadata_path = self._DiffMetadataPath(before, after)
    metadata = _Metadata(
        [before, after], self.build, metadata_path, self.subrepo)
    if metadata.Exists():
      logging.info(
          'Skipping diff for %s and %s. Matching diff already exists: %s',
          before.rev, after.rev, diff_path)
    else:
      with open(diff_path, 'w') as diff_file, \
           open(short_diff_path, 'w') as summary_file:
        for d in self.diffs:
          d.RunDiff((diff_file, summary_file), before.dir, after.dir)
      metadata.Write()
      self._AddDiffSummaryStat(before, after)
    if os.path.exists(short_diff_path):
      _PrintFile(short_diff_path)
    logging.info('See detailed diff results here: %s',
                 os.path.relpath(diff_path))

  def GenerateHtmlReport(self, before_id, after_id):
    """Generate HTML report given two build archives."""
    before = self.build_archives[before_id]
    after = self.build_archives[after_id]
    diff_path = self._DiffDir(before, after)
    if not self._CanDiff(before, after):
      logging.info(
          'Skipping HTML report for %s due to missing build archives.',
          diff_path)
      return

    supersize_path = os.path.join(_BINARY_SIZE_DIR, 'supersize')

    report_path = os.path.join(diff_path, 'diff.ndjson')

    supersize_cmd = [supersize_path, 'html_report', '--diff-with',
      before.archived_size_path,
      after.archived_size_path,
      report_path]

    logging.info('Creating HTML report')

    _RunCmd(supersize_cmd)

    logging.info('View using a local server via: %s start_server %s',
      os.path.relpath(supersize_path),
      os.path.relpath(report_path))

  def Summarize(self):
    path = os.path.join(self.archive_dir, 'last_diff_summary.txt')
    if self._summary_stats:
      with open(path, 'w') as f:
        stats = sorted(
            self._summary_stats, key=lambda x: x[0].value, reverse=True)
        _WriteToFile(f, '\nDiff Summary')
        for s, before, after in stats:
          _WriteToFile(f, '{:>+10} {} {} for range: {}..{}',
                               s.value, s.units, s.name, before, after)

    # Print cached file if all builds were cached.
    num_archives = len(self.build_archives)
    if os.path.exists(path) and num_archives > 1:
      _PrintFile(path)
    if num_archives <= 2:
      if not all(a.Exists() for a in self.build_archives):
        return
      supersize_path = os.path.join(_BINARY_SIZE_DIR, 'supersize')
      size2 = ''
      if num_archives == 2:
        size2 = os.path.relpath(self.build_archives[-1].archived_size_path)
      logging.info('Enter supersize console via: %s console %s %s',
          os.path.relpath(supersize_path),
          os.path.relpath(self.build_archives[0].archived_size_path), size2)


  def _AddDiffSummaryStat(self, before, after):
    stat = None
    if self.build.IsAndroid():
      summary_diff_type = ResourceSizesDiff
    else:
      summary_diff_type = NativeDiff
    for d in self.diffs:
      if isinstance(d, summary_diff_type):
        stat = d.summary_stat
    if stat:
      self._summary_stats.append((stat, before.rev, after.rev))

  def _CanDiff(self, before, after):
    return before.Exists() and after.Exists()

  def _DiffFilePaths(self, before, after):
    ret = os.path.join(self._DiffDir(before, after), 'diff_results')
    return ret + '.txt', ret + '.short.txt'

  def _DiffMetadataPath(self, before, after):
    return os.path.join(self._DiffDir(before, after), 'metadata.txt')

  def _DiffDir(self, before, after):
    archive_range = '%s..%s' % (before.rev, after.rev)
    diff_path = os.path.join(self.archive_dir, 'diffs', archive_range)
    _EnsureDirsExist(diff_path)
    return diff_path


class _Metadata(object):

  def __init__(self, archives, build, path, subrepo):
    self.data = {
      'revs': [a.rev for a in archives],
      'apply_patch': build.apply_patch,
      'archive_dirs': [a.dir for a in archives],
      'target': build.target,
      'target_os': build.target_os,
      'subrepo': subrepo,
      'path': path,
      'gn_args': {
        'extra_gn_args_str': build.extra_gn_args_str,
        'enable_chrome_android_internal': build.enable_chrome_android_internal,
      }
    }

  def Exists(self):
    path = self.data['path']
    if os.path.exists(path):
      with open(path, 'r') as f:
        return self.data == json.load(f)
    return False

  def Write(self):
    with open(self.data['path'], 'w') as f:
      json.dump(self.data, f)


def _EnsureDirsExist(path):
  if not os.path.exists(path):
    os.makedirs(path)


def _RunCmd(cmd, verbose=False, exit_on_failure=True):
  """Convenience function for running commands.

  Args:
    cmd: the command to run.
    verbose: if this is True, then the stdout and stderr of the process will be
        printed. If it's false, the stdout will be returned.
    exit_on_failure: die if an error occurs when this is True.

  Returns:
    Tuple of (process stdout, process returncode).
  """
  assert not (verbose and exit_on_failure)
  cmd_str = ' '.join(c for c in cmd)
  logging.debug('Running: %s', cmd_str)
  proc_stdout = proc_stderr = subprocess.PIPE
  if verbose:
    proc_stdout, proc_stderr = sys.stdout, subprocess.STDOUT

  proc = subprocess.Popen(cmd, stdout=proc_stdout, stderr=proc_stderr)
  stdout, stderr = proc.communicate()

  if proc.returncode and exit_on_failure:
    _Die('command failed: %s\nstderr:\n%s', cmd_str, stderr)

  stdout = stdout.strip() if stdout else ''
  return stdout, proc.returncode


def _GitCmd(args, subrepo):
  return _RunCmd(['git', '-C', subrepo] + args)[0]


def _GclientSyncCmd(rev, subrepo):
  cwd = os.getcwd()
  os.chdir(subrepo)
  _, retcode = _RunCmd(['gclient', 'sync', '-r', 'src@' + rev],
                       verbose=True, exit_on_failure=False)
  os.chdir(cwd)
  return retcode


def _SyncAndBuild(archive, build, subrepo, no_gclient, extra_rev):
  """Sync, build and return non 0 if any commands failed."""
  # Simply do a checkout if subrepo is used.
  if _CurrentGitHash(subrepo) == archive.rev:
    if subrepo != _SRC_ROOT:
      logging.info('Skipping git checkout since already at desired rev')
    else:
      logging.info('Skipping gclient sync since already at desired rev')
  elif subrepo != _SRC_ROOT or no_gclient:
    _GitCmd(['checkout',  archive.rev], subrepo)
  else:
    # Move to a detached state since gclient sync doesn't work with local
    # commits on a branch.
    _GitCmd(['checkout', '--detach'], subrepo)
    logging.info('Syncing to %s', archive.rev)
    ret = _GclientSyncCmd(archive.rev, subrepo)
    if ret:
      return ret
  with _ApplyPatch(extra_rev, subrepo):
    return build.Run()


@contextmanager
def _ApplyPatch(rev, subrepo):
  if not rev:
    yield
  else:
    restore_func = _GenRestoreFunc(subrepo)
    try:
      _GitCmd(['cherry-pick', rev, '--strategy-option', 'theirs'], subrepo)
      yield
    finally:
      restore_func()


def _GenerateRevList(rev, reference_rev, all_in_range, subrepo, step):
  """Normalize and optionally generate a list of commits in the given range.

  Returns:
    A list of revisions ordered from oldest to newest.
  """
  rev_seq = '%s^..%s' % (reference_rev, rev)
  stdout = _GitCmd(['rev-list', rev_seq], subrepo)
  all_revs = stdout.splitlines()[::-1]
  if all_in_range or len(all_revs) < 2 or step:
    revs = all_revs
    if step:
      revs = revs[::step]
  else:
    revs = [all_revs[0], all_revs[-1]]
  num_revs = len(revs)
  if num_revs >= _COMMIT_COUNT_WARN_THRESHOLD:
    _VerifyUserAccepts(
        'You\'ve provided a commit range that contains %d commits.' % num_revs)
  logging.info('Processing %d commits', num_revs)
  return revs


def _ValidateRevs(rev, reference_rev, subrepo, extra_rev):
  def git_fatal(args, message):
    devnull = open(os.devnull, 'wb')
    retcode = subprocess.call(
        ['git', '-C', subrepo] + args, stdout=devnull, stderr=subprocess.STDOUT)
    if retcode:
      _Die(message)

  no_obj_message = ('%s either doesn\'t exist or your local repo is out of '
                    'date, try "git fetch origin master"')
  git_fatal(['cat-file', '-e', rev], no_obj_message % rev)
  git_fatal(['cat-file', '-e', reference_rev], no_obj_message % reference_rev)
  if extra_rev:
    git_fatal(['cat-file', '-e', extra_rev], no_obj_message % extra_rev)
  git_fatal(['merge-base', '--is-ancestor', reference_rev, rev],
            'reference-rev is newer than rev')


def _VerifyUserAccepts(message):
  print(message + ' Do you want to proceed? [y/n]')
  if raw_input('> ').lower() != 'y':
    sys.exit()


def _EnsureDirectoryClean(subrepo):
  logging.info('Checking source directory')
  stdout = _GitCmd(['status', '--porcelain'], subrepo)
  # Ignore untracked files.
  if stdout and stdout[:2] != '??':
    logging.error('Failure: please ensure working directory is clean.')
    sys.exit()


def _Die(s, *args):
  logging.error('Failure: ' + s, *args)
  sys.exit(1)


def _WriteToFile(logfile, s, *args, **kwargs):
  if isinstance(s, basestring):
    data = s.format(*args, **kwargs) + '\n'
  else:
    data = '\n'.join(s) + '\n'
  logfile.write(data)


def _PrintFile(path):
  with open(path) as f:
    sys.stdout.write(f.read())


@contextmanager
def _TmpCopyBinarySizeDir():
  """Recursively copy files to a temp dir and yield temp paths."""
  # Needs to be at same level of nesting as the real //tools/binary_size
  # since supersize uses this to find d3 in //third_party.
  tmp_dir = tempfile.mkdtemp(dir=_SRC_ROOT)
  try:
    bs_dir = os.path.join(tmp_dir, 'binary_size')
    shutil.copytree(_BINARY_SIZE_DIR, bs_dir)
    # We also copy the tools supersize needs, but only if they exist.
    tool_prefix = None
    if os.path.exists(_CLANG_UPDATE_PATH):
      if not os.path.exists(os.path.join(_LLVM_TOOLS_DIR, 'llvm-readelf')):
        _RunCmd([_CLANG_UPDATE_PATH, '--package=objdump'])
      tools_dir = os.path.join(bs_dir, 'bintools')
      tool_prefix = os.path.join(tools_dir, 'llvm-')
      shutil.copytree(_LLVM_TOOLS_DIR, tools_dir)
    yield (os.path.join(bs_dir, 'supersize'), tool_prefix)
  finally:
    shutil.rmtree(tmp_dir)


def _CurrentGitHash(subrepo):
  return _GitCmd(['rev-parse', 'HEAD'], subrepo)


def _GenRestoreFunc(subrepo):
  branch = _GitCmd(['rev-parse', '--abbrev-ref', 'HEAD'], subrepo)
  # Happens when the repo didn't start on a named branch.
  if branch == 'HEAD':
    branch = _GitCmd(['rev-parse', 'HEAD'], subrepo)
  def _RestoreFunc():
    logging.warning('Restoring original git checkout')
    _GitCmd(['checkout', branch], subrepo)
  return _RestoreFunc


def _SetRestoreFunc(subrepo):
  atexit.register(_GenRestoreFunc(subrepo))


def main():
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument('rev',
                      help='Find binary size bloat for this commit.')
  parser.add_argument('--archive-directory',
                      default=_DEFAULT_ARCHIVE_DIR,
                      help='Where results are stored.')
  parser.add_argument('--reference-rev',
                      help='Older rev to diff against. If not supplied, '
                           'the previous commit to rev will be used.')
  parser.add_argument('--all',
                      action='store_true',
                      help='Build/download all revs from --reference-rev to '
                           'rev and diff the contiguous revisions.')
  parser.add_argument('--include-slow-options',
                      action='store_true',
                      help='Run some extra steps that take longer to complete. '
                           'This includes apk-patch-size estimation and '
                           'static-initializer counting.')
  parser.add_argument('--single',
                      action='store_true',
                      help='Sets --reference-rev=rev.')
  parser.add_argument('--unstripped',
                      action='store_true',
                      help='Save the unstripped native library when archiving.')
  parser.add_argument(
      '--subrepo',
      help='Specify a subrepo directory to use. Implies '
      '--no-gclient. All git commands will be executed '
      'from the subrepo directory.')
  parser.add_argument('--no-gclient',
                      action='store_true',
                      help='Do not perform gclient sync steps.')
  parser.add_argument('--apply-patch', dest='extra_rev',
                      help='A local commit to cherry-pick before each build. '
                           'This can leave your repo in a broken state if '
                           'the cherry-pick fails.')
  parser.add_argument('--step', type=int,
                      help='Assumes --all and only builds/downloads every '
                           '--step\'th revision.')
  parser.add_argument('-v',
                      '--verbose',
                      action='store_true',
                      help='Show commands executed, extra debugging output'
                           ', and Ninja/GN output.')

  build_group = parser.add_argument_group('build arguments')
  build_group.add_argument('-j',
                           dest='max_jobs',
                           help='Run N jobs in parallel.')
  build_group.add_argument('-l',
                           dest='max_load_average',
                           help='Do not start new jobs if the load average is '
                           'greater than N.')
  build_group.add_argument('--no-goma',
                           action='store_false',
                           dest='use_goma',
                           default=True,
                           help='Do not use goma when building with ninja.')
  build_group.add_argument('--clean',
                           action='store_true',
                           help='Do a clean build for each revision.')
  build_group.add_argument('--gn-args',
                           default='',
                           help='Extra GN args to set.')
  build_group.add_argument('--target-os',
                           default='android',
                           choices=['android', 'linux'],
                           help='target_os gn arg. Default: android.')
  build_group.add_argument('--output-directory',
                           default=_DEFAULT_OUT_DIR,
                           help='ninja output directory. '
                                'Default: %s.' % _DEFAULT_OUT_DIR)
  build_group.add_argument('--enable-chrome-android-internal',
                           action='store_true',
                           help='Allow downstream targets to be built.')
  build_group.add_argument('--target',
                           help='GN target to build. Linux default: chrome. '
                           'Android default: monochrome_public_minimal_apks or '
                           'monochrome_minimal_apks (depending on '
                           '--enable-chrome-android-internal).')
  if len(sys.argv) == 1:
    parser.print_help()
    return 1

  args = parser.parse_args()
  log_level = logging.DEBUG if args.verbose else logging.INFO
  logging.basicConfig(level=log_level,
                      format='%(levelname).1s %(relativeCreated)6d %(message)s')

  build = _BuildHelper(args)
  subrepo = args.subrepo or _SRC_ROOT
  _EnsureDirectoryClean(subrepo)
  _SetRestoreFunc(subrepo)

  if build.IsLinux():
    _VerifyUserAccepts('Linux diffs have known deficiencies (crbug/717550).')

  reference_rev = args.reference_rev or args.rev + '^'
  if args.single:
    reference_rev = args.rev
  _ValidateRevs(args.rev, reference_rev, subrepo, args.extra_rev)
  revs = _GenerateRevList(args.rev, reference_rev, args.all, subrepo, args.step)
  with _TmpCopyBinarySizeDir() as paths:
    supersize_path, tool_prefix = paths
    diffs = [NativeDiff(build.size_name, supersize_path)]
    if build.IsAndroid():
      diffs +=  [
          ResourceSizesDiff(build.apk_name)
      ]
    diff_mngr = _DiffArchiveManager(revs, args.archive_directory, diffs, build,
                                    subrepo, args.include_slow_options,
                                    args.unstripped)
    consecutive_failures = 0
    i = 0
    for i, archive in enumerate(diff_mngr.build_archives):
      if archive.Exists():
        logging.info('Found matching metadata for %s, skipping build step.',
                     archive.rev)
      else:
        build_failure = _SyncAndBuild(archive, build, subrepo, args.no_gclient,
                                      args.extra_rev)
        if build_failure:
          logging.info(
              'Build failed for %s, diffs using this rev will be skipped.',
              archive.rev)
          consecutive_failures += 1
          if len(diff_mngr.build_archives) <= 2:
            _Die('Stopping due to build failure.')
          elif consecutive_failures > _ALLOWED_CONSECUTIVE_FAILURES:
            _Die('%d builds failed in a row, last failure was %s.',
                 consecutive_failures, archive.rev)
        else:
          archive.ArchiveBuildResults(supersize_path, tool_prefix)
          consecutive_failures = 0

      if i != 0:
        diff_mngr.MaybeDiff(i - 1, i)

    diff_mngr.GenerateHtmlReport(0, i)
    diff_mngr.Summarize()


if __name__ == '__main__':
  sys.exit(main())
