#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import contextlib
import copy
import glob
import io
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
import data_quality
import describe
import diff
import file_format
import models
import test_util


_SCRIPT_DIR = os.path.dirname(__file__)
_TEST_DATA_DIR = os.path.join(_SCRIPT_DIR, 'testdata')
_TEST_SDK_DIR = os.path.join(_TEST_DATA_DIR, 'mock_sdk')
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
_TEST_ON_DEMAND_MANIFEST_PATH = os.path.join(_TEST_DATA_DIR,
                                             'AndroidManifest_OnDemand.xml')
_TEST_ALWAYS_INSTALLED_MANIFEST_PATH = os.path.join(
    _TEST_DATA_DIR, 'AndroidManifest_AlwaysInstalled.xml')

# The following files are dynamically created.
_TEST_ELF_PATH = os.path.join(_TEST_OUTPUT_DIR, 'elf')
_TEST_APK_PATH = os.path.join(_TEST_OUTPUT_DIR, 'test.apk')
_TEST_NOT_ON_DEMAND_SPLIT_APK_PATH = os.path.join(_TEST_OUTPUT_DIR,
                                                  'not_on_demand.apk')
_TEST_ON_DEMAND_SPLIT_APK_PATH = os.path.join(_TEST_OUTPUT_DIR, 'on_demand.apk')
_TEST_MINIMAL_APKS_PATH = os.path.join(_TEST_OUTPUT_DIR, 'Bundle.minimal.apks')
_TEST_SSARGS_PATH = os.path.join(_TEST_OUTPUT_DIR, 'test.ssargs')

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
  os.environ['APK_ANALYZER'] = os.path.join(_TEST_SDK_DIR, 'tools', 'bin',
                                            'apkanalyzer')
  os.environ['AAPT2'] = os.path.join(_TEST_SDK_DIR, 'tools', 'bin', 'aapt2')
  try:
    yield
  finally:
    os.environ['PATH'] = prev_path
    del os.environ['APK_ANALYZER']
    del os.environ['AAPT2']


