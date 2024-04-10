# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs apkanalyzer to parse dex files in an apk.

Assumes that apk_path.mapping and apk_path.jar.info is available.
"""

import collections
import functools
import itertools
import logging
import os
import posixpath
import re
import subprocess
import zipfile

import archive_util
import dalvik_bytecode
import dex_parser
import models
import path_util
import parallel
import string_extract

_TOTAL_NODE_NAME = '<TOTAL>'

# A limit on the number of symbols a DEX string literal can have, before these
# symbols are compacted into shared symbols. Increasing this value causes more
# data to be stored .size files, but is also more expensive.
# Effect as of Nov 2022 (run on TrichromeGoogle.ssargs with --java-only):
# 1: shared syms = 117811 bytes, file size = 3385635 (33630 syms).
# 2: shared syms = 39689 bytes, file size = 3408845 (36843 syms).
# 3: shared syms = 17831 bytes, file size = 3419021 (38553 syms).
# 5: shared syms = 6874 bytes, file size = 3425173 (40097 syms).
# 6: shared syms = 5098 bytes, file size = 3427458 (40597 syms).
# 8: shared syms = 3370 bytes, file size = 3429819 (41208 syms).
# 10: shared syms = 2250 bytes, file size = 3431944 (41720 syms).
# 20: shared syms = 587 bytes, file size = 3435466 (42983 syms).
# 40: shared syms = 204 bytes, file size = 3439084 (43909 syms).
# max: shared syms = 0 bytes, file size = 3446275 (46315 syms).
# Going with 6, i.e., strings literals with > 6 aliases are combined into a
# shared symbol. So 46315 - 40597 = 5718, or ~12% of original syms are removed,
# at the cost leaving ~5100 byte in binary sizes unresolved into aliases).
_DEX_STRING_MAX_SAME_NAME_ALIAS_COUNT = 6

# Synthetics that map 1:1 with the class they are a suffix on.
_CLASS_SPECIFIC_SYNTHETICS = (
    'ExternalSyntheticLambda',
    'ExternalSyntheticApiModelOutline',
    'ExternalSyntheticServiceLoad',
    'Lambda',
)


def _ParseJarInfoFile(file_name):
  with open(file_name, 'r') as info:
    source_map = dict()
    for line in info:
      package_path, file_path = line.strip().split(',', 1)
      source_map[package_path] = file_path
  return source_map


def RunApkAnalyzerAsync(apk_path, mapping_path):
  """Starts an apkanalyzer job for the given apk.

  Args:
    apk_path: Path to the apk to run on.
    mapping_path: Path to the proguard mapping file.

  Returns:
    An object to pass to CreateDexSymbols().
  """
  args = [path_util.GetApkAnalyzerPath(), 'dex', 'packages', apk_path]
  if mapping_path and os.path.exists(mapping_path):
    args.extend(['--proguard-mappings', mapping_path])
  env = os.environ.copy()
  env['JAVA_HOME'] = path_util.GetJavaHome()

  # Use a thread rather than directly using a Popen instance so that stdout is
  # being read from.
  return parallel.CallOnThread(subprocess.run,
                               args,
                               env=env,
                               encoding='utf-8',
                               capture_output=True,
                               check=True)


def _ParseApkAnalyzerOutput(stdout, stderr):
  stderr = re.sub(r'Successfully loaded.*?\n', '', stderr)
  if stderr.strip():
    raise Exception('Unexpected stderr:\n' + stderr)
  data = []
  for line in stdout.splitlines():
    try:
      vals = line.split()
      # We want to name these columns so we know exactly which is which.
      # pylint: disable=unused-variable
      node_type, state, defined_methods, referenced_methods, size, name = (
          vals[0], vals[1], vals[2], vals[3], vals[4], vals[5:])
      data.append((node_type, ' '.join(name), int(size)))
    except Exception:
      logging.error('Problem line was: %s', line)
      raise
  return data


# VisibleForTesting
def UndoHierarchicalSizing(data):
  """Subtracts child node sizes from parent nodes.

  Note that inner classes
  should be considered as siblings rather than child nodes.

  Example nodes:
    [
      ('P', '<TOTAL>', 37),
      ('P', 'org', 32),
      ('P', 'org.chromium', 32),
      ('C', 'org.chromium.ClassA', 14),
      ('M', 'org.chromium.ClassA void methodA()', 10),
      ('C', 'org.chromium.ClassA$Proxy', 8),
    ]

  Processed nodes:
    [
      ('<TOTAL>', 15),
      ('org.chromium.ClassA', 4),
      ('org.chromium.ClassA void methodA()', 10),
      ('org.chromium.ClassA$Proxy', 8),
    ]
  """
  num_nodes = len(data)
  nodes = []

  def process_node(start_idx):
    assert start_idx < num_nodes, 'Attempting to parse beyond data array.'
    node_type, name, size = data[start_idx]
    total_child_size = 0
    next_idx = start_idx + 1
    name_len = len(name)
    while next_idx < num_nodes:
      next_name = data[next_idx][1]
      if name == _TOTAL_NODE_NAME or (
          len(next_name) > name_len and next_name.startswith(name)
          and next_name[name_len] in '. '):
        # Child node
        child_next_idx, child_node_size = process_node(next_idx)
        next_idx = child_next_idx
        total_child_size += child_node_size
      else:
        # Sibling or higher nodes
        break

    # Apkanalyzer may overcount private method sizes at times. Unfortunately
    # the fix is not in the version we have in Android SDK Tools. For now we
    # prefer to undercount child sizes since the parent's size is more
    # accurate. This means the sum of child nodes may exceed its immediate
    # parent node's size.
    total_child_size = min(size, total_child_size)
    # TODO(wnwen): Add assert back once dexlib2 2.2.5 is released and rolled.
    #assert total_child_size <= size, (
    #    'Child node total size exceeded parent node total size')

    node_size = size - total_child_size
    # It is valid to have a package and a class with the same name.
    # To avoid having two symbols with the same name in these cases, do not
    # create symbols for packages (which have no size anyways).
    if node_type == 'P' and node_size != 0 and name != _TOTAL_NODE_NAME:
      logging.warning('Unexpected java package that takes up size: %d, %s',
                      node_size, name)
    if node_type != 'P' or node_size != 0:
      nodes.append((node_type, name, node_size))
    return next_idx, size

  idx = 0
  while idx < num_nodes:
    idx = process_node(idx)[0]
  return nodes


def _TruncateFrom(value, delimiter, rfind=False):
  idx = value.rfind(delimiter) if rfind else value.find(delimiter)
  if idx != -1:
    return value[:idx]
  return value


def _NormalizeName(orig_name):
  """Extracts outer class name and normalizes names with hashes in them.

  Returns:
    outer_class: The outer class. Example: package.Class
      Returns None for classes that are outlines.
    new_name: Normalized name.
  """
  # May be reassigned by one of the cases below.
  outer_class = _TruncateFrom(orig_name, '$')

  # $$ is the convention for a synthetic class and all known desugared lambda
  synthetic_marker_idx = orig_name.find('$$')
  if synthetic_marker_idx == -1:
    return outer_class, orig_name

  synthetic_part = orig_name[synthetic_marker_idx + 2:]

  # Example: package.Cls$$InternalSyntheticLambda$0$81073ff6$0
  if synthetic_part.startswith('InternalSyntheticLambda$'):
    next_dollar_idx = orig_name.index('$',
        synthetic_marker_idx + len('$$InternalSyntheticLambda$'))
    return outer_class, orig_name[:next_dollar_idx]

  # Ensure we notice if a new type of InternalSythetic pops up.
  # E.g. to see if it follows the same naming scheme.
  assert not synthetic_part.startswith('Internal'), f'Unrecognized: {orig_name}'

  if synthetic_part.startswith(_CLASS_SPECIFIC_SYNTHETICS):
    return outer_class, orig_name

  return None, orig_name


def NormalizeLine(orig_name, full_name):
  """Normalizes a line from apkanalyzer output.
  Args:
    orig_name: The original name from apkanalyzer output.
    full_name: The full name of the symbol.
  Returns:
    outer_class: The outer class. Example: package.Class
      Returns None for classes that are outlines.
    new_full_name: Normalized full name.
  """
  # See tests for a more comprehensive list of what d8 currently generates.
  outer_class, new_name = _NormalizeName(orig_name)
  if new_name is not orig_name:
    full_name = full_name.replace(orig_name, new_name)
  return outer_class, full_name


def _MakeDexObjectPath(package_name, is_outlined):
  if is_outlined:
    # Create a special meta-directory for outlined lambdas to easily monitor
    # their total size and spot regressions.
    return posixpath.join(models.OUTLINES_PREFIX_PATH, *package_name.split('.'))
  return posixpath.join(models.APK_PREFIX_PATH, *package_name.split('.'))


# Visible for testing.
def CreateDexSymbol(name, size, source_map):
  parts = name.split(' ')  # (class_name, return_type, method_name)
  new_package = parts[0]

  if new_package == _TOTAL_NODE_NAME:
    return None

  outer_class, name = NormalizeLine(new_package, name)

  # Look for class merging.
  old_package = new_package
  # len(parts) == 2 for class nodes.
  if len(parts) > 2:
    method = parts[2]
    # last_idx == -1 for fields, which is fine.
    last_idx = method.find('(')
    last_idx = method.rfind('.', 0, last_idx)
    if last_idx != -1:
      old_package = method[:last_idx]

      # TODO(b/333617478): Delete this work-around when R8 mapping files no
      #     longer output this pattern.
      suspect_class_name = old_package
      if suspect_class_name.startswith('WV.'):
        suspect_class_name = suspect_class_name[3:]
      if ('.' not in suspect_class_name
          and new_package.endswith(f'.{suspect_class_name}')):
        name = name.replace(f' {old_package}.', ' ')
        old_package = new_package
      else:
        # Non-workaround case:
        outer_class, name = NormalizeLine(old_package, name)

  is_outlined = outer_class == None
  object_path = _MakeDexObjectPath(old_package, is_outlined)
  if name.endswith(')'):
    section_name = models.SECTION_DEX_METHOD
  else:
    section_name = models.SECTION_DEX

  source_path = source_map.get(outer_class, '')
  return models.Symbol(section_name,
                       size,
                       full_name=name,
                       object_path=object_path,
                       source_path=source_path)


def _SymbolsFromNodes(nodes, source_map):
  # Use (DEX_METHODS, DEX) buckets to speed up sorting.
  symbol_buckets = ([], [])
  for _, name, node_size in nodes:
    symbol = CreateDexSymbol(name, node_size, source_map)
    if symbol:
      bucket_index = int(symbol.section_name is models.SECTION_DEX)
      symbol_buckets[bucket_index].append(symbol)
  for symbols_bucket in symbol_buckets:
    symbols_bucket.sort(key=lambda s: s.full_name)
  return symbol_buckets

def _GenDexStringsUsedByClasses(dexfile, class_deobfuscation_map):
  """Emit strings used in code_items and associate them with classes.

  Args:
    dexfile: A DexFile instance.
    class_deobfuscation_map: Map from obfuscated names to class names.

  Yields:
    string_idx: DEX string index.
    size: Number of bytes taken by string, including pointer.
    decoded_string: The decoded string.
    class_names: List of class names
  """
  if not dexfile or not dexfile.code_item_list:
    return

  # Helper to deobfuscate class names while converting 'LFoo;' -> 'Foo'.
  num_bad_name = 0
  num_deobfus_names = 0
  num_failed_deobfus = 0

  @functools.lru_cache(None)
  def LookupDeobfuscatedClassNames(class_def_idx):
    nonlocal num_bad_name, num_deobfus_names, num_failed_deobfus
    class_def_item = dexfile.class_def_item_list[class_def_idx]
    name = dexfile.GetTypeString(class_def_item.class_idx)
    if not (name.startswith('L') and name.endswith(';')):
      num_bad_name += 1
      return name
    # Change "L{X};" to "{X}", and convert path name to class name.
    name = name[1:-1].replace('/', '.')
    deobfuscated_name = class_deobfuscation_map.get(name, None)
    if deobfuscated_name is not None:
      name = deobfuscated_name
      num_deobfus_names += 1
    else:
      num_failed_deobfus += 1
    return name

  # Precompute map from code item offsets to set of string id used.
  code_off_to_used_string_ids = {
      code_item.offset: set(dexfile.IterAllStringIdsUsedByCodeItem(code_item))
      for code_item in dexfile.code_item_list
  }
  code_off_to_used_string_ids[0] = set()  # Offset 0 = No code.

  # Walk code for each class, each methods, mark string usages.
  string_idx_to_class_idxs = collections.defaultdict(set)
  for i, class_item in enumerate(dexfile.class_def_item_list):
    string_idxs_used_by_class = set()
    class_data_item = dexfile.GetClassDataItemByOffset(
        class_item.class_data_off)
    if class_data_item:
      for encoded_method in itertools.chain(class_data_item.direct_methods,
                                            class_data_item.virtual_methods):
        code_off = encoded_method.code_off
        string_idxs_used_by_class |= code_off_to_used_string_ids[code_off]
    for string_idx in string_idxs_used_by_class:
      string_idx_to_class_idxs[string_idx].add(i)

  # Emit each string used by code, with names of classes that use it. Both are
  # sorted to maintain consitency.
  for string_idx in sorted(string_idx_to_class_idxs):
    string_item = dexfile.string_data_item_list[string_idx]
    size = string_item.byte_size + 4  # +4 for pointer.
    decoded_string = string_item.data
    class_idxs = string_idx_to_class_idxs[string_idx]
    class_names = sorted(LookupDeobfuscatedClassNames(i) for i in class_idxs)
    yield string_idx, size, decoded_string, class_names

  logging.info('Deobfuscated %d / %d classes (%d failures)', num_deobfus_names,
               len(dexfile.class_def_item_list), num_failed_deobfus)
  if num_bad_name > 0:
    logging.warn('Found %d class names not formatted as "L.*;".' % num_bad_name)


def _MakeFakeSourcePath(class_name):
  class_path = class_name.replace('.', '/')
  return f'{models.APK_PREFIX_PATH}/{class_path}'


def _StringSymbolsFromDexFile(apk_path, dexfile, source_map,
                              class_deobfuscation_map):
  if not dexfile:
    return []
  logging.info('Extractng string symbols from %s', apk_path)

  # Code strings: Strings accessed via class -> method -> code -> string.
  # These are extracted into separate symbols ,aliases among referring classes.
  fresh_string_idx_set = set(range(len(dexfile.string_data_item_list)))
  object_path = str(apk_path)
  dex_string_symbols = []
  string_iter = _GenDexStringsUsedByClasses(dexfile, class_deobfuscation_map)
  for string_idx, size, decoded_string, string_user_class_names in string_iter:
    fresh_string_idx_set.remove(string_idx)
    num_aliases = len(string_user_class_names)
    aliases = []
    for class_name in string_user_class_names:
      outer_class, class_name = _NormalizeName(class_name)
      full_name = string_extract.GetNameOfStringLiteralBytes(
          decoded_string.encode('utf-8', errors='surrogatepass'))
      source_path = (source_map.get(outer_class, '')
                     or _MakeFakeSourcePath(class_name))
      sym = models.Symbol(models.SECTION_DEX,
                          size,
                          full_name=full_name,
                          object_path=object_path,
                          source_path=source_path,
                          aliases=aliases if num_aliases > 1 else None)
      aliases.append(sym)
    assert num_aliases == len(aliases)
    dex_string_symbols += aliases

  logging.info('Counted %d class -> method -> code strings',
               len(dexfile.string_data_item_list) - len(fresh_string_idx_set))

  # Extract aggregate string symbols for {types, methods, fields, prototypes}.
  # Due to significant overlap (coincidental or induced by R8), {method, field}
  # string symbols share a common aggregate. Other overlap sare resolved by
  # applying the priority:
  #   code > type > {method, field} > prototype,
  # i.e., bytes from code strings are not counted in aggregates; bytes from type
  # string aggregate are not counted by {{method, field}, prototype}, etc.

  def _AddAggregateStringSymbol(name, string_idx_set):
    nonlocal fresh_string_idx_set
    old_count = len(string_idx_set)
    string_idx_set &= fresh_string_idx_set
    fresh_string_idx_set -= string_idx_set
    logging.info('Counted %d %s strings among %d found', len(string_idx_set),
                 name, old_count)
    if string_idx_set:
      # Each string has +4 for pointer.
      size = sum(dexfile.string_data_item_list[string_idx].byte_size
                 for string_idx in string_idx_set) + 4 * len(string_idx_set)
      sym = models.Symbol(models.SECTION_DEX,
                          size,
                          full_name=f'** .dex ({name} strings)')
      dex_string_symbols.append(sym)

  # Type strings.
  type_string_idx_set = {i.descriptor_idx for i in dexfile.type_id_item_list}
  _AddAggregateStringSymbol('type', type_string_idx_set)

  # Method and field strings.
  method_string_idx_set = {i.name_idx for i in dexfile.method_id_item_list}
  field_string_idx_set = {i.name_idx for i in dexfile.field_id_item_list}
  _AddAggregateStringSymbol('method and field',
                            method_string_idx_set | field_string_idx_set)

  # Prototype strings.
  proto_string_idx_set = {i.shorty_idx for i in dexfile.proto_id_item_list}
  _AddAggregateStringSymbol('prototype', proto_string_idx_set)

  return dex_string_symbols


def _ParseDexfilesInApk(apk_path):
  with zipfile.ZipFile(apk_path) as src_zip:
    dex_infos = [
        info for info in src_zip.infolist() if
        info.filename.startswith('classes') and info.filename.endswith('.dex')
    ]
    # Assume sound and stable ordering of DEX filenames.
    for dex_info in dex_infos:
      dex_data = src_zip.read(dex_info)
      yield dex_info.filename, dex_parser.DexFile(dex_data)


def CreateDexSymbols(apk_path, apk_analyzer_async_result, dex_total_size,
                     class_deobfuscation_map, size_info_prefix,
                     track_string_literals):
  """Creates DEX symbols given apk_analyzer output.

  Args:
    apk_path: Path to the APK containing the DEX file.
    apk_analyzer_async_result: Return value from RunApkAnalyzerAsync().
    dex_total_size: Sum of the sizes of all .dex files in the apk.
    class_deobfuscation_map: Map from obfuscated names to class names.
    size_info_prefix: Path such as: out/Release/size-info/BaseName.
    track_string_literals: Create symbols for string literals.

  Returns:
    A tuple of (section_ranges, raw_symbols, metrics_by_file), where
    metrics_by_file is a dict from DEX file name to a dict of
    {metric_name: value}.
  """
  logging.debug('Waiting for apkanalyzer to finish')
  apk_analyzer_result = apk_analyzer_async_result.get()
  logging.debug('Analyzing DEX - processing results')
  if size_info_prefix:
    source_map = _ParseJarInfoFile(size_info_prefix + '.jar.info')
  else:
    source_map = dict()

  nodes = _ParseApkAnalyzerOutput(apk_analyzer_result.stdout,
                                  apk_analyzer_result.stderr)
  nodes = UndoHierarchicalSizing(nodes)

  total_node_size = sum([x[2] for x in nodes])
  # TODO(agrieve): Figure out why this log is triggering for
  #     ChromeModernPublic.apk (https://crbug.com/851535).
  # Reporting: dex_total_size=6546088 total_node_size=6559549
  if dex_total_size < total_node_size:
    logging.error(
        'Node size too large, check for node processing errors. '
        'dex_total_size=%d total_node_size=%d', dex_total_size, total_node_size)

  dex_method_symbols, dex_other_symbols = _SymbolsFromNodes(nodes, source_map)
  dex_string_symbols = []
  metrics_by_file = {}
  for dex_path, dexfile in _ParseDexfilesInApk(apk_path):
    logging.debug('Found DEX: %r', dex_path)
    if track_string_literals:
      dex_string_symbols += _StringSymbolsFromDexFile(apk_path, dexfile,
                                                      source_map,
                                                      class_deobfuscation_map)
    map_item_sizes = dexfile.ComputeMapItemSizes()
    metrics = {}
    for item in map_item_sizes:
      metrics[f'{models.METRICS_SIZE}/' + item['name']] = item['byte_size']
      metrics[f'{models.METRICS_COUNT}/' + item['name']] = item['size']
    metrics_by_file[dex_path] = metrics

  if dex_string_symbols:
    logging.info('Converting excessive DEX string aliases into shared-path '
                 'symbols')
    archive_util.CompactLargeAliasesIntoSharedSymbols(
        dex_string_symbols, _DEX_STRING_MAX_SAME_NAME_ALIAS_COUNT)

  dex_method_size = round(sum(s.pss for s in dex_method_symbols))
  dex_other_size = round(sum(s.pss for s in dex_other_symbols))
  dex_other_size += round(sum(s.pss for s in dex_string_symbols))
  unattributed_dex = dex_total_size - dex_method_size - dex_other_size
  # Compare against -5 instead of 0 to guard against round-off errors.
  assert unattributed_dex >= -5, (
      'sum(dex_symbols.size) > sum(filesize(dex file)). {} vs {}'.format(
          dex_method_size + dex_other_size, dex_total_size))

  if unattributed_dex > 0:
    dex_other_symbols.append(
        models.Symbol(
            models.SECTION_DEX,
            unattributed_dex,
            full_name='** .dex (unattributed)'))

  dex_other_symbols.extend(dex_method_symbols)
  dex_other_symbols.extend(dex_string_symbols)

  # We can't meaningfully track section size of dex methods vs other, so
  # just fake the size of dex methods as the sum of symbols, and make
  # "dex other" responsible for any unattributed bytes.
  section_ranges = {
      models.SECTION_DEX_METHOD: (0, dex_method_size),
      models.SECTION_DEX: (0, dex_total_size - dex_method_size),
  }

  return section_ranges, dex_other_symbols, metrics_by_file
