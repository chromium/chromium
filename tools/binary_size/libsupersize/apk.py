# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Functions for creating APK symbols."""

import logging
import os
import posixpath
import re
import zipfile

import archive_util
import arsc_parser
import file_format
import models
import zip_util


RESOURCES_ARSC_FILE = 'resources.arsc'


class _ResourcePathDeobfuscator:
  def __init__(self, pathmap_path):
    self._pathmap = self._LoadResourcesPathmap(pathmap_path)

  def _LoadResourcesPathmap(self, pathmap_path):
    """Loads the pathmap of obfuscated resource paths.

    Returns: A dict mapping from obfuscated paths to original paths or an
             empty dict if passed a None |pathmap_path|.
    """
    if pathmap_path is None:
      return {}

    pathmap = {}
    with open(pathmap_path, 'r') as f:
      for line in f:
        line = line.strip()
        if line.startswith('--') or line == '':
          continue
        original, renamed = line.split(' -> ')
        pathmap[renamed] = original
    return pathmap

  def MaybeRemapPath(self, path):
    long_path = self._pathmap.get(path)
    if long_path:
      return long_path
    # if processing a .minimal.apks, we are actually just processing the base
    # split.
    long_path = self._pathmap.get('base/{}'.format(path))
    if long_path:
      # The first 5 chars are 'base/', which we don't need because we are
      # looking directly inside the base apk.
      return long_path[5:]
    return path


class _ResourceSourceMapper:
  def __init__(self, size_info_prefix, path_defaults):
    self._path_defaults = path_defaults or {}
    if size_info_prefix:
      self._res_info = self._LoadResInfo(size_info_prefix)
    else:
      self._res_info = dict()
    self._pattern_dollar_underscore = re.compile(r'\$+(.*?)(?:__\d)+')
    self._pattern_version_suffix = re.compile(r'-v\d+/')

  @staticmethod
  def _ParseResInfoFile(res_info_path):
    with open(res_info_path, 'r') as info_file:
      return dict(l.rstrip().split('\t') for l in info_file)

  def _LoadResInfo(self, size_info_prefix):
    apk_res_info_path = size_info_prefix + '.res.info'
    res_info_without_root = self._ParseResInfoFile(apk_res_info_path)
    # We package resources in the res/ folder only in the apk.
    res_info = {
        os.path.join('res', dest): source
        for dest, source in res_info_without_root.items()
    }
    res_info.update(self._path_defaults)
    return res_info

  def FindSourceForPath(self, path):
    # Sometimes android adds $ in front and __# before extension.
    path = self._pattern_dollar_underscore.sub(r'\1', path)
    path = archive_util.RemoveAssetSuffix(path)
    ret = self._res_info.get(path)
    if ret:
      return ret
    # Android build tools may append extra -v flags for the root dir.
    path = self._pattern_version_suffix.sub('/', path)
    ret = self._res_info.get(path)
    if ret:
      return ret
    return ''


def CreateArscSymbols(apk_spec):
  """Creates symbols for resources"""
  raw_symbols = []
  metrics_by_file = {}
  with zipfile.ZipFile(apk_spec.apk_path) as src_zip:
    arsc_infos = [
        info for info in src_zip.infolist()
        if info.filename == RESOURCES_ARSC_FILE
    ]
    if len(arsc_infos) != 0:
      assert len(arsc_infos) == 1
      filename = arsc_infos[0].filename
      metrics = {}
      arsc_data = src_zip.read(arsc_infos[0])
      arsc_file = arsc_parser.ArscFile(arsc_data)
      source_path = posixpath.join(models.APK_PREFIX_PATH, filename)
      overhead = len(arsc_data)
      for inner_path, chunk in arsc_file.VisitPreOrder():
        if not chunk.children:  # Leaf chunk.
          name = chunk.symbol_name()
          sym_source_path = (f'{source_path}/{inner_path}'
                             if inner_path else source_path)
          sym = models.Symbol(models.SECTION_ARSC,
                              chunk.size - chunk.placeholder,
                              source_path=sym_source_path,
                              full_name=name)
          raw_symbols.append(sym)
          if chunk.placeholder:
            placeholder_sym = (models.Symbol(
                models.SECTION_ARSC,
                chunk.placeholder,
                source_path=sym_source_path,
                full_name=f'{name} (placeholders)'))
            raw_symbols.append(placeholder_sym)

          if isinstance(chunk, arsc_parser.ArscResTableTypeSpec):
            metrics[f'{models.METRICS_COUNT}/{chunk.type_str}'] = (
                chunk.entry_count)

          overhead -= chunk.size
      if overhead > 0:
        raw_symbols.append(
            models.Symbol(models.SECTION_ARSC,
                          overhead,
                          source_path=source_path,
                          full_name='Overhead: ARSC'))
      metrics_by_file[filename] = metrics

  section_ranges = {}
  archive_util.ExtendSectionRange(section_ranges, models.SECTION_ARSC,
                                  sum(s.size for s in raw_symbols))
  return section_ranges, raw_symbols, metrics_by_file