def _RunApp(name, args, debug_measures=False):
  argv = [os.path.join(_SCRIPT_DIR, 'main.py'), name]
  argv.extend(args)
  with _AddMocksToPath():
    env = None
    if debug_measures:
      env = os.environ.copy()
      env['SUPERSIZE_DISABLE_ASYNC'] = '1'
      env['SUPERSIZE_MEASURE_GZIP'] = '1'

    return subprocess.check_output(argv, env=env).decode('utf-8').splitlines()


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
      # Exactly 4MB of data (2^22), with some zipalign overhead.
      info = zipfile.ZipInfo(_TEST_APK_SMALL_SO_PATH)
      info.extra = b'\x00' * 16
      apk_file.writestr(info, IntegrationTest._CreateBlankData(22))
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

    with zipfile.ZipFile(_TEST_NOT_ON_DEMAND_SPLIT_APK_PATH, 'w') as z:
      z.write(_TEST_ALWAYS_INSTALLED_MANIFEST_PATH, 'AndroidManifest.xml')
    with zipfile.ZipFile(_TEST_ON_DEMAND_SPLIT_APK_PATH, 'w') as z:
      z.write(_TEST_ON_DEMAND_MANIFEST_PATH, 'AndroidManifest.xml')

    with zipfile.ZipFile(_TEST_MINIMAL_APKS_PATH, 'w') as apk_file:
      apk_file.writestr('toc.pb', 'x' * 80)
      apk_file.write(_TEST_APK_PATH, 'splits/base-master.apk')
      apk_file.writestr('splits/base-en.apk', 'x' * 10)
      apk_file.write(_TEST_NOT_ON_DEMAND_SPLIT_APK_PATH,
                     'splits/not_on_demand-master.apk')
      apk_file.write(_TEST_ON_DEMAND_SPLIT_APK_PATH,
                     'splits/on_demand-master.apk')
      apk_file.writestr('splits/vr-en.apk', 'x' * 40)


  @classmethod
  def tearDownClass(cls):
    IntegrationTest._SafeRemoveFiles([
        _TEST_ELF_PATH,
        _TEST_APK_PATH,
        _TEST_NOT_ON_DEMAND_SPLIT_APK_PATH,
        _TEST_ON_DEMAND_SPLIT_APK_PATH,
        _TEST_MINIMAL_APKS_PATH,
    ])

  def _CreateTestArgs(self):
    parser = argparse.ArgumentParser()
    archive.AddArguments(parser)
    ret = parser.parse_args(['foo'])
    return ret

  def _CloneSizeInfo(self,
                     use_output_directory=True,
                     use_elf=False,
                     use_apk=False,
                     use_minimal_apks=False,
                     use_pak=False,
                     use_aux_elf=False):
    assert not use_elf or use_output_directory
    assert not (use_apk and use_pak)
    cache_key = (use_output_directory, use_elf, use_apk, use_minimal_apks,
                 use_pak, use_aux_elf)
    if cache_key not in IntegrationTest.cached_size_info:
      knobs = archive.SectionSizeKnobs()
      # Override for testing. Lower the bar for compacting symbols, to allow
      # smaller test cases to be created.
      knobs.max_same_name_alias_count = 3

      args = self._CreateTestArgs()
      args.elf_file = _TEST_ELF_PATH if use_elf or use_aux_elf else None
      args.map_file = _TEST_MAP_PATH
      args.output_directory = _TEST_OUTPUT_DIR if use_output_directory else None
      args.source_directory = _TEST_SOURCE_DIR
      args.tool_prefix = _TEST_TOOL_PREFIX
      apk_so_path = None
      size_info_prefix = None
      extracted_minimal_apk_path = None
      container_name = ''
      if use_apk:
        args.apk_file = _TEST_APK_PATH
      elif use_minimal_apks:
        args.minimal_apks_file = _TEST_MINIMAL_APKS_PATH
        extracted_minimal_apk_path = _TEST_APK_PATH
        container_name = 'Bundle.minimal.apks'
      if use_apk or use_minimal_apks:
        apk_so_path = _TEST_APK_SO_PATH
        if args.output_directory:
          if use_apk:
            orig_path = _TEST_APK_PATH
          else:
            orig_path = _TEST_MINIMAL_APKS_PATH.replace('.minimal.apks', '.aab')
          size_info_prefix = os.path.join(args.output_directory, 'size-info',
                                          os.path.basename(orig_path))
      pak_files = None
      pak_info_file = None
      if use_pak:
        pak_files = [_TEST_APK_LOCALE_PAK_PATH, _TEST_APK_PAK_PATH]
        pak_info_file = _TEST_PAK_INFO_PATH
      linker_name = 'gold'

      # For simplicity, using |args| for both params. This is okay since
      # |args.ssargs_file| is unassigned.
      opts = archive.ContainerArchiveOptions(args, args)
      with _AddMocksToPath():
        build_config = {}
        metadata = archive.CreateMetadata(args, linker_name, build_config)
        container_list = []
        raw_symbols_list = []
        container, raw_symbols = archive.CreateContainerAndSymbols(
            knobs=knobs,
            opts=opts,
            container_name='{}/base.apk'.format(container_name)
            if container_name else '',
            metadata=metadata,
            map_path=args.map_file,
            tool_prefix=args.tool_prefix,
            output_directory=args.output_directory,
            source_directory=args.source_directory,
            elf_path=args.elf_file,
            apk_path=args.apk_file or extracted_minimal_apk_path,
            apk_so_path=apk_so_path,
            pak_files=pak_files,
            pak_info_file=pak_info_file,
            linker_name=linker_name,
            size_info_prefix=size_info_prefix)
        container_list.append(container)
        raw_symbols_list.append(raw_symbols)
        if use_minimal_apks:
          opts.analyze_native = False
          args.split_name = 'not_on_demand'
          args.apk_file = _TEST_NOT_ON_DEMAND_SPLIT_APK_PATH
          args.elf_file = None
          args.map_file = None
          metadata = archive.CreateMetadata(args, None, build_config)
          container, raw_symbols = archive.CreateContainerAndSymbols(
              knobs=knobs,
              opts=opts,
              container_name='{}/not_on_demand.apk'.format(container_name),
              metadata=metadata,
              tool_prefix=args.tool_prefix,
              output_directory=args.output_directory,
              source_directory=args.source_directory,
              apk_path=_TEST_NOT_ON_DEMAND_SPLIT_APK_PATH,
              size_info_prefix=size_info_prefix)
          container_list.append(container)
          raw_symbols_list.append(raw_symbols)
          args.split_name = 'on_demand'
          args.apk_file = _TEST_ON_DEMAND_SPLIT_APK_PATH
          metadata = archive.CreateMetadata(args, None, build_config)
          container, raw_symbols = archive.CreateContainerAndSymbols(
              knobs=knobs,
              opts=opts,
              container_name='{}/on_demand.apk'.format(container_name),
              metadata=metadata,
              tool_prefix=args.tool_prefix,
              output_directory=args.output_directory,
              source_directory=args.source_directory,
              apk_path=_TEST_ON_DEMAND_SPLIT_APK_PATH,
              size_info_prefix=size_info_prefix)
          container_list.append(container)
          raw_symbols_list.append(raw_symbols)
        IntegrationTest.cached_size_info[cache_key] = archive.CreateSizeInfo(
            build_config, container_list, raw_symbols_list)
    return copy.deepcopy(IntegrationTest.cached_size_info[cache_key])

  def _DoArchive(self,
                 archive_path,
                 use_output_directory=True,
                 use_elf=False,
                 use_apk=False,
                 use_ssargs=False,
                 use_minimal_apks=False,
                 use_pak=False,
                 use_aux_elf=None,
                 debug_measures=False,
                 include_padding=False):
    args = [
        archive_path,
        '--source-directory',
        _TEST_SOURCE_DIR,
        #  --map-file ignored for use_ssargs.
        '--map-file',
        _TEST_MAP_PATH,
    ]

    if use_output_directory:
      # Let autodetection find output_directory when --elf-file is used.
      if not use_elf:
        args += ['--output-directory', _TEST_OUTPUT_DIR]
    else:
      args += ['--no-output-directory']
    if use_ssargs:
      args += ['-f', _TEST_SSARGS_PATH]
    elif use_apk:
      args += ['-f', _TEST_APK_PATH]
    elif use_minimal_apks:
      args += ['-f', _TEST_MINIMAL_APKS_PATH]
    elif use_elf:
      args += ['-f', _TEST_ELF_PATH]
    if use_pak:
      args += ['--pak-file', _TEST_APK_LOCALE_PAK_PATH,
               '--pak-file', _TEST_APK_PAK_PATH,
               '--pak-info-file', _TEST_PAK_INFO_PATH]
    if use_aux_elf:
      args += ['--aux-elf-file', _TEST_ELF_PATH]
    if include_padding:
      args += ['--include-padding']
    _RunApp('archive', args, debug_measures=debug_measures)

  def _DoArchiveTest(self,
                     use_output_directory=True,
                     use_elf=False,
                     use_apk=False,
                     use_minimal_apks=False,
                     use_pak=False,
                     use_aux_elf=False,
                     debug_measures=False,
                     include_padding=False):
    with tempfile.NamedTemporaryFile(suffix='.size') as temp_file:
      self._DoArchive(temp_file.name,
                      use_output_directory=use_output_directory,
                      use_elf=use_elf,
                      use_apk=use_apk,
                      use_minimal_apks=use_minimal_apks,
                      use_pak=use_pak,
                      use_aux_elf=use_aux_elf,
                      debug_measures=debug_measures,
                      include_padding=include_padding)
      size_info = archive.LoadAndPostProcessSizeInfo(temp_file.name)
    # Check that saving & loading is the same as directly parsing.
    expected_size_info = self._CloneSizeInfo(
        use_output_directory=use_output_directory,
        use_elf=use_elf,
        use_apk=use_apk,
        use_minimal_apks=use_minimal_apks,
        use_pak=use_pak,
        use_aux_elf=use_aux_elf)
    self.assertEqual(expected_size_info.metadata, size_info.metadata)
    # Don't cluster.
    expected_size_info.symbols = expected_size_info.raw_symbols
    size_info.symbols = size_info.raw_symbols
    expected = list(describe.GenerateLines(expected_size_info, verbose=True))
    actual = list(describe.GenerateLines(size_info, verbose=True))
    self.assertEqual(expected, actual)

    sym_strs = (repr(sym) for sym in size_info.symbols)
    stats = data_quality.DescribeSizeInfoCoverage(size_info)
    if len(size_info.containers) == 1:
      # If there's only one container, merge the its metadata into build_config.
      merged_data_desc = describe.DescribeDict(size_info.metadata_legacy)
    else:
      merged_data_desc = describe.DescribeDict(size_info.build_config)
      for m in size_info.metadata:
        merged_data_desc.extend(describe.DescribeDict(m))
    return itertools.chain(merged_data_desc, stats, sym_strs)

  @_CompareWithGolden()
  def test_Archive(self):
    return self._DoArchiveTest(use_output_directory=False, use_elf=False)

  @_CompareWithGolden()
  def test_Archive_OutputDirectory(self):
    return self._DoArchiveTest()

  @_CompareWithGolden()
  def test_Archive_Elf(self):
    return self._DoArchiveTest(use_elf=True)

  @_CompareWithGolden()
  def test_Archive_Apk(self):
    return self._DoArchiveTest(use_apk=True, use_aux_elf=True)

  @_CompareWithGolden()
  def test_Archive_MinimalApks(self):
    return self._DoArchiveTest(use_minimal_apks=True, use_aux_elf=True)

  @_CompareWithGolden()
  def test_Archive_Pak_Files(self):
    return self._DoArchiveTest(use_pak=True, use_aux_elf=True)

  @_CompareWithGolden(name='Archive_Elf')
  def test_Archive_Elf_DebugMeasures(self):
    return self._DoArchiveTest(use_elf=True, debug_measures=True)

  @_CompareWithGolden(name='Archive_Apk')
  def test_ArchiveSparse(self):
    return self._DoArchiveTest(use_apk=True,
                               use_aux_elf=True,
                               include_padding=True)

  def test_SaveDeltaSizeInfo(self):
    # Check that saving & loading is the same as directly parsing.
    orig_info1 = self._CloneSizeInfo(use_apk=True, use_aux_elf=True)
    orig_info2 = self._CloneSizeInfo(use_elf=True)
    orig_delta = diff.Diff(orig_info1, orig_info2)

    with tempfile.NamedTemporaryFile(suffix='.sizediff') as sizediff_file:
      file_format.SaveDeltaSizeInfo(orig_delta, sizediff_file.name)
      new_info1, new_info2 = archive.LoadAndPostProcessDeltaSizeInfo(
          sizediff_file.name)
    new_delta = diff.Diff(new_info1, new_info2)

    # File format discards unchanged symbols.
    orig_delta.raw_symbols = orig_delta.raw_symbols.WhereDiffStatusIs(
        models.DIFF_STATUS_UNCHANGED).Inverted()

    self.assertEqual(
        '\n'.join(describe.GenerateLines(orig_delta, verbose=True)),
        '\n'.join(describe.GenerateLines(new_delta, verbose=True)))

  @_CompareWithGolden()
  def test_Console(self):
    with tempfile.NamedTemporaryFile(suffix='.size') as size_file, \
         tempfile.NamedTemporaryFile(suffix='.txt') as output_file:
      file_format.SaveSizeInfo(self._CloneSizeInfo(use_elf=True),
                               size_file.name)
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
      file_format.SaveSizeInfo(self._CloneSizeInfo(use_elf=True),
                               size_file.name)
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
      file_format.SaveSizeInfo(self._CloneSizeInfo(use_elf=True),
                               temp_file.name)
      return _RunApp('diff', [temp_file.name, temp_file.name])

  # Runs archive 3 times, and asserts the contents are the same each time.
  def test_Idempotent(self):
    prev_contents = None
    for _ in range(3):
      with tempfile.NamedTemporaryFile(suffix='.size') as temp_file:
        self._DoArchive(temp_file.name)
        contents = temp_file.read()
        self.assertTrue(prev_contents is None or contents == prev_contents)
        prev_contents = contents

  @_CompareWithGolden()
  def test_Diff_Basic(self):
    size_info1 = self._CloneSizeInfo(use_pak=True)
    size_info2 = self._CloneSizeInfo(use_pak=True)
    size_info2.build_config['git_revision'] = 'xyz789'
    container1 = size_info1.containers[0]
    container2 = size_info2.containers[0]
    container1.metadata = {"foo": 1, "bar": [1, 2, 3], "baz": "yes"}
    container2.metadata = {"foo": 1, "bar": [1, 3], "baz": "yes"}

    size_info1.raw_symbols -= size_info1.raw_symbols.WhereNameMatches(
        r'pLinuxKernelCmpxchg|pLinuxKernelMemoryBarrier')
    size_info2.raw_symbols -= size_info2.raw_symbols.WhereNameMatches(
        r'IDS_AW_WEBPAGE_PARENTAL_|IDS_WEB_FONT_FAMILY|IDS_WEB_FONT_SIZE')
    changed_sym = size_info1.raw_symbols.WhereNameMatches('Patcher::Name_')[0]
    changed_sym.size -= 10
    padding_sym = size_info2.raw_symbols.WhereNameMatches('symbol gap 0')[0]
    padding_sym.padding += 20
    padding_sym.size += 20
    # Test pak symbols changing .grd files. They should not show as changed.
    pak_sym = size_info2.raw_symbols.WhereNameMatches(
        r'IDR_PDF_COMPOSITOR_MANIFEST')[0]
    pak_sym.full_name = pak_sym.full_name.replace('.grd', '2.grd')

    # Serialize & de-serialize so that name normalization runs again for the pak
    # symbol.
    bytesio = io.BytesIO()
    file_format.SaveSizeInfo(size_info2, 'path', file_obj=bytesio)
    bytesio.seek(0)
    size_info2 = archive.LoadAndPostProcessSizeInfo('path', file_obj=bytesio)

    d = diff.Diff(size_info1, size_info2)
    d.raw_symbols = d.raw_symbols.Sorted()
    self.assertEqual((1, 2, 3), d.raw_symbols.CountsByDiffStatus()[1:])
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
    size_info = self._CloneSizeInfo(use_elf=True)
    # Show both clustered and non-clustered so that they can be compared.
    size_info.symbols = size_info.raw_symbols
    return itertools.chain(
        describe.GenerateLines(size_info, verbose=True),
        describe.GenerateLines(size_info.symbols._Clustered(), recursive=True,
                               verbose=True),
    )

  @_CompareWithGolden()
  def test_SymbolGroupMethods(self):
    all_syms = self._CloneSizeInfo(use_elf=True).symbols
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

  @_CompareWithGolden()
  def test_ArchiveContainers(self):
    with tempfile.NamedTemporaryFile(suffix='.size') as temp_file:
      self._DoArchive(temp_file.name,
                      use_output_directory=True,
                      use_ssargs=True)
      size_info = archive.LoadAndPostProcessSizeInfo(temp_file.name)

    # Don't cluster.
    size_info.symbols = size_info.raw_symbols
    sym_strs = (repr(sym) for sym in size_info.symbols)
    build_config = describe.DescribeDict(size_info.build_config)
    metadata = itertools.chain.from_iterable(
        itertools.chain([c.name], describe.DescribeDict(c.metadata))
        for c in size_info.containers)
    return itertools.chain(
        ['BuildConfig:'],
        build_config,
        ['Metadata:'],
        metadata,
        ['Symbols:'],
        sym_strs,
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
