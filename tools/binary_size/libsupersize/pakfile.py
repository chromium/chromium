# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Size analysis of .pak files."""

import collections
import logging
import os
import posixpath
import sys
import zipfile
import zlib

import archive_util
import file_format
import models
import path_util

sys.path.insert(1, path_util.FromToolsSrcRoot('tools', 'grit'))
from grit.format import data_pack

_UNCOMPRESSED_COMPRESSION_RATIO_THRESHOLD = 0.9


class PakIdMap:
  def __init__(self):
    self._dict = collections.defaultdict(set)

  def Update(self, object_paths_by_name, ninja_source_mapper):
    # IDS_ macro usages result in templated function calls that contain the
    # resource ID in them. These names are collected along with all other
    # symbols by running "nm" on them. We just need to extract the values.
    PREFIX = 'void ui::AllowlistedResource<'
    id_start_idx = len(PREFIX)
    id_end_idx = -len('>()')
    for name in object_paths_by_name:
      if name.startswith(PREFIX):
        pak_id = int(name[id_start_idx:id_end_idx])
        object_paths = object_paths_by_name[name]
        self._dict[pak_id].update(
            (o, ninja_source_mapper.FindSourceForPath(o)) for o in object_paths)

  def Lookup(self, pak_id):
    ret = self._dict.get(pak_id)
    if ret:
      ret = sorted(ret)
    return ret


def _IsPakContentUncompressed(content):
  raw_size = len(content)
  # Assume anything less than 100 bytes cannot be compressed.
  if raw_size < 100:
    return False

  compressed_size = len(zlib.compress(content, 1))
  compression_ratio = compressed_size / float(raw_size)
  return compression_ratio < _UNCOMPRESSED_COMPRESSION_RATIO_THRESHOLD


def _ParsePakInfoFile(pak_info_path):
  with open(pak_info_path, 'r') as info_file:
    res_info = {}
    for line in info_file.readlines():
      name, res_id, path = line.split(',')
      res_info[int(res_id)] = (name, path.strip())
  return res_info


def _CreateSymbolsFromFile(file_name, contents, res_info, symbols_by_id):
  # Reversed so that aliases are clobbered by the entries they are aliases of.
  id_map = {id(v): k for k, v in reversed(contents.resources.items())}
  alias_map = {
      k: id_map[id(v)]
      for k, v in contents.resources.items() if id_map[id(v)] != k
  }
  name = posixpath.basename(file_name)
  # Hyphens used for language regions. E.g.: en-GB.pak, sr-Latn.pak, ...
  # Longest translated .pak file without hyphen: fil.pak
  if '-' in name or len(name) <= 7:
    section_name = models.SECTION_PAK_TRANSLATIONS
  else:
    # E.g.: resources.pak, chrome_100_percent.pak.
    section_name = models.SECTION_PAK_NONTRANSLATED
  overhead = 12 + 6  # Header size plus extra offset
  # Key just needs to be unique from other IDs and pak overhead symbols.
  symbols_by_id[-len(symbols_by_id) - 1] = models.Symbol(
      section_name, overhead, full_name='Overhead: {}'.format(file_name))
  for pak_id in sorted(contents.resources):
    aliased_pak_id = alias_map.get(pak_id)
    if aliased_pak_id is not None:
      # 4 extra bytes of metadata (2 16-bit ints)
      size = 4
      pak_id = aliased_pak_id
    else:
      resource_data = contents.resources[pak_id]
      # 6 extra bytes of metadata (1 32-bit int, 1 16-bit int)
      size = len(resource_data) + 6
      name, source_path = res_info[pak_id]
      if pak_id not in symbols_by_id:
        full_name = '{}: {}'.format(source_path, name)
        new_symbol = models.Symbol(section_name,
                                   0,
                                   address=pak_id,
                                   full_name=full_name)
        if (section_name == models.SECTION_PAK_NONTRANSLATED
            and _IsPakContentUncompressed(resource_data)):
          new_symbol.flags |= models.FLAG_UNCOMPRESSED
        symbols_by_id[pak_id] = new_symbol

    symbols_by_id[pak_id].size += size
  return section_name


def _FinalizeSymbols(symbols_by_id, pak_id_map):
  """Converts dict -> list, adds paths, and adds aliases."""
  raw_symbols = []
  for pak_id, symbol in symbols_by_id.items():
    raw_symbols.append(symbol)
    path_tuples = pak_id_map.Lookup(pak_id)
    if not path_tuples:
      continue
    symbol.object_path, symbol.source_path = path_tuples[0]
    if len(path_tuples) == 1:
      continue
    aliases = symbol.aliases or [symbol]
    symbol.aliases = aliases
    for object_path, source_path in path_tuples[1:]:
      new_sym = models.Symbol(symbol.section_name,
                              symbol.size,
                              address=symbol.address,
                              full_name=symbol.full_name,
                              object_path=object_path,
                              source_path=source_path,
                              aliases=aliases)
      aliases.append(new_sym)
      raw_symbols.append(new_sym)

  # Pre-sort to make final sort faster.
  file_format.SortSymbols(raw_symbols)
  return raw_symbols


def CreatePakSymbolsFromApk(section_ranges, apk_path, apk_pak_paths,
                            size_info_prefix, pak_id_map):
  """Uses files in apk to find and add pak symbols."""
  with zipfile.ZipFile(apk_path) as z:
    pak_zip_infos = [z.getinfo(p) for p in apk_pak_paths]
    pak_info_path = size_info_prefix + '.pak.info'
    res_info = _ParsePakInfoFile(pak_info_path)
    symbols_by_id = {}
    for zip_info in pak_zip_infos:
      contents = data_pack.ReadDataPackFromString(z.read(zip_info))
      if zip_info.compress_type != zipfile.ZIP_STORED:
        logging.warning(
            'Expected .pak files to be STORED, but this one is compressed: %s',
            zip_info.filename)
      path = archive_util.RemoveAssetSuffix(zip_info.filename)
      section_name = _CreateSymbolsFromFile(path, contents, res_info,
                                            symbols_by_id)
      archive_util.ExtendSectionRange(section_ranges, section_name,
                                      zip_info.compress_size)
  return _FinalizeSymbols(symbols_by_id, pak_id_map)


def CreatePakSymbolsFromFiles(section_ranges, pak_paths, pak_info_path,
                              output_directory, pak_id_map):
  """Uses files from --pak-file args to find and add pak symbols."""
  if pak_info_path:
    res_info = _ParsePakInfoFile(pak_info_path)
  symbols_by_id = {}
  for pak_path in pak_paths:
    if not pak_info_path:
      res_info = _ParsePakInfoFile(pak_path + '.info')
    with open(pak_path, 'rb') as f:
      contents = data_pack.ReadDataPackFromString(f.read())
    section_name = _CreateSymbolsFromFile(
        os.path.relpath(pak_path, output_directory), contents, res_info,
        symbols_by_id)
    archive_util.ExtendSectionRange(section_ranges, section_name,
                                    os.path.getsize(pak_path))
  return _FinalizeSymbols(symbols_by_id, pak_id_map)