def CreateMetadata(apk_spec, include_file_details, shorten_path):
  """Returns metadata for the given apk_spec."""
  logging.debug('Constructing APK metadata')
  apk_metadata = {}
  if include_file_details:
    if apk_spec.mapping_path:
      apk_metadata[models.METADATA_PROGUARD_MAPPING_FILENAME] = shorten_path(
          apk_spec.mapping_path)
  if apk_spec.minimal_apks_path:
    apk_metadata[models.METADATA_APK_FILENAME] = shorten_path(
        apk_spec.minimal_apks_path)
    apk_metadata[models.METADATA_APK_SPLIT_NAME] = apk_spec.split_name
  else:
    apk_metadata[models.METADATA_APK_FILENAME] = shorten_path(apk_spec.apk_path)
  return apk_metadata


def CreateApkOtherSymbols(apk_spec):
  """Creates symbols for resources / assets within the apk.

  Returns:
    A tuple of (section_ranges, raw_symbols, apk_metadata, apk_metrics_by_file).
  """
  logging.info('Creating symbols for other APK entries')
  res_source_mapper = _ResourceSourceMapper(apk_spec.size_info_prefix,
                                            apk_spec.path_defaults)
  resource_deobfuscator = _ResourcePathDeobfuscator(
      apk_spec.resources_pathmap_path)
  raw_symbols = []
  zip_info_total = 0
  zipalign_total = 0
  with zipfile.ZipFile(apk_spec.apk_path) as z:
    signing_block_size = zip_util.MeasureApkSignatureBlock(z)
    for zip_info in z.infolist():
      zip_info_total += zip_info.compress_size
      # Account for zipalign overhead that exists in local file header.
      zipalign_total += zip_util.ReadZipInfoExtraFieldLength(z, zip_info)
      # Account for zipalign overhead that exists in central directory header.
      # Happens when python aligns entries in apkbuilder.py, but does not
      # exist when using Android's zipalign. E.g. for bundle .apks files.
      zipalign_total += len(zip_info.extra)

      # Skip files that we explicitly analyze: .so, .dex, .pak, and .arsc.
      if zip_info.filename in apk_spec.ignore_apk_paths:
        continue

      resource_filename = resource_deobfuscator.MaybeRemapPath(
          zip_info.filename)
      source_path = res_source_mapper.FindSourceForPath(resource_filename)
      if not source_path:
        source_path = posixpath.join(models.APK_PREFIX_PATH, resource_filename)
      raw_symbols.append(
          models.Symbol(
              models.SECTION_OTHER,
              zip_info.compress_size,
              source_path=source_path,
              full_name=resource_filename))  # Full name must disambiguate

  # Store zipalign overhead and signing block size as metadata rather than an
  # "Overhead:" symbol because they fluctuate in size, and would be a source of
  # noise in symbol diffs if included as symbols (http://crbug.com/1130754).
  # Might be even better if we had an option in Tiger Viewer to ignore certain
  # symbols, but taking this as a short-cut for now.
  apk_metadata = {
      models.METADATA_ZIPALIGN_OVERHEAD: zipalign_total,
      models.METADATA_SIGNING_BLOCK_SIZE: signing_block_size,
  }

  apk_metrics_by_file = {}
  apk_metrics_by_file[posixpath.basename(apk_spec.apk_path)] = {
      f'{models.METRICS_SIZE}/{models.METRICS_SIZE_APK_FILE}':
      os.path.getsize(apk_spec.apk_path),
  }

  # Overhead includes:
  #  * Size of all local zip headers (minus zipalign padding).
  #  * Size of central directory & end of central directory.
  overhead_size = (os.path.getsize(apk_spec.apk_path) - zip_info_total -
                   zipalign_total - signing_block_size)
  assert overhead_size >= 0, 'Apk overhead must be non-negative'
  zip_overhead_symbol = models.Symbol(models.SECTION_OTHER,
                                      overhead_size,
                                      full_name='Overhead: APK file')
  raw_symbols.append(zip_overhead_symbol)

  section_ranges = {}
  archive_util.ExtendSectionRange(section_ranges, models.SECTION_OTHER,
                                  sum(s.size for s in raw_symbols))
  file_format.SortSymbols(raw_symbols)
  return section_ranges, raw_symbols, apk_metadata, apk_metrics_by_file
