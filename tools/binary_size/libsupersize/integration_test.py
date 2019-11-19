#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cStringIO
import contextlib
import copy
import glob
import itertools
import os
import unittest
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

import archive
import describe
import diff
import file_format
import models
import test_util


_SCRIPT_DIR = os.path.dirname(__file__)
_TEST_DATA_DIR = os.path.join(_SCRIPT_DIR, 'testdata')
_TEST_SOURCE_DIR = os.path.join(_TEST_DATA_DIR, 'mock_source_directory')
_TEST_OUTPUT_DIR = os.path.join(_TEST_SOURCE_DIR, 'out', 'Release')
_TEST_TOOL_PREFIX = os.path.join(
    os.path.abspath(_TEST_DATA_DIR), 'mock_toolchain', '')
_TEST_APK_ROOT_DIR = os.path.join(_TEST_DATA_DIR, 'mock_apk')
_TEST_MAP_PATH = os.path.join(_TEST_DATA_DIR, 'test.map')
_TEST_PAK_INFO_PATH = os.path.join(
    _TEST_OUTPUT_DIR, 'size-info/test.apk.pak.info')
_TEST_ELF_FILE_BEGIN = os.path.join(_TEST_OUTPUT_DIR, 'elf.begin')
_TEST_APK_LOCALE_PAK_PATH = os.path.join(_TEST_APK_ROOT_DIR, 'assets/en-US.pak')
_TEST_APK_PAK_PATH = os.path.join(_TEST_APK_ROOT_DIR, 'assets/resources.pak')

# The following files are dynamically created.
_TEST_ELF_PATH = os.path.join(_TEST_OUTPUT_DIR, 'elf')
_TEST_APK_PATH = os.path.join(_TEST_OUTPUT_DIR, 'test.apk')
_TEST_MINIMAL_APKS_PATH = os.path.join(_TEST_OUTPUT_DIR, 'Bundle.minimal.apks')

# Generated file paths relative to apk
_TEST_APK_SO_PATH = 'test.so'
_TEST_APK_SMALL_SO_PATH = 'smalltest.so'
_TEST_APK_DEX_PATH = 'test.dex'
_TEST_APK_OTHER_FILE_PATH = 'assets/icudtl.dat'
_TEST_APK_RES_FILE_PATH = 'res/drawable-v13/test.xml'


def _CompareWithGolden(name=None):
  def real_decorator(func):
    basename = name
    if not basename:
      basename = func.__name__.replace('test_', '')
    golden_path = os.path.join(_TEST_DATA_DIR, basename + '.golden')

    def inner(self):
      actual_lines = func(self)
      actual_lines = (re.sub(r'(elf_mtime=).*', r'\1{redacted}', l)
                      for l in actual_lines)
      actual_lines = (re.sub(r'(Loaded from ).*', r'\1{redacted}', l)
                      for l in actual_lines)
      test_util.Golden.CheckOrUpdate(golden_path, actual_lines)

    return inner
  return real_decorator


@contextlib.contextmanager
def _AddMocksToPath():
  prev_path = os.environ['PATH']
  os.environ['PATH'] = _TEST_TOOL_PREFIX[:-1] + os.path.pathsep + prev_path
  yield
  os.environ['PATH'] = prev_path


def _RunApp(name, args, debug_measures=False):
  argv = [os.path.join(_SCRIPT_DIR, 'main.py'), name, '--no-pypy']
  argv.extend(args)
  with _AddMocksToPath():
    env = None
    if debug_measures:
      env = os.environ.copy()
      env['SUPERSIZE_DISABLE_ASYNC'] = '1'
      env['SUPERSIZE_MEASURE_GZIP'] = '1'

    return subprocess.check_output(argv, env=env).splitlines()


