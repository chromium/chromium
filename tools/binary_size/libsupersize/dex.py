# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates symbols for dex files in an apk.

Dex files are parsed (and symbols names deobfuscated) in a background process.
Assumes that apk_path.mapping and apk_path.jar.info is available.
"""

import collections
import functools
import itertools
import logging
import posixpath
import zipfile

import archive_util
import dex_deobfuscate
import dex_parser
import models
import parallel
import string_extract

# Sizes of fixed-size dex items.
# https://source.android.com/docs/core/runtime/dex-format
_CLASS_DEF_ITEM_SIZE = 32
_TYPE_ID_ITEM_SIZE = 4
_METHOD_ID_ITEM_SIZE = 8
_FIELD_ID_ITEM_SIZE = 8
_ANNOTATIONS_DIRECTORY_HEADER_SIZE = 16
_FIELD_ANNOTATION_SIZE = 8
_NO_INDEX = 0xFFFFFFFF

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

  synthetic_part = orig_name[synthetic_marker_idx + 2 :]

  # Example: package.Cls$$InternalSyntheticLambda$0$81073ff6$0
  if synthetic_part.startswith('InternalSyntheticLambda$'):
    next_dollar_idx = orig_name.index(
      '$', synthetic_marker_idx + len('$$InternalSyntheticLambda$')
    )
    return outer_class, orig_name[:next_dollar_idx]

  # Ensure we notice if a new type of InternalSynthetic pops up.
  # E.g. to see if it follows the same naming scheme.
  assert not synthetic_part.startswith('Internal'), f'Unrecognized: {orig_name}'

  if synthetic_part.startswith(_CLASS_SPECIFIC_SYNTHETICS):
    return outer_class, orig_name

  return None, orig_name


def NormalizeLine(orig_name, full_name):
  """Normalizes a dex symbol name.
  Args:
    orig_name: The class name to normalize.
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
      outer_class, name = NormalizeLine(old_package, name)

  is_outlined = outer_class is None
  object_path = _MakeDexObjectPath(old_package, is_outlined)
  if name.endswith(')'):
    section_name = models.SECTION_DEX_METHOD
  else:
    section_name = models.SECTION_DEX

  source_path = source_map.get(outer_class, '')
  return models.Symbol(
    section_name,
    size,
    full_name=name,
    object_path=object_path,
    source_path=source_path,
  )


def _SymbolsFromNodes(nodes, source_map):
  # Use (DEX_METHODS, DEX) buckets to speed up sorting.
  symbol_buckets = ([], [])
  for name, node_size in nodes:
    symbol = CreateDexSymbol(name, node_size, source_map)
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
      class_item.class_data_off
    )
    if class_data_item:
      for encoded_method in itertools.chain(
        class_data_item.direct_methods, class_data_item.virtual_methods
      ):
        code_off = encoded_method.code_off
        string_idxs_used_by_class |= code_off_to_used_string_ids[code_off]
    for string_idx in string_idxs_used_by_class:
      string_idx_to_class_idxs[string_idx].add(i)

  # Emit each string used by code, with names of classes that use it. Both are
  # sorted to maintain consistency.
  for string_idx in sorted(string_idx_to_class_idxs):
    string_item = dexfile.string_data_item_list[string_idx]
    size = string_item.byte_size + 4  # +4 for pointer.
    decoded_string = string_item.data
    class_idxs = string_idx_to_class_idxs[string_idx]
    class_names = sorted(LookupDeobfuscatedClassNames(i) for i in class_idxs)
    yield string_idx, size, decoded_string, class_names

  logging.info(
    'Deobfuscated %d / %d classes (%d failures)',
    num_deobfus_names,
    len(dexfile.class_def_item_list),
    num_failed_deobfus,
  )
  if num_bad_name > 0:
    logging.warn('Found %d class names not formatted as "L.*;".' % num_bad_name)


def _MakeFakeSourcePath(class_name):
  class_path = class_name.replace('.', '/')
  return f'{models.APK_PREFIX_PATH}/{class_path}'


