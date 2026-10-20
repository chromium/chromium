# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities to restore DEX symbols name from obfuscated names."""

import collections
import json
import logging
import re

_PROGUARD_CLASS_MAPPING_RE = re.compile(
  r'(?P<original_name>[^ ]+)'
  r' -> '
  r'(?P<obfuscated_name>[^:]+):'
)

_RESIDUAL_SIGNATURE_ID = 'com.android.tools.r8.residualsignature'

_PRIMITIVE_DESCRIPTORS = {
  'V': 'void',
  'Z': 'boolean',
  'B': 'byte',
  'S': 'short',
  'C': 'char',
  'I': 'int',
  'J': 'long',
  'F': 'float',
  'D': 'double',
}


def DescriptorToJavaName(descriptor):
  """Converts a type descriptor to a Java type name.

  Examples:
    "Lfoo/Bar;" -> "foo.Bar"
    "[I" -> "int[]"
  """
  dims = 0
  while descriptor.startswith('[', dims):
    dims += 1
  base = descriptor[dims:]
  if base.startswith('L') and base.endswith(';'):
    name = base[1:-1].replace('/', '.')
  else:
    name = _PRIMITIVE_DESCRIPTORS.get(base, base)
  if dims:
    name += '[]' * dims
  return name


# name: Original name. Contains the original class name as a prefix (e.g.
#     "org.Foo.bar") when the member has been moved to a different class.
# return_type: Original return type (for fields, the field's type).
# params: Tuple of original parameter types (None for fields).
# residual_signature: Signature (type descriptor for fields) of the member
#     within the dex file, when it differs from the original signature.
# synthetic_signature: Tuple of (return_type, params) of the frame synthesized
#     by R8 for the method (see _ParseSyntheticFrame()), or None. This is the
#     signature of the method within the dex file.
MemberInfo = collections.namedtuple(
  'MemberInfo',
  'name,return_type,params,residual_signature,synthetic_signature',
  defaults=(None, None),
)


class _ClassInfo:
  """Holds the mapping information for a single class."""

  __slots__ = ('name', 'fields', 'methods')

  def __init__(self, name):
    self.name = name
    # For both fields and methods:
    # Obfuscated name -> {"type name(params)": [MemberInfo, ...]}.
    self.fields = {}
    self.methods = {}


def _ParseSignature(signature):
  """Returns (name, type, params) for a signature like "int foo(int)"."""
  space_idx = signature.index(' ')
  member_type = signature[:space_idx]
  paren_idx = signature.find('(', space_idx)
  if paren_idx == -1:
    return signature[space_idx + 1 :], member_type, None
  name = signature[space_idx + 1 : paren_idx]
  params = signature[paren_idx + 1 : -1]
  params = tuple(params.split(',')) if params else ()
  return name, member_type, params


def _AddMember(member_dict, obfuscated_name, signature, residual_signature):
  """Adds a member to |member_dict| if it is not already present.

  Args:
    member_dict: _ClassInfo.fields or _ClassInfo.methods.
    obfuscated_name: Obfuscated member name.
    signature: Original signature, without line numbers. E.g.:
        "int foo(java.lang.String)" or "int mField".
    residual_signature: The residualsignature (or None).

  Returns:
    The list of MemberInfo for |signature|.
  """
  infos_by_signature = member_dict.setdefault(obfuscated_name, {})
  infos = infos_by_signature.get(signature)
  if infos:
    # Residual signatures appear only after the first line of each method, so
    # subsequent lines for the same method should not create new entries.
    if residual_signature is None:
      return infos
    for i, info in enumerate(infos):
      if info.residual_signature == residual_signature:
        return infos
      if info.residual_signature is None:
        infos[i] = info._replace(residual_signature=residual_signature)
        return infos
  else:
    infos = []
    infos_by_signature[signature] = infos

  name, member_type, params = _ParseSignature(signature)
  infos.append(MemberInfo(name, member_type, params, residual_signature))
  return infos


def _IsSimpleNameOf(qualifier, class_name):
  """Returns whether |qualifier| is the package-less name of |class_name|."""
  # TODO(b/333617478): R8 uses package-less names for synthesized frames. For
  #     WebView, it uses the "WV." prefix (the package that WebView repackages
  #     obfuscated classes into).
  if qualifier.startswith('WV.'):
    qualifier = qualifier[3:]
  if '.' in qualifier:
    return False
  return class_name == qualifier or class_name.endswith('.' + qualifier)