class IntegrationTest(unittest.TestCase):
  maxDiff = None  # Don't trucate diffs in errors.
  cached_size_info = {}

  @staticmethod
  def _CreateBlankData(power_of_two):
    data = '\0'
    for _ in range(power_of_two):
      data = data + data
    return data

  @staticmethod
  def _SafeRemoveFiles(file_names):
    for file_name in file_names:
      if os.path.exists(file_name):
        os.remove(file_name)

  @classmethod
  def setUpClass(cls):
    shutil.copy(_TEST_ELF_FILE_BEGIN, _TEST_ELF_PATH)
    # Exactly 128MB of data (2^27), extra bytes will be accounted in overhead.
    with open(_TEST_ELF_PATH, 'a') as elf_file:
      elf_file.write(IntegrationTest._CreateBlankData(27))

    with zipfile.ZipFile(_TEST_APK_PATH, 'w') as apk_file:
      apk_file.write(_TEST_ELF_PATH, _TEST_APK_SO_PATH)
      # Exactly 4MB of data (2^22).
      apk_file.writestr(
          _TEST_APK_SMALL_SO_PATH, IntegrationTest._CreateBlankData(22))
      # Exactly 1MB of data (2^20).
      apk_file.writestr(
          _TEST_APK_OTHER_FILE_PATH, IntegrationTest._CreateBlankData(20))
      # Exactly 1KB of data (2^10).
      apk_file.writestr(
          _TEST_APK_RES_FILE_PATH, IntegrationTest._CreateBlankData(10))
      locale_pak_rel_path = os.path.relpath(
          _TEST_APK_LOCALE_PAK_PATH, _TEST_APK_ROOT_DIR)
      apk_file.write(_TEST_APK_LOCALE_PAK_PATH, locale_pak_rel_path)
      pak_rel_path = os.path.relpath(_TEST_APK_PAK_PATH, _TEST_APK_ROOT_DIR)
      apk_file.write(_TEST_APK_PAK_PATH, pak_rel_path)
      # Exactly 8MB of data (2^23).
      apk_file.writestr(
          _TEST_APK_DEX_PATH, IntegrationTest._CreateBlankData(23))

    with zipfile.ZipFile(_TEST_MINIMAL_APKS_PATH, 'w') as apk_file:
      apk_file.write(_TEST_APK_PATH, 'splits/base-master.apk')
      apk_file.writestr('splits/base-en.apk', 'x' * 10)
      apk_file.writestr('splits/vr-master.apk', 'x' * 20)
      apk_file.writestr('splits/vr-en.apk', 'x' * 40)
      apk_file.writestr('toc.pb', 'x' * 80)

  @classmethod
  def tearDownClass(cls):
    IntegrationTest._SafeRemoveFiles([
      _TEST_ELF_PATH,
      _TEST_APK_PATH,
      _TEST_MINIMAL_APKS_PATH,
    ])

  def _CloneSizeInfo(self, use_output_directory=True, use_elf=True,
                     use_apk=False, use_minimal_apks=False, use_pak=False):
    assert not use_elf or use_output_directory
    assert not (use_apk and use_pak)
    cache_key = (
        use_output_directory, use_elf, use_apk, use_minimal_apks, use_pak)
    if cache_key not in IntegrationTest.cached_size_info:
      elf_path = _TEST_ELF_PATH if use_elf else None
      output_directory = _TEST_OUTPUT_DIR if use_output_directory else None
      knobs = archive.SectionSizeKnobs()
      # Override for testing. Lower the bar for compacting symbols, to allow
      # smaller test cases to be created.
      knobs.max_same_name_alias_count = 3
      knobs.src_root = _TEST_SOURCE_DIR
      apk_path = None
      minimal_apks_path = None
      apk_so_path = None
      size_info_prefix = None
      extracted_minimal_apk_path = None
      if use_apk:
        apk_path = _TEST_APK_PATH
      elif use_minimal_apks:
        minimal_apks_path = _TEST_MINIMAL_APKS_PATH
        extracted_minimal_apk_path = _TEST_APK_PATH
      if use_apk or use_minimal_apks:
        apk_so_path = _TEST_APK_SO_PATH
        if output_directory:
          if use_apk:
            orig_path = _TEST_APK_PATH
          else:
            orig_path = _TEST_MINIMAL_APKS_PATH.replace('.minimal.apks', '.aab')
          size_info_prefix = os.path.join(
              output_directory, 'size-info', os.path.basename(orig_path))
      pak_files = None
      pak_info_file = None
      if use_pak:
        pak_files = [_TEST_APK_LOCALE_PAK_PATH, _TEST_APK_PAK_PATH]
        pak_info_file = _TEST_PAK_INFO_PATH
      metadata = None
      linker_name = 'gold'
      if use_elf:
        with _AddMocksToPath():
          metadata = archive.CreateMetadata(
              _TEST_MAP_PATH, elf_path, apk_path, minimal_apks_path,
              _TEST_TOOL_PREFIX, output_directory, linker_name)
      section_sizes, raw_symbols = archive.CreateSectionSizesAndSymbols(
          map_path=_TEST_MAP_PATH, tool_prefix=_TEST_TOOL_PREFIX,
          elf_path=elf_path, output_directory=output_directory,
          apk_path=apk_path or extracted_minimal_apk_path,
          apk_so_path=apk_so_path, metadata=metadata, pak_files=pak_files,
          pak_info_file=pak_info_file, linker_name=linker_name,
          size_info_prefix=size_info_prefix, knobs=knobs)
      IntegrationTest.cached_size_info[cache_key] = archive.CreateSizeInfo(
          section_sizes, raw_symbols, metadata=metadata)
    return copy.deepcopy(IntegrationTest.cached_size_info[cache_key])

  def _DoArchive(self, archive_path, use_output_directory=True, use_elf=True,
                 use_apk=False, use_minimal_apks=False, use_pak=False,
                 debug_measures=False):
    args = [
      archive_path,
      '--map-file', _TEST_MAP_PATH,
      '--source-directory', _TEST_SOURCE_DIR,
    ]
    if use_output_directory:
      # Let autodetection find output_directory when --elf-file is used.
      if not use_elf:
        args += ['--output-directory', _TEST_OUTPUT_DIR]
    else:
      args += ['--no-source-paths']
    if use_apk:
      args += ['-f', _TEST_APK_PATH]
    elif use_minimal_apks:
      args += ['-f', _TEST_MINIMAL_APKS_PATH]
    if use_elf:
      if use_apk or use_minimal_apks:
        args += ['--elf-file', _TEST_ELF_PATH]
      else:
        args += ['-f', _TEST_ELF_PATH]
    if use_pak:
      args += ['--pak-file', _TEST_APK_LOCALE_PAK_PATH,
               '--pak-file', _TEST_APK_PAK_PATH,
               '--pak-info-file', _TEST_PAK_INFO_PATH]
    _RunApp('archive', args, debug_measures=debug_measures)

  def _DoArchiveTest(self, use_output_directory=True, use_elf=True,
                     use_apk=False, use_minimal_apks=False, use_pak=False,
                     debug_measures=False):
    with tempfile.NamedTemporaryFile(suffix='.size') as temp_file:
      self._DoArchive(
          temp_file.name, use_output_directory=use_output_directory,
          use_elf=use_elf, use_apk=use_apk, use_minimal_apks=use_minimal_apks,
          use_pak=use_pak, debug_measures=debug_measures)
      size_info = archive.LoadAndPostProcessSizeInfo(temp_file.name)
    # Check that saving & loading is the same as directly parsing.
    expected_size_info = self._CloneSizeInfo(
        use_output_directory=use_output_directory, use_elf=use_elf,
        use_apk=use_apk, use_minimal_apks=use_minimal_apks, use_pak=use_pak)
    self.assertEquals(expected_size_info.metadata, size_info.metadata)
    # Don't cluster.
    expected_size_info.symbols = expected_size_info.raw_symbols
    size_info.symbols = size_info.raw_symbols
    expected = list(describe.GenerateLines(expected_size_info, verbose=True))
    actual = list(describe.GenerateLines(size_info, verbose=True))
    self.assertEquals(expected, actual)

    sym_strs = (repr(sym) for sym in size_info.symbols)
    stats = describe.DescribeSizeInfoCoverage(size_info)
    if size_info.metadata:
      metadata = describe.DescribeMetadata(size_info.metadata)
    else:
      metadata = []
    return itertools.chain(metadata, stats, sym_strs)

  @_CompareWithGolden()
  def test_Archive(self):
    return self._DoArchiveTest(use_output_directory=False, use_elf=False)

  @_CompareWithGolden()
  def test_Archive_OutputDirectory(self):
    return self._DoArchiveTest(use_elf=False)

  @_CompareWithGolden()
  def test_Archive_Elf(self):
    return self._DoArchiveTest()

  @_CompareWithGolden()
  def test_Archive_Apk(self):
    return self._DoArchiveTest(use_apk=True)

  @_CompareWithGolden()
  def test_Archive_MinimalApks(self):
    return self._DoArchiveTest(use_minimal_apks=True)

  @_CompareWithGolden()
  def test_Archive_Pak_Files(self):
    return self._DoArchiveTest(use_pak=True)

  @_CompareWithGolden(name='Archive_Elf')
  def test_Archive_Elf_DebugMeasures(self):
    return self._DoArchiveTest(debug_measures=True)

  @_CompareWithGolden()
  def test_Console(self):
    with tempfile.NamedTemporaryFile(suffix='.size') as size_file, \
         tempfile.NamedTemporaryFile(suffix='.txt') as output_file:
      file_format.SaveSizeInfo(self._CloneSizeInfo(), size_file.name)
      query = [
          'ShowExamples()',
          'ExpandRegex("_foo_")',
          'canned_queries.CategorizeGenerated()',
          'canned_queries.CategorizeByChromeComponent()',
          'canned_queries.LargeFiles()',
          'canned_queries.TemplatesByName()',
          'canned_queries.StaticInitializers()',
          'canned_queries.PakByPath()',
          'Print(ReadStringLiterals(elf_path={}))'.format(repr(_TEST_ELF_PATH)),
          'Print(size_info, to_file=%r)' % output_file.name,
      ]
      ret = _RunApp('console', [size_file.name, '--query', '; '.join(query)])
      with open(output_file.name) as f:
        ret.extend(l.rstrip() for l in f)
      return ret

  @_CompareWithGolden()
  def test_Csv(self):
    with tempfile.NamedTemporaryFile(suffix='.size') as size_file, \
         tempfile.NamedTemporaryFile(suffix='.txt') as output_file:
      file_format.SaveSizeInfo(self._CloneSizeInfo(), size_file.name)
      query = [
          'Csv(size_info, to_file=%r)' % output_file.name,
      ]
      ret = _RunApp('console', [size_file.name, '--query', '; '.join(query)])
      with open(output_file.name) as f:
        ret.extend(l.rstrip() for l in f)
      return ret

  @_CompareWithGolden()
  def test_Diff_NullDiff(self):
    with tempfile.NamedTemporaryFile(suffix='.size') as temp_file:
      file_format.SaveSizeInfo(self._CloneSizeInfo(), temp_file.name)
      return _RunApp('diff', [temp_file.name, temp_file.name])

  # Runs archive 3 times, and asserts the contents are the same each time.
  def test_Idempotent(self):
    prev_contents = None
    for _ in xrange(3):
      with tempfile.NamedTemporaryFile(suffix='.size') as temp_file:
        self._DoArchive(temp_file.name)
        contents = temp_file.read()
        self.assertTrue(prev_contents is None or contents == prev_contents)
        prev_contents = contents

  @_CompareWithGolden()
  def test_Diff_Basic(self):
    size_info1 = self._CloneSizeInfo(use_elf=False, use_pak=True)
    size_info2 = self._CloneSizeInfo(use_elf=False, use_pak=True)
    size_info1.metadata = {"foo": 1, "bar": [1,2,3], "baz": "yes"}
    size_info2.metadata = {"foo": 1, "bar": [1,3], "baz": "yes"}

    size_info1.raw_symbols -= size_info1.raw_symbols[:2]
    size_info2.raw_symbols -= size_info2.raw_symbols[-3:]
    changed_sym = size_info1.raw_symbols.WhereNameMatches('Patcher::Name_')[0]
    changed_sym.size -= 10
    padding_sym = size_info2.raw_symbols.WhereNameMatches('symbol gap 0')[0]
    padding_sym.padding += 20
    padding_sym.size += 20
    pak_sym = size_info2.raw_symbols.WhereInSection(
        models.SECTION_PAK_NONTRANSLATED)[0]
    pak_sym.full_name = 'foo: ' + pak_sym.full_name.split()[-1]

    # Serialize & de-serialize so that name normalization runs again for the pak
    # symbol.
    stringio = cStringIO.StringIO()
    file_format.SaveSizeInfo(size_info2, 'path', file_obj=stringio)
    stringio.seek(0)
    size_info2 = archive.LoadAndPostProcessSizeInfo('path', file_obj=stringio)

    d = diff.Diff(size_info1, size_info2)
    d.raw_symbols = d.raw_symbols.Sorted()
    self.assertEquals(d.raw_symbols.CountsByDiffStatus()[1:], (2, 2, 3))
    changed_sym = d.raw_symbols.WhereNameMatches('Patcher::Name_')[0]
    padding_sym = d.raw_symbols.WhereNameMatches('symbol gap 0')[0]
    bss_sym = d.raw_symbols.WhereInSection(models.SECTION_BSS)[0]
    # Padding-only deltas should sort after all non-padding changes.
    padding_idx = d.raw_symbols.index(padding_sym)
    changed_idx = d.raw_symbols.index(changed_sym)
    bss_idx = d.raw_symbols.index(bss_sym)
    self.assertLess(changed_idx, padding_idx)
    # And before bss.
    self.assertLess(padding_idx, bss_idx)

    return describe.GenerateLines(d, verbose=True)

  @_CompareWithGolden()
  def test_FullDescription(self):
    size_info = self._CloneSizeInfo()
    # Show both clustered and non-clustered so that they can be compared.
    size_info.symbols = size_info.raw_symbols
    return itertools.chain(
        describe.GenerateLines(size_info, verbose=True),
        describe.GenerateLines(size_info.symbols._Clustered(), recursive=True,
                               verbose=True),
    )

  @_CompareWithGolden()
  def test_SymbolGroupMethods(self):
    all_syms = self._CloneSizeInfo().symbols
    global_syms = all_syms.WhereNameMatches('GLOBAL')
    # Tests Filter(), Inverted(), and __sub__().
    non_global_syms = global_syms.Inverted()
    self.assertEqual(non_global_syms, (all_syms - global_syms))
    # Tests Sorted() and __add__().
    self.assertEqual(all_syms.Sorted(),
                     (global_syms + non_global_syms).Sorted())
    # Tests GroupedByName() and __len__().
    return itertools.chain(
        ['GroupedByName()'],
        describe.GenerateLines(all_syms.GroupedByName()),
        ['GroupedByName(depth=1)'],
        describe.GenerateLines(all_syms.GroupedByName(depth=1)),
        ['GroupedByName(depth=-1)'],
        describe.GenerateLines(all_syms.GroupedByName(depth=-1)),
        ['GroupedByName(depth=1, min_count=2)'],
        describe.GenerateLines(all_syms.GroupedByName(depth=1, min_count=2)),
    )


def main():
  argv = sys.argv
  if len(argv) > 1 and argv[1] == '--update':
    argv.pop(0)
    test_util.Golden.EnableUpdate()
    for f in glob.glob(os.path.join(_TEST_DATA_DIR, '*.golden')):
      os.unlink(f)

  unittest.main(argv=argv, verbosity=2)


if __name__ == '__main__':
  main()