def _ExtractStringLiterals(dexfile, class_deobfuscation_map):
  """Finds string literals and aggregate string sizes within a dex file.

  Returns:
    A tuple of (string_literals, string_aggregates), where:
    * string_literals is a list of (size, full_name, class_names) for strings
      used by code.
    * string_aggregates is a list of (full_name, size) for strings not used by
      code.
  """
  # Code strings: Strings accessed via class -> method -> code -> string.
  # These are extracted into separate symbols ,aliases among referring classes.
  fresh_string_idx_set = set(range(len(dexfile.string_data_item_list)))
  string_literals = []
  string_iter = _GenDexStringsUsedByClasses(dexfile, class_deobfuscation_map)
  for string_idx, size, decoded_string, string_user_class_names in string_iter:
    fresh_string_idx_set.remove(string_idx)
    full_name = string_extract.GetNameOfStringLiteralBytes(
      decoded_string.encode('utf-8', errors='surrogatepass')
    )
    string_literals.append((size, full_name, string_user_class_names))

  logging.info(
    'Counted %d class -> method -> code strings',
    len(dexfile.string_data_item_list) - len(fresh_string_idx_set),
  )

  # Extract aggregate string symbols for {types, methods, fields, prototypes}.
  # Due to significant overlap (coincidental or induced by R8), {method, field}
  # string symbols share a common aggregate. Other overlap sare resolved by
  # applying the priority:
  #   code > type > {method, field} > prototype,
  # i.e., bytes from code strings are not counted in aggregates; bytes from type
  # string aggregate are not counted by {{method, field}, prototype}, etc.
  string_aggregates = []

  def _AddAggregateStringSymbol(name, string_idx_set):
    nonlocal fresh_string_idx_set
    old_count = len(string_idx_set)
    string_idx_set &= fresh_string_idx_set
    fresh_string_idx_set -= string_idx_set
    logging.info(
      'Counted %d %s strings among %d found',
      len(string_idx_set),
      name,
      old_count,
    )
    if string_idx_set:
      # Each string has +4 for pointer.
      size = sum(
        dexfile.string_data_item_list[string_idx].byte_size
        for string_idx in string_idx_set
      ) + 4 * len(string_idx_set)
      string_aggregates.append((f'** .dex ({name} strings)', size))

  # Type strings.
  type_string_idx_set = {i.descriptor_idx for i in dexfile.type_id_item_list}
  _AddAggregateStringSymbol('type', type_string_idx_set)

  # Method and field strings.
  method_string_idx_set = {i.name_idx for i in dexfile.method_id_item_list}
  field_string_idx_set = {i.name_idx for i in dexfile.field_id_item_list}
  _AddAggregateStringSymbol(
    'method and field', method_string_idx_set | field_string_idx_set
  )

  # Prototype strings.
  proto_string_idx_set = {i.shorty_idx for i in dexfile.proto_id_item_list}
  _AddAggregateStringSymbol('prototype', proto_string_idx_set)

  return string_literals, string_aggregates


def _CreateStringSymbols(
  apk_path, string_literals, string_aggregates, source_map
):
  logging.info('Creating string symbols for %s', apk_path)
  object_path = str(apk_path)
  dex_string_symbols = []
  for size, full_name, string_user_class_names in string_literals:
    num_aliases = len(string_user_class_names)
    aliases = []
    for class_name in string_user_class_names:
      outer_class, class_name = _NormalizeName(class_name)
      source_path = source_map.get(outer_class, '') or _MakeFakeSourcePath(
        class_name
      )
      sym = models.Symbol(
        models.SECTION_DEX,
        size,
        full_name=full_name,
        object_path=object_path,
        source_path=source_path,
        aliases=aliases if num_aliases > 1 else None,
      )
      aliases.append(sym)
    assert num_aliases == len(aliases)
    dex_string_symbols += aliases

  for full_name, size in string_aggregates:
    dex_string_symbols.append(
      models.Symbol(models.SECTION_DEX, size, full_name=full_name)
    )

  return dex_string_symbols


def _ParseDexfilesInApk(apk_path):
  with zipfile.ZipFile(apk_path) as src_zip:
    dex_infos = [
      info for info in src_zip.infolist() if info.filename.endswith('.dex')
    ]
    # Assume sound and stable ordering of DEX filenames.
    for dex_info in dex_infos:
      dex_data = src_zip.read(dex_info)
      yield dex_info.filename, dex_parser.DexFile(dex_data)


def _FormatMethodName(class_name, return_type, name, params):
  params = ','.join(params)
  if name in ('<init>', '<clinit>'):
    return f'{class_name} {name}({params})'
  return f'{class_name} {return_type} {name}({params})'


