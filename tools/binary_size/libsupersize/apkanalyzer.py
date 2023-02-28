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
_OUTLINED_PREFIX = '$$Outlined$'

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
                               encoding='utf8',
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


# Visible for testing.
class LambdaNormalizer:
  def __init__(self):
    self._lambda_by_class_counter = collections.defaultdict(int)
    self._lambda_name_to_nested_number = {}

  def _GetLambdaName(self, class_path, base_name, prefix=''):
    lambda_number = self._lambda_name_to_nested_number.get(class_path)
    if lambda_number is None:
      # First time we've seen this lambda, increment nested class count.
      lambda_number = self._lambda_by_class_counter[base_name]
      self._lambda_name_to_nested_number[class_path] = lambda_number
      self._lambda_by_class_counter[base_name] = lambda_number + 1
    return prefix + base_name + '$$Lambda$' + str(lambda_number)


  def ExtractOuterClassAndDesugarLambda(self, class_path):
    """Extracts outer class name and converts Lambda to Desugar format.

    Args:
      class_path: Class path as d8 desugared name, possibly including inner
      classes and Lambda parts. Example:
      package.Class$$Lambda$2$$InternalSyntheticOutline$8$cbe941dd782$0

    Returns:
      outer_class: The outer class. Example: package.Class
      new_name: |class_path| converted to Desugar format, or None if it is not a
        Lambda. Example: package.Class$$Lambda$0
    """
    # May be reassigned by one of the cases below.
    outer_class = _TruncateFrom(class_path, '$')

    # $$ is the convention for a synthetic class and all known desugared lambda
    # classes have 'Lambda' in the synthetic part of its name. If it doesn't
    # then it's almost certainly not a desugared lambda class.
    if 'Lambda' not in class_path[class_path.find('$$'):]:
      return outer_class, None

    # Example: package.AnimatedProgressBar$$InternalSyntheticLambda$3$81073ff6$0
    # Example: package.Class$$Lambda$2$$InternalSyntheticOutline$8$cbe941dd782$0
    match = re.fullmatch(
        # The base_name group needs to be non-greedy/minimal (using +?) since we
        # want it to not include $$Lambda$28 when present.
        r'(?P<base_name>.+?)(\$\$Lambda\$\d+)?'
        r'\$\$InternalSynthetic[a-zA-Z0-9_]+'
        r'\$\d+\$[0-9a-f]+\$\d+',
        class_path)
    if match:
      new_name = self._GetLambdaName(class_path=class_path,
                                     base_name=match.group('base_name'))
      return outer_class, new_name
    # Example: AnimatedProgressBar$$ExternalSyntheticLambda0
    # Example: AutofillAssistant$$Lambda$2$$ExternalSyntheticOutline0
    # Example: ContextMenuCoord$$Lambda$2$$ExternalSyntheticThrowCCEIfNotNull0
    match = re.fullmatch(
        r'(?P<base_name>.+?)(\$\$Lambda\$\d+)?'
        r'\$\$ExternalSynthetic[a-zA-Z0-9_]+', class_path)
    if match:
      new_name = self._GetLambdaName(class_path=class_path,
                                     base_name=match.group('base_name'),
                                     prefix=_OUTLINED_PREFIX)
      return outer_class, new_name
    # Example: package.FirebaseInstallationsRegistrar$$Lambda$1
    match = re.fullmatch(r'(?P<base_name>.+)\$\$Lambda\$\d+', class_path)
    if match:
      # Although these are already valid names, re-number them to avoid name
      # collisions with renamed InternalSyntheticLambdas.
      new_name = self._GetLambdaName(class_path=class_path,
                                     base_name=match.group('base_name'))
      return outer_class, new_name
    # Example: org.-$$Lambda$StackAnimation$Nested1$kjevdDQ8V2zqCrdieLqWLHzk
    # Assume that the last portion of the name after $ is the hash identifier.
    match = re.fullmatch(
        r'(?P<package>.+)-\$\$Lambda\$(?P<class>[^$]+)(?P<nested>.*)\$[^$]+',
        class_path)
    if match:
      package_name = match.group('package')
      class_name = match.group('class')
      nested_classes = match.group('nested')
      base_name = package_name + class_name + nested_classes
      new_name = self._GetLambdaName(class_path=class_path, base_name=base_name)
      outer_class = package_name + class_name
      return outer_class, new_name
    assert False, (
        'No valid match for new lambda name format: ' + class_path + '\n'
        'Please update https://crbug.com/1208385 with this error so we can '
        'update the lambda normalization code.')
    return None

  def Normalize(self, class_path, full_name):
    """Make d8 desugared lambdas look the same as Desugar ones."""
    # Desugar lambda: org.Promise$Nested1$$Lambda$0
    # 1) Need to prefix with proper class name so that they will show as nested.
    # 2) Need to suffix with number so that they diff better.
    # Original name will be kept as "object_path".
    # See tests for a more comprehensive list of what d8 currently generates.
    outer_class, new_name = self.ExtractOuterClassAndDesugarLambda(class_path)
    if new_name:
      full_name = full_name.replace(class_path, new_name)
    return outer_class, full_name