def _ParseSyntheticFrame(signature, class_name):
  """Detects frames synthesized by R8.

  R8 creates frames for methods that it synthesizes (e.g. when changing a
  method's signature), which use package-less names for the class, and for
  types that were obfuscated. E.g., within class org.Foo:
    1:2:Bar Foo.method(java.lang.String,Bar):0 -> a
  Synthesized fields also use package-less class names:
    byte Foo.$r8$classId -> b

  Returns:
    The signature with the class qualifier removed, or None if |signature| is
    not a synthesized frame.
  """
  space_idx = signature.index(' ')
  paren_idx = signature.find('(', space_idx)
  if paren_idx == -1:
    paren_idx = len(signature)
  dot_idx = signature.rfind('.', space_idx, paren_idx)
  if dot_idx == -1:
    return None
  if not _IsSimpleNameOf(signature[space_idx + 1 : dot_idx], class_name):
    return None
  return signature[: space_idx + 1] + signature[dot_idx + 1 :]


class _MappingParser:
  """Parses .mapping file lines into a dict of {obfuscated name: _ClassInfo}."""

  def __init__(self):
    self.classes = {}
    self.has_residual_signatures = False
    self._cur_class = None
    # The method mapping lines that share the same obfuscated name and
    # obfuscated line range form a block. For inlined methods, the last line of
    # the block is the outer-most frame (the method that exists in the dex
    # file), and the others are the inlined frames.
    self._block_key = None
    # Tuple of (member_dict, obfuscated_name, signature) for the most recent
    # member line.
    self._pending = None
    self._pending_residual_signature = None
    # Signature of the line before |_pending| within the same block.
    self._prev_block_signature = None
    # List of (infos, synthetic_signature) for synthetic frames that have an
    # inner frame. |infos| is the list of MemberInfo for the inner frame.
    self._synthetic_frames = []
    # List of (member_dict, obfuscated_name, synthetic_signature,
    # residual_signature) for synthetic frames without an inner frame.
    self._lone_synthetic_frames = []

  def _FlushMember(self):
    if not self._pending:
      return
    member_dict, obfuscated_name, signature = self._pending
    residual_signature = self._pending_residual_signature
    self._block_key = None
    self._pending = None
    self._pending_residual_signature = None
    prev_block_signature = self._prev_block_signature
    self._prev_block_signature = None

    synthetic_signature = _ParseSyntheticFrame(signature, self._cur_class.name)
    if synthetic_signature and member_dict is self._cur_class.fields:
      signature = synthetic_signature
    elif synthetic_signature:
      if prev_block_signature:
        # Use the frame of the original method.
        infos = _AddMember(
          member_dict,
          obfuscated_name,
          prev_block_signature,
          residual_signature,
        )
        self._synthetic_frames.append((infos, synthetic_signature))
      else:
        self._lone_synthetic_frames.append(
          (
            member_dict,
            obfuscated_name,
            synthetic_signature,
            residual_signature,
          )
        )
      return
    _AddMember(member_dict, obfuscated_name, signature, residual_signature)

  def _HandleMetadata(self, stripped_line):
    # Example:
    # {"id":"com.android.tools.r8.residualsignature","signature":"(I)V"}
    if self._pending and _RESIDUAL_SIGNATURE_ID in stripped_line:
      metadata = json.loads(stripped_line[1:])
      self._pending_residual_signature = metadata['signature']

  def _HandleMemberLine(self, stripped_line):
    # Examples:
    #     int mField -> a
    #     1:3:void foo(int,java.lang.String):10:12 -> b
    #     void org.chromium.Other.bar() -> c
    arrow_idx = stripped_line.find(' -> ')
    if arrow_idx == -1:
      logging.debug('Unrecognized mapping line: %s', stripped_line)
      return
    obfuscated_name = stripped_line[arrow_idx + 4 :]
    signature = stripped_line[:arrow_idx]
    paren_idx = signature.find('(')
    if paren_idx == -1:
      self._FlushMember()
      self._pending = (self._cur_class.fields, obfuscated_name, signature)
      return

    line_range = None
    if signature[0].isdigit():
      # Strip obfuscated line range: "1:3:" (or "1:").
      colon_idx = signature.index(':')
      next_colon_idx = signature.find(':', colon_idx + 1)
      if signature[colon_idx + 1 : next_colon_idx].isdigit():
        colon_idx = next_colon_idx
      line_range = signature[:colon_idx]
      signature = signature[colon_idx + 1 :]
    # Strip original line numbers: ":10:12".
    signature = signature[: signature.index(')') + 1]

    # This relies on R8 assigning non-overlapping line ranges to methods that
    # share an obfuscated name (so that stack traces can be retraced).
    key = (line_range, obfuscated_name)
    if line_range is None or key != self._block_key:
      self._FlushMember()
    elif self._pending:
      self._prev_block_signature = self._pending[2]
    self._block_key = key
    self._pending = (self._cur_class.methods, obfuscated_name, signature)

  def ParseLine(self, line):
    if line.startswith(' '):
      if self._cur_class is None:
        return
      stripped_line = line.lstrip()
      if stripped_line.startswith('#'):
        self._HandleMetadata(stripped_line)
      else:
        self._HandleMemberLine(stripped_line)
      return

    if line.startswith('#'):
      if '"com.android.tools.r8.mapping"' in line:
        version = json.loads(line[1:]).get('version', '0')
        try:
          self.has_residual_signatures = tuple(
            int(x) for x in version.split('.')
          ) >= (2, 2)
        except ValueError:
          # E.g. "experimental".
          logging.warning('Unrecognized mapping file version: %s', version)
          self.has_residual_signatures = True
      return

    self._FlushMember()
    self._cur_class = None
    m = _PROGUARD_CLASS_MAPPING_RE.match(line)
    if m:
      self._cur_class = _ClassInfo(m.group('original_name'))
      self.classes[m.group('obfuscated_name')] = self._cur_class

  def Finish(self):
    self._FlushMember()
    if self._synthetic_frames or self._lone_synthetic_frames:
      self._ProcessSyntheticFrames()

  def _ProcessSyntheticFrames(self):
    names_by_simple_name = collections.defaultdict(list)
    for class_info in self.classes.values():
      name = class_info.name
      names_by_simple_name[name[name.rfind('.') + 1 :]].append(name)

    def fix_type(type_name):
      # Adds the package to type names when unambiguous.
      base_name = type_name.rstrip('[]')
      if '.' in base_name:
        return type_name
      names = names_by_simple_name.get(base_name, ())
      if len(names) == 1:
        return names[0] + type_name[len(base_name) :]
      return type_name

    def parse_synthetic_signature(signature):
      name, return_type, params = _ParseSignature(signature)
      return name, fix_type(return_type), tuple(fix_type(p) for p in params)

    for infos, signature in self._synthetic_frames:
      _, return_type, params = parse_synthetic_signature(signature)
      for i, info in enumerate(infos):
        if not info.synthetic_signature:
          infos[i] = info._replace(synthetic_signature=(return_type, params))

    # Synthetic frames without an inner frame usually belong to a method that
    # had an inner frame on a different line range.
    for (
      member_dict,
      obfuscated_name,
      signature,
      residual_signature,
    ) in self._lone_synthetic_frames:
      name, return_type, params = parse_synthetic_signature(signature)
      synthetic_signature = (return_type, params)
      infos_by_signature = member_dict.get(obfuscated_name, {})
      existing = next(
        (
          info
          for infos in infos_by_signature.values()
          for info in infos
          if info.synthetic_signature == synthetic_signature
        ),
        None,
      )
      if existing:
        continue
      # Do not use _AddMember(), since it would merge with a non-synthetic
      # method that has the same original signature (but whose signature was
      # changed by R8).
      signature = '{} {}({})'.format(return_type, name, ','.join(params))
      infos = member_dict.setdefault(obfuscated_name, {}).setdefault(
        signature, []
      )
      infos.append(
        MemberInfo(
          name, return_type, params, residual_signature, synthetic_signature
        )
      )