class _DexSizeAnalyzer:
  """Computes sizes of classes, methods, and fields across dex files.

  Sizes are computed the same way as dexlib2 (which SuperSize used via
  apkanalyzer), for consistency with older .size files:
  * Class: class_def_item + type_id_item + interfaces type_list +
    annotations_directory_item header + class annotation_set_item +
    class_data_item header + static values encoded_array_item header.
  * Method: method_id_item + encoded_method + code_item.
  * Field: field_id_item + encoded_field + field_annotation + static value.
  * Referenced (but not defined) members: method_id_item / field_id_item.

  Members are identified using their obfuscated names until all dex files have
  been processed, since resolving inherited members requires knowing the
  class hierarchy.
  """

  def __init__(self, mapping):
    self._mapping = mapping
    # Obfuscated class descriptor -> size.
    self._class_sizes = collections.Counter()
    # Obfuscated class descriptor -> tuple of supertype descriptors.
    self._supertypes = {}
    # (class desc, name, return desc, param descs) -> size.
    self._method_sizes = collections.Counter()
    # (class desc, name, type desc) -> size.
    self._field_sizes = collections.Counter()
    self._descriptor_to_java_name = functools.lru_cache(None)(
      dex_deobfuscate.DescriptorToJavaName
    )

  def AddDexFile(self, dexfile):
    type_strings = [
      dexfile.GetString(t.descriptor_idx) for t in dexfile.type_id_item_list
    ]
    method_id_item_list = dexfile.method_id_item_list
    field_id_item_list = dexfile.field_id_item_list
    proto_id_item_list = dexfile.proto_id_item_list

    @functools.lru_cache(None)
    def get_proto_parts(proto_idx):
      proto = proto_id_item_list[proto_idx]
      params = dexfile.GetTypeListStringsByOffset(proto.parameters_off)
      return type_strings[proto.return_type_idx], params

    def method_key(method_idx):
      item = method_id_item_list[method_idx]
      return_type, params = get_proto_parts(item.proto_idx)
      name = dexfile.GetString(item.name_idx)
      return (type_strings[item.type_idx], name, return_type, params)

    def field_key(field_idx):
      item = field_id_item_list[field_idx]
      name = dexfile.GetString(item.name_idx)
      return (type_strings[item.class_idx], name, type_strings[item.type_idx])

    defined_method_idxs = set()
    defined_field_idxs = set()
    for class_def in dexfile.class_def_item_list:
      class_desc = type_strings[class_def.class_idx]
      interfaces = dexfile.GetTypeListStringsByOffset(class_def.interfaces_off)
      supertypes = interfaces
      if class_def.superclass_idx != _NO_INDEX:
        supertypes = (type_strings[class_def.superclass_idx],) + interfaces
      self._supertypes[class_desc] = supertypes

      class_size = _CLASS_DEF_ITEM_SIZE + _TYPE_ID_ITEM_SIZE
      if interfaces:
        class_size += 4 + 2 * len(interfaces)

      annotated_field_idxs = ()
      if class_def.annotations_off:
        num_class_annotations, annotated_field_idxs = (
          dexfile.GetAnnotationsDirectoryInfo(class_def.annotations_off)
        )
        class_size += _ANNOTATIONS_DIRECTORY_HEADER_SIZE
        if num_class_annotations:
          class_size += 4 + 4 * num_class_annotations

      static_value_sizes = ()
      if class_def.static_values_off:
        header_size, static_value_sizes = dexfile.GetEncodedArrayItemSizes(
          class_def.static_values_off
        )
        class_size += header_size

      class_data = dexfile.GetClassDataItemByOffset(class_def.class_data_off)
      if class_data:
        class_size += class_data.header_size
        for i, encoded_field in enumerate(
          itertools.chain(class_data.static_fields, class_data.instance_fields)
        ):
          size = _FIELD_ID_ITEM_SIZE + encoded_field.byte_size
          if encoded_field.field_idx in annotated_field_idxs:
            size += _FIELD_ANNOTATION_SIZE
          # Static values apply only to static fields, which come first.
          if i < len(static_value_sizes) and i < len(class_data.static_fields):
            size += static_value_sizes[i]
          defined_field_idxs.add(encoded_field.field_idx)
          self._field_sizes[field_key(encoded_field.field_idx)] += size

        for encoded_method in itertools.chain(
          class_data.direct_methods, class_data.virtual_methods
        ):
          size = _METHOD_ID_ITEM_SIZE + encoded_method.byte_size
          if encoded_method.code_off:
            code_item = dexfile.GetCodeItemByOffset(encoded_method.code_off)
            size += code_item.byte_size
          defined_method_idxs.add(encoded_method.method_idx)
          self._method_sizes[method_key(encoded_method.method_idx)] += size

      self._class_sizes[class_desc] += class_size

    # Members that are referenced but not defined in this dex file.
    for method_idx in range(len(method_id_item_list)):
      if method_idx not in defined_method_idxs:
        self._method_sizes[method_key(method_idx)] += _METHOD_ID_ITEM_SIZE
    for field_idx in range(len(field_id_item_list)):
      if field_idx not in defined_field_idxs:
        self._field_sizes[field_key(field_idx)] += _FIELD_ID_ITEM_SIZE

  def _IterTypeHierarchy(self, class_desc):
    """Yields the given type and all of its supertypes (breadth-first)."""
    queue = collections.deque([class_desc])
    seen = set()
    while queue:
      cur = queue.popleft()
      if cur in seen:
        continue
      seen.add(cur)
      yield cur
      queue.extend(self._supertypes.get(cur, ()))

  def _LookupMember(self, class_desc, name, is_method, lookup_func):
    """Looks up a member using the mapping of the given class or a supertype.

    Uses the first type in the class hierarchy whose mapping contains a member
    with the given obfuscated name (members are not necessarily defined in the
    class they are referenced through).

    Args:
      class_desc: Obfuscated descriptor of the class containing the member.
      name: Obfuscated name of the member.
      is_method: Whether the member is a method (vs a field).
      lookup_func: Function that takes a class name and returns a MemberInfo.

    Returns:
      A tuple of (MemberInfo or None, whether any mapping entries were found).
    """
    for cur_desc in self._IterTypeHierarchy(class_desc):
      class_name = self._descriptor_to_java_name(cur_desc)
      if self._mapping.HasMemberNamed(class_name, name, is_method):
        return lookup_func(class_name), True
    return None, False

  def CreateNodes(self):
    """Returns a list of (name, size) for all classes, methods, and fields."""
    mapping = self._mapping
    deobfuscate_type = functools.lru_cache(None)(mapping.DeobfuscateDescriptor)
    sizes_by_name = collections.Counter()
    for class_desc, size in self._class_sizes.items():
      sizes_by_name[deobfuscate_type(class_desc)] += size

    referenced_class_descs = set()
    num_deobfuscated = 0
    num_failed = 0
    for key, size in self._method_sizes.items():
      class_desc, name, return_type, params = key
      referenced_class_descs.add(class_desc)
      class_name = deobfuscate_type(class_desc)
      info, found = self._LookupMember(
        class_desc,
        name,
        True,
        functools.partial(
          mapping.LookupMethod,
          name=name,
          return_descriptor=return_type,
          param_descriptors=params,
        ),
      )
      if info:
        num_deobfuscated += 1
        full_name = _FormatMethodName(
          class_name, info.return_type, info.name, info.params
        )
      else:
        if found:
          num_failed += 1
          logging.debug(
            'Failed to deobfuscate method: %s %s%s', class_desc, name, params
          )
        full_name = _FormatMethodName(
          class_name,
          deobfuscate_type(return_type),
          name,
          [deobfuscate_type(p) for p in params],
        )
      sizes_by_name[full_name] += size

    for key, size in self._field_sizes.items():
      class_desc, name, type_desc = key
      referenced_class_descs.add(class_desc)
      class_name = deobfuscate_type(class_desc)
      info, _ = self._LookupMember(
        class_desc,
        name,
        False,
        functools.partial(
          mapping.LookupField, name=name, type_descriptor=type_desc
        ),
      )
      if info:
        num_deobfuscated += 1
        full_name = f'{class_name} {info.return_type} {info.name}'
      else:
        full_name = f'{class_name} {deobfuscate_type(type_desc)} {name}'
      sizes_by_name[full_name] += size

    # Create empty nodes for classes that are referenced but not defined.
    for class_desc in referenced_class_descs:
      class_name = deobfuscate_type(class_desc)
      sizes_by_name.setdefault(class_name, 0)

    logging.info(
      'Deobfuscated %d / %d dex members (%d signature mismatches)',
      num_deobfuscated,
      len(self._method_sizes) + len(self._field_sizes),
      num_failed,
    )
    return list(sizes_by_name.items())