def _MakeDexObjectPath(package_name, is_outlined):
  if is_outlined:
    # Create a special meta-directory for outlined lambdas to easily monitor
    # their total size and spot regressions.
    return posixpath.join(models.APK_PREFIX_PATH, 'Outlined',
                          *package_name.split('.'))
  return posixpath.join(models.APK_PREFIX_PATH, *package_name.split('.'))


# Visible for testing.
def CreateDexSymbol(name, size, source_map, lambda_normalizer):
  parts = name.split(' ')  # (class_name, return_type, method_name)
  new_package = parts[0]

  if new_package == _TOTAL_NODE_NAME:
    return None

  # Make d8 desugared lambdas look the same as Desugar ones.
  outer_class, name = lambda_normalizer.Normalize(new_package, name)

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
      outer_class, name = lambda_normalizer.Normalize(old_package, name)

  source_path = source_map.get(outer_class, '')
  object_path = _MakeDexObjectPath(old_package,
                                   name.startswith(_OUTLINED_PREFIX))
  if name.endswith(')'):
    section_name = models.SECTION_DEX_METHOD
  else:
    section_name = models.SECTION_DEX

  return models.Symbol(section_name,
                       size,
                       full_name=name,
                       object_path=object_path,
                       source_path=source_path)


def _SymbolsFromNodes(nodes, source_map):
  # Use (DEX_METHODS, DEX) buckets to speed up sorting.
  symbol_buckets = ([], [])
  lambda_normalizer = LambdaNormalizer()
  for _, name, node_size in nodes:
    symbol = CreateDexSymbol(name, node_size, source_map, lambda_normalizer)
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
    name = name[1:-1]
    deobfuscated_name = class_deobfuscation_map.get(name, None)
    if deobfuscated_name is not None:
      name = deobfuscated_name
      num_deobfus_names += 1
    elif '/' in name:
      # Has path: Assume not obfuscated, and convert to class name.
      name = name.replace('/', '.')
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
    return [], 0
  logging.info('Extractng string symbols from %s', apk_path)

  # Code strings: Strings accessed via class -> method -> code -> string.
  # These are extracted into separate symbols ,aliases among referring classes.
  fresh_string_idx_set = set(range(len(dexfile.string_data_item_list)))
  lambda_normalizer = LambdaNormalizer()
  object_path = str(apk_path)
  dex_string_data_size = 0
  dex_string_symbols = []
  string_iter = _GenDexStringsUsedByClasses(dexfile, class_deobfuscation_map)
  for string_idx, size, decoded_string, string_user_class_names in string_iter:
    fresh_string_idx_set.remove(string_idx)
    dex_string_data_size += size
    num_aliases = len(string_user_class_names)
    aliases = []
    for class_name in string_user_class_names:
      outer_class, _ = lambda_normalizer.ExtractOuterClassAndDesugarLambda(
          class_name)
      full_name = string_extract.GetNameOfStringLiteralBytes(
          decoded_string.encode('utf-8'))
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
    total_size = 0
    if string_idx_set:
      # Each string has +4 for pointer.
      size = sum(dexfile.string_data_item_list[string_idx].byte_size
                 for string_idx in string_idx_set) + 4 * len(string_idx_set)
      total_size += size
      sym = models.Symbol(models.SECTION_DEX,
                          size,
                          full_name=f'** .dex ({name} strings)')
      dex_string_symbols.append(sym)
    return total_size

  # Type strings.
  type_string_idx_set = {i.descriptor_idx for i in dexfile.type_id_item_list}
  dex_string_data_size += _AddAggregateStringSymbol('type', type_string_idx_set)

  # Method and field strings.
  method_string_idx_set = {i.name_idx for i in dexfile.method_id_item_list}
  field_string_idx_set = {i.name_idx for i in dexfile.field_id_item_list}
  dex_string_data_size += _AddAggregateStringSymbol(
      'method and field', method_string_idx_set | field_string_idx_set)

  # Prototype strings.
  proto_string_idx_set = {i.shorty_idx for i in dexfile.proto_id_item_list}
  dex_string_data_size += _AddAggregateStringSymbol('prototype',
                                                    proto_string_idx_set)

  return dex_string_symbols, dex_string_data_size


