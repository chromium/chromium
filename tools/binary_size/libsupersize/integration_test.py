#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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
import json_config_parser
import models
import pakfile
import test_util
import zip_util


_SCRIPT_DIR = os.path.dirname(__file__)
_TEST_DATA_DIR = test_util.TEST_DATA_DIR
_TEST_SOURCE_DIR = test_util.TEST_SOURCE_DIR
_TEST_OUTPUT_DIR = test_util.TEST_OUTPUT_DIR
_TEST_APK_ROOT_DIR = os.path.join(_TEST_DATA_DIR, 'mock_apk')
_TEST_MAP_PATH = os.path.join(_TEST_DATA_DIR, 'test.map')
_TEST_PAK_INFO_PATH = os.path.join(
    _TEST_OUTPUT_DIR, 'size-info/test.apk.pak.info')
_TEST_ELF_FILE_BEGIN = os.path.join(_TEST_OUTPUT_DIR, 'elf.begin')
_TEST_APK_LOCALE_PAK_SUBPATH = 'assets/en-US.pak'
_TEST_APK_PAK_SUBPATH = 'assets/resources.pak'
_TEST_APK_LOCALE_PAK_PATH = os.path.join(_TEST_APK_ROOT_DIR,
                                         _TEST_APK_LOCALE_PAK_SUBPATH)
_TEST_APK_PAK_PATH = os.path.join(_TEST_APK_ROOT_DIR, _TEST_APK_PAK_SUBPATH)
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
_TEST_APK_SO_PATH = 'lib/armeabi-v7a/test.so'
_TEST_APK_SMALL_SO_PATH = 'lib/x86/smalltest.so'
_TEST_APK_DEX_PATH = 'classes.dex'
_TEST_APK_OTHER_FILE_PATH = 'assets/icudtl.dat'
_TEST_APK_RES_FILE_PATH = 'res/drawable-v13/test.xml'

_TEST_CONFIG_JSON = os.path.join(_TEST_DATA_DIR, 'supersize.json')
_TEST_JSON_CONFIG = json_config_parser.Parse(_TEST_CONFIG_JSON, None)
_TEST_PATH_DEFAULTS = {
    'assets/icudtl.dat': '../../third_party/icu/android/icudtl.dat',
}

_TEST_DEX_AFTER_PATH = os.path.join(_TEST_DATA_DIR,
                                    'mock_dex/after/classes.dex')


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


def _RunApp(name, args, debug_measures=False):
  argv = [os.path.join(_SCRIPT_DIR, 'main.py'), name]
  argv.extend(args)
  if '-v' in sys.argv:
    argv.append('-v')
  with test_util.AddMocksToPath():
    env = None
    if debug_measures:
      env = os.environ.copy()
      env['SUPERSIZE_DISABLE_ASYNC'] = '1'
      env['SUPERSIZE_MEASURE_GZIP'] = '1'

    return subprocess.check_output(argv, env=env).decode('utf-8').splitlines()