def _AnalyzeDexFiles(apk_path, mapping_path, track_string_literals):
  """Parses all dex files within an apk. Runs in a background process.

  Returns:
    A tuple of (nodes, string_literals, string_aggregates, metrics_by_file).
    See _DexSizeAnalyzer.CreateNodes() and _ExtractStringLiterals().
  """
  mapping = dex_deobfuscate.ParseMappingFile(mapping_path)
  analyzer = _DexSizeAnalyzer(mapping)
  string_literals = []
  string_aggregates = []
  metrics_by_file = {}
  for dex_path, dexfile in _ParseDexfilesInApk(apk_path):
    logging.debug('Found DEX: %r', dex_path)
    analyzer.AddDexFile(dexfile)
    if track_string_literals:
      logging.info('Extracting string literals from %s', dex_path)
      cur_literals, cur_aggregates = _ExtractStringLiterals(
        dexfile, mapping.class_map
      )
      string_literals += cur_literals
      string_aggregates += cur_aggregates
    map_item_sizes = dexfile.ComputeMapItemSizes()
    metrics = {}
    for item in map_item_sizes:
      metrics[f'{models.METRICS_SIZE}/' + item['name']] = item['byte_size']
      metrics[f'{models.METRICS_COUNT}/' + item['name']] = item['size']
    metrics_by_file[dex_path] = metrics

  nodes = analyzer.CreateNodes()
  return nodes, string_literals, string_aggregates, metrics_by_file