def _ParseMainDexfileInApk(apk_path):
  with zipfile.ZipFile(apk_path) as src_zip:
    dex_infos = [
        info for info in src_zip.infolist() if
        info.filename.startswith('classes') and info.filename.endswith('.dex')
    ]
    if len(dex_infos) != 0:
      if len(dex_infos) > 1:
        logging.warning(
            'Found multiple .dex files in %s: Only the first will be used.',
            apk_path)
      dex_data = src_zip.read(dex_infos[0])
      return dex_infos[0].filename, dex_parser.DexFile(dex_data)

  return None


def CreateDexSymbols(apk_path, apk_analyzer_async_result, dex_total_size,
                     class_deobfuscation_map, size_info_prefix):
  """Creates DEX symbols given apk_analyzer output.

  Args:
    apk_path: Path to the APK containing the DEX file.
    apk_analyzer_async_result: Return value from RunApkAnalyzerAsync().
    dex_total_size: Sum of the sizes of all .dex files in the apk.
    class_deobfuscation_map: Map from obfuscated names to class names.
    size_info_prefix: Path such as: out/Release/size-info/BaseName.

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

  dex_path, dexfile = _ParseMainDexfileInApk(apk_path)
  # TODO(huangs): Handle the case where an APK contains multiple DEX files.
  dex_string_symbols, dex_string_data_size = _StringSymbolsFromDexFile(
      apk_path, dexfile, source_map, class_deobfuscation_map)
  if dex_string_symbols:
    logging.info('Converting excessive DEX string aliases into shared-path '
                 'symbols')
    archive_util.CompactLargeAliasesIntoSharedSymbols(
        dex_string_symbols, _DEX_STRING_MAX_SAME_NAME_ALIAS_COUNT)

  dex_method_size = round(sum(s.pss for s in dex_method_symbols))
  dex_other_size = round(sum(s.pss for s in dex_other_symbols))

  unattributed_dex = (dex_total_size - dex_method_size - dex_other_size -
                      dex_string_data_size)
  # Compare against -5 instead of 0 to guard against round-off errors.
  assert unattributed_dex >= -5, (
      'sum(dex_symbols.size) > filesize(classes.dex). {} vs {}'.format(
          dex_method_size + dex_other_size, dex_total_size))

  if unattributed_dex > 0:
    dex_other_symbols.append(
        models.Symbol(
            models.SECTION_DEX,
            unattributed_dex,
            full_name='** .dex (unattributed)'))

  # We can't meaningfully track section size of dex methods vs other, so
  # just fake the size of dex methods as the sum of symbols, and make
  # "dex other" responsible for any unattributed bytes.
  section_ranges = {
      models.SECTION_DEX_METHOD: (0, dex_method_size),
      models.SECTION_DEX: (0, dex_total_size - dex_method_size),
  }

  dex_other_symbols.extend(dex_method_symbols)
  dex_other_symbols.extend(dex_string_symbols)

  map_item_sizes = dexfile.ComputeMapItemSizes()
  metrics = {}
  for item in map_item_sizes:
    metrics['SIZE/' + item['name']] = item['byte_size']
    metrics['COUNT/' + item['name']] = item['size']
  return section_ranges, dex_other_symbols, {dex_path: metrics}