class ProguardMapping:
  """Deobfuscates class, method, and field names using a .mapping file."""

  def __init__(self, classes=None, has_residual_signatures=False):
    self._classes = classes or {}
    self._has_residual_signatures = has_residual_signatures
    # Map of obfuscated class name -> original class name.
    self.class_map = {k: v.name for k, v in self._classes.items()}

  def DeobfuscateClassName(self, name):
    """Returns the original name for the given (Java-style) class name."""
    return self.class_map.get(name, name)

  def DeobfuscateDescriptor(self, descriptor):
    """Returns the original Java type name for the given type descriptor."""
    name = DescriptorToJavaName(descriptor)
    if not descriptor.endswith(';'):
      return name
    idx = name.find('[')
    if idx == -1:
      return self.DeobfuscateClassName(name)
    return self.DeobfuscateClassName(name[:idx]) + name[idx:]

  def HasMemberNamed(self, class_name, name, is_method):
    """Returns whether the mapping has any member with the given names."""
    class_info = self._classes.get(class_name)
    if not class_info:
      return False
    return name in (class_info.methods if is_method else class_info.fields)

  def _GetCandidates(self, class_name, name, is_method):
    class_info = self._classes.get(class_name)
    if not class_info:
      return None
    member_dict = class_info.methods if is_method else class_info.fields
    infos_by_signature = member_dict.get(name)
    if not infos_by_signature:
      return None
    return [info for infos in infos_by_signature.values() for info in infos]

  def LookupMethod(
    self, class_name, name, return_descriptor, param_descriptors
  ):
    """Returns the MemberInfo for the given method, or None if not found.

    Args:
      class_name: Obfuscated (Java-style) name of the class containing the
          method.
      name: Obfuscated method name.
      return_descriptor: Type descriptor of the method's return type.
      param_descriptors: List of type descriptors for the method's parameters.
    """
    candidates = self._GetCandidates(class_name, name, True)
    if not candidates:
      return None

    signature = '({}){}'.format(''.join(param_descriptors), return_descriptor)
    deobfuscated_return_type = None
    deobfuscated_params = None
    # When R8 merges methods (e.g. via horizontal class merging), multiple
    # candidates can match. Prefer the last one (which is what apkanalyzer did).
    for candidate in reversed(candidates):
      if candidate.residual_signature:
        if candidate.residual_signature == signature:
          return candidate
        continue
      if deobfuscated_params is None:
        deobfuscated_return_type = self.DeobfuscateDescriptor(return_descriptor)
        deobfuscated_params = tuple(
          self.DeobfuscateDescriptor(p) for p in param_descriptors
        )
      return_type, params = candidate.synthetic_signature or (
        candidate.return_type,
        candidate.params,
      )
      if (
        params == deobfuscated_params
        and return_type == deobfuscated_return_type
      ):
        return candidate

    # Without residual signatures, R8's signature changes (e.g. removal of
    # unused parameters) cannot be resolved. Assume a match when there is only
    # one option.
    if not self._has_residual_signatures and len(candidates) == 1:
      return candidates[0]
    return None

  def LookupField(self, class_name, name, type_descriptor):
    """Returns the MemberInfo for the given field, or None if not found.

    Args:
      class_name: Obfuscated (Java-style) name of the class containing the
          field.
      name: Obfuscated field name.
      type_descriptor: Type descriptor of the field.
    """
    candidates = self._GetCandidates(class_name, name, False)
    if not candidates:
      return None
    deobfuscated_type = None
    for candidate in reversed(candidates):
      if candidate.residual_signature:
        if candidate.residual_signature == type_descriptor:
          return candidate
        continue
      if deobfuscated_type is None:
        deobfuscated_type = self.DeobfuscateDescriptor(type_descriptor)
      if candidate.return_type == deobfuscated_type:
        return candidate
    # R8 does not always emit residual signatures when it changes field types,
    # and does not give multiple fields the same name by default, so assume a
    # match when there is only one option.
    if len(candidates) == 1:
      return candidates[0]
    return None


def ParseMappingLines(lines):
  """Parses .mapping file lines into a ProguardMapping."""
  parser = _MappingParser()
  for line in lines:
    parser.ParseLine(line.rstrip('\r\n'))
  parser.Finish()
  return ProguardMapping(parser.classes, parser.has_residual_signatures)


def ParseMappingFile(path):
  """Parses a .mapping file into a ProguardMapping.

  Returns an empty ProguardMapping when |path| is None.
  """
  if path is None:
    return ProguardMapping()
  logging.debug('Parsing mapping file %s', path)
  with open(path, 'r', encoding='utf-8') as f:
    return ParseMappingLines(f)