def _AllMetadata(size_info):
  return [c.metadata for c in size_info.containers]


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
      apk_file.write(_TEST_DEX_AFTER_PATH, _TEST_APK_DEX_PATH)

    with zipfile.ZipFile(_TEST_NOT_ON_DEMAND_SPLIT_APK_PATH, 'w') as z:
      z.write(_TEST_ALWAYS_INSTALLED_MANIFEST_PATH, 'AndroidManifest.xml')
    with zipfile.ZipFile(_TEST_ON_DEMAND_SPLIT_APK_PATH, 'w') as z:
      z.write(_TEST_ON_DEMAND_MANIFEST_PATH, 'AndroidManifest.xml')

    with zipfile.ZipFile(_TEST_MINIMAL_APKS_PATH, 'w') as apk_file:
      apk_file.writestr('toc.pb', 'x' * 80)
      apk_file.write(_TEST_APK_PATH, 'splits/base-master.apk')
      apk_file.writestr('splits/base-en.apk', 'x' * 10)  # Ignored.
      apk_file.write(_TEST_NOT_ON_DEMAND_SPLIT_APK_PATH,
                     'splits/base-hi.apk')  # Not Ignored.
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

  def _CloneSizeInfo(self,
                     *,
                     use_output_directory=True,
                     use_elf=False,
                     use_apk=False,
                     use_minimal_apks=False,
                     use_pak=False,
                     use_aux_elf=False,
                     ignore_linker_map=False):
    assert not use_elf or use_output_directory
    assert not (use_apk and use_pak)
    assert not (use_apk and use_minimal_apks)
    cache_key = (use_output_directory, use_elf, use_apk, use_minimal_apks,
                 use_pak, use_aux_elf, ignore_linker_map)
    if cache_key not in IntegrationTest.cached_size_info:
      output_directory = _TEST_OUTPUT_DIR if use_output_directory else None

      def iter_specs():
        pak_spec = None
        if use_pak or use_apk or use_minimal_apks:
          pak_spec = archive.PakSpec()
          if use_pak:
            pak_spec.pak_paths = [_TEST_APK_LOCALE_PAK_PATH, _TEST_APK_PAK_PATH]
            pak_spec.pak_info_path = _TEST_PAK_INFO_PATH
          else:
            pak_spec.apk_pak_paths = [
                _TEST_APK_LOCALE_PAK_SUBPATH, _TEST_APK_PAK_SUBPATH
            ]

        native_spec = archive.NativeSpec()

        # TODO(crbug.com/40757867): Remove when we implement string literal
        #     tracking without map files.
        if ignore_linker_map:
          native_spec.track_string_literals = False
        else:
          native_spec.map_path = _TEST_MAP_PATH
          native_spec.linker_name = 'gold'

        if use_elf or use_aux_elf:
          native_spec.elf_path = _TEST_ELF_PATH

        apk_spec = None
        if use_apk or use_minimal_apks:
          apk_spec = archive.ApkSpec(apk_path=_TEST_APK_PATH)
          apk_spec.path_defaults = _TEST_PATH_DEFAULTS
          apk_spec.ignore_apk_paths.update(
              ['classes.dex', _TEST_APK_SO_PATH, _TEST_APK_SMALL_SO_PATH])
          if pak_spec and pak_spec.apk_pak_paths:
            apk_spec.ignore_apk_paths.update(pak_spec.apk_pak_paths)
          if output_directory:
            orig_path = _TEST_APK_PATH
            if use_minimal_apks:
              orig_path = _TEST_MINIMAL_APKS_PATH.replace(
                  '.minimal.apks', '.aab')
            apk_spec.size_info_prefix = os.path.join(
                output_directory, 'size-info', os.path.basename(orig_path))

          native_spec.apk_so_path = _TEST_APK_SO_PATH
          small_native_spec = archive.NativeSpec(
              apk_so_path=_TEST_APK_SMALL_SO_PATH)

        if use_minimal_apks:
          apk_spec.minimal_apks_path = _TEST_MINIMAL_APKS_PATH
          apk_spec.split_name = 'base'

        container_name = ''
        if use_apk:
          container_name = 'test.apk'
        if use_minimal_apks:
          container_name = 'Bundle.minimal.apks/base.apk'
        container_spec = archive.ContainerSpec(
            container_name=container_name,
            apk_spec=apk_spec,
            pak_spec=pak_spec,
            native_spec=native_spec,
            source_directory=_TEST_SOURCE_DIR,
            output_directory=output_directory)
        if not apk_spec:
          yield container_spec
          return

        container_spec.native_spec = None
        yield container_spec
        container_spec = copy.copy(container_spec)
        container_spec.container_name = (
            f'{container_name}/test.so (armeabi-v7a)')
        container_spec.pak_spec = None
        container_spec.native_spec = native_spec
        yield container_spec
        container_spec = copy.copy(container_spec)
        container_spec.container_name = f'{container_name}/smalltest.so (x86)'
        container_spec.native_spec = small_native_spec
        yield container_spec
        container_spec = None

        if use_minimal_apks:
          for split_name, apk_path in [
              ('base-hi', _TEST_NOT_ON_DEMAND_SPLIT_APK_PATH),
              ('not_on_demand', _TEST_NOT_ON_DEMAND_SPLIT_APK_PATH),
              ('on_demand', _TEST_ON_DEMAND_SPLIT_APK_PATH),
          ]:
            apk_spec = archive.ApkSpec(
                minimal_apks_path=_TEST_MINIMAL_APKS_PATH,
                apk_path=apk_path,
                split_name=split_name,
                size_info_prefix=apk_spec.size_info_prefix)
            container_name = 'Bundle.minimal.apks/%s.apk' % split_name
            if split_name == 'on_demand':
              container_name += '?'
              apk_spec.default_component = 'DEFAULT'
            yield archive.ContainerSpec(container_name=container_name,
                                        apk_spec=apk_spec,
                                        pak_spec=None,
                                        native_spec=None,
                                        source_directory=_TEST_SOURCE_DIR,
                                        output_directory=output_directory)

      with test_util.AddMocksToPath(), \
          zip_util.ApkFileManager() as apk_file_manager:
        build_config = archive.CreateBuildConfig(output_directory,
                                                 _TEST_SOURCE_DIR)
        container_specs = list(iter_specs())
        size_info = archive.CreateSizeInfo(container_specs, build_config,
                                           _TEST_JSON_CONFIG, apk_file_manager)
        IntegrationTest.cached_size_info[cache_key] = size_info

    return copy.deepcopy(IntegrationTest.cached_size_info[cache_key])

  def _DoArchive(self,
                 archive_path,
                 *,
                 use_output_directory=True,
                 use_elf=False,
                 use_map=False,
                 use_apk=False,
                 use_ssargs=False,
                 use_minimal_apks=False,
                 use_pak=False,
                 use_aux_elf=None,
                 ignore_linker_map=False,
                 debug_measures=False):
    args = [
        archive_path,
        '--source-directory',
        _TEST_SOURCE_DIR,
        '--json-config',
        _TEST_CONFIG_JSON,
        '--abi-filter',
        'armeabi-v7a',
    ]

    if use_output_directory:
      # Let autodetection find output_directory when --elf-file is used.
      if not use_elf:
        args += ['--output-directory', _TEST_OUTPUT_DIR]
    else:
      args += ['--no-output-directory']
    if use_ssargs:
      args += ['-f', _TEST_SSARGS_PATH]
    if use_apk:
      args += ['-f', _TEST_APK_PATH]
    if use_minimal_apks:
      args += ['-f', _TEST_MINIMAL_APKS_PATH]
    if use_elf:
      args += ['-f', _TEST_ELF_PATH]
    if use_map:
      args += ['-f', _TEST_MAP_PATH]
    if use_pak:
      args += ['--pak-file', _TEST_APK_LOCALE_PAK_PATH,
               '--pak-file', _TEST_APK_PAK_PATH,
               '--pak-info-file', _TEST_PAK_INFO_PATH]

    if ignore_linker_map:
      args += ['--no-map-file']
    elif not use_ssargs and not use_map:
      args += ['--aux-map-file', _TEST_MAP_PATH]

    if use_aux_elf:
      args += ['--aux-elf-file', _TEST_ELF_PATH]

    _RunApp('archive', args, debug_measures=debug_measures)

  def _FixupExpectedSizeInfoForMinimalApks(self, expected_size_info):
    # DEX string symbols "actual" size_info have object_path assigned to
    # '$SYSTEM/base-master.apk', which is from the value written to minimal
    # apks file. Meanwhile, the correpsonding symbols in "expected" are
    # '$SYSTEM/test.apk' because that's the file passed.
    # * Changing both to "test.apk" is bad because SuperSize has code to
    #   handle '-master.apk' filename.
    # * Changing both to "base-master.apk" is bad because the name "test.apk"
    #   is engrained in the test.
    # As a kludge, here we mutate "expected" values for the affected symbols.
    for sym in expected_size_info.raw_symbols:
      if (sym.section_name == models.SECTION_DEX
          and sym.object_path == '$SYSTEM/test.apk'):
        sym.object_path = '$SYSTEM/base-master.apk'

  def _DoArchiveTest(self,
                     *,
                     use_output_directory=True,
                     use_map=False,
                     use_elf=False,
                     use_apk=False,
                     use_minimal_apks=False,
                     use_pak=False,
                     use_aux_elf=False,
                     ignore_linker_map=False,
                     debug_measures=False):
    with tempfile.NamedTemporaryFile(suffix='.size') as temp_file:
      self._DoArchive(temp_file.name,
                      use_output_directory=use_output_directory,
                      use_map=use_map,
                      use_elf=use_elf,
                      use_apk=use_apk,
                      use_minimal_apks=use_minimal_apks,
                      use_pak=use_pak,
                      use_aux_elf=use_aux_elf,
                      ignore_linker_map=ignore_linker_map,
                      debug_measures=debug_measures)
      size_info = archive.LoadAndPostProcessSizeInfo(temp_file.name)
    # Check that saving & loading is the same as directly parsing.
    expected_size_info = self._CloneSizeInfo(
        use_output_directory=use_output_directory,
        use_elf=use_elf,
        use_apk=use_apk,
        use_minimal_apks=use_minimal_apks,
        use_pak=use_pak,
        use_aux_elf=use_aux_elf,
        ignore_linker_map=ignore_linker_map)
    if use_minimal_apks:
      self._FixupExpectedSizeInfoForMinimalApks(expected_size_info)
    self.assertEqual(_AllMetadata(expected_size_info), _AllMetadata(size_info))
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
      for m in _AllMetadata(size_info):
        merged_data_desc.extend(describe.DescribeDict(m))
    return itertools.chain(merged_data_desc, stats, sym_strs)

  @_CompareWithGolden()
  def test_Archive(self):
    return self._DoArchiveTest(use_output_directory=False, use_map=True)

  @_CompareWithGolden()
  def test_Archive_OutputDirectory(self):
    return self._DoArchiveTest(use_map=True)

  @_CompareWithGolden()
  def test_Archive_Elf(self):
    return self._DoArchiveTest(use_elf=True)

  @_CompareWithGolden()
  def test_Archive_Elf_No_Map(self):
    return self._DoArchiveTest(use_elf=True, ignore_linker_map=True)

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

  def test_SaveDeltaSizeInfo(self):
    # Check that saving & loading is the same as directly parsing.
    orig_info1 = self._CloneSizeInfo(use_apk=True, use_aux_elf=True)
    orig_info2 = copy.deepcopy(orig_info1)
    # Remove the last 5 symbols to ensure a small but non-empty diff.
    orig_info2.raw_symbols._symbols[-5:] = []
    orig_delta = diff.Diff(orig_info1, orig_info2)

    with tempfile.NamedTemporaryFile(suffix='.sizediff') as sizediff_file:
      file_format.SaveDeltaSizeInfo(orig_delta, sizediff_file.name)
      new_info1, new_info2 = archive.LoadAndPostProcessDeltaSizeInfo(
          sizediff_file.name)
    new_delta = diff.Diff(new_info1, new_info2)

    self.assertEqual(list(describe.GenerateLines(orig_delta, verbose=True)),
                     list(describe.GenerateLines(new_delta, verbose=True)))

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
        self._DoArchive(temp_file.name, use_map=True)
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
    metrics_by_file = itertools.chain.from_iterable(
        itertools.chain([c.name], describe.DescribeDict(c.metrics_by_file))
        for c in size_info.containers)
    return itertools.chain(['BuildConfig:'], build_config, ['Metadata:'],
                           metadata, ['Symbols:'], sym_strs, ['MetricsByFile:'],
                           metrics_by_file)


def main():
  argv = sys.argv
  if len(argv) > 1 and argv[1] == '--update':
    argv.pop(0)
    test_util.Golden.EnableUpdate()
    for f in glob.glob(os.path.join(_TEST_DATA_DIR, '*.golden')):
      os.unlink(f)

  unittest.main(argv=argv)


if __name__ == '__main__':
  main()