def AnalyzeDexAsync(apk_path, mapping_path, track_string_literals):
  """Starts parsing the dex files of the given apk in a background process.

  Args:
    apk_path: Path to the apk to run on.
    mapping_path: Path to the proguard mapping file (or None).
    track_string_literals: Whether to extract string literals.

  Returns:
    An object to pass to CreateDexSymbols().
  """
  return parallel.ForkAndCall(
    _AnalyzeDexFiles, (apk_path, mapping_path, track_string_literals)
  )


def CreateDexSymbols(
  apk_path,
  dex_async_result,
  dex_total_size,
  size_info_prefix,
):
  """Creates DEX symbols given the result of AnalyzeDexAsync().

  Args:
    apk_path: Path to the APK containing the DEX file.
    dex_async_result: Return value from AnalyzeDexAsync().
    dex_total_size: Sum of the sizes of all .dex files in the apk.
    size_info_prefix: Path such as: out/Release/size-info/BaseName.

  Returns:
    A tuple of (section_ranges, raw_symbols, metrics_by_file), where
    metrics_by_file is a dict from DEX file name to a dict of
    {metric_name: value}.
  """
  if size_info_prefix:
    source_map = _ParseJarInfoFile(size_info_prefix + '.jar.info')
  else:
    source_map = dict()

  logging.debug('Waiting for dex analysis to finish')
  nodes, string_literals, string_aggregates, metrics_by_file = (
    dex_async_result.get()
  )
  logging.debug('Analyzing DEX - processing results')

  total_node_size = sum(x[1] for x in nodes)
  if dex_total_size < total_node_size:
    logging.error(
      'Node size too large, check for node processing errors. '
      'dex_total_size=%d total_node_size=%d',
      dex_total_size,
      total_node_size,
    )

  dex_method_symbols, dex_other_symbols = _SymbolsFromNodes(nodes, source_map)
  dex_string_symbols = _CreateStringSymbols(
    apk_path, string_literals, string_aggregates, source_map
  )

  if dex_string_symbols:
    logging.info(
      'Converting excessive DEX string aliases into shared-path symbols'
    )
    archive_util.CompactLargeAliasesIntoSharedSymbols(
      dex_string_symbols, _DEX_STRING_MAX_SAME_NAME_ALIAS_COUNT
    )

  dex_method_size = round(sum(s.pss for s in dex_method_symbols))
  dex_other_size = round(sum(s.pss for s in dex_other_symbols))
  dex_other_size += round(sum(s.pss for s in dex_string_symbols))
  unattributed_dex = dex_total_size - dex_method_size - dex_other_size
  # Compare against -5 instead of 0 to guard against round-off errors.
  assert unattributed_dex >= -5, (
    'sum(dex_symbols.size) > sum(filesize(dex file)). {} vs {}'.format(
      dex_method_size + dex_other_size, dex_total_size
    )
  )

  if unattributed_dex > 0:
    dex_other_symbols.append(
      models.Symbol(
        models.SECTION_DEX, unattributed_dex, full_name='** .dex (unattributed)'
      )
    )

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
