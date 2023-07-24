#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Parses R8 disassembly outputs."""

import argparse
import code
import collections
import dataclasses
import logging
import readline  # Makes code.InteractiveConsole works better.

# R8 disassembly performs best-effort deobfuscation of symbols while dumping
# DEX bytecode for each class, each method. The high-level structure (in pseudo
# RegExp) is "H(CM*)*", where H = Header (not parsed); C = Class info;
# M = Method info.
#
# Class info has ({} = data; {{}} = comment; else literals) format:
#
#   # Bytecode for
#   # Class: "{class_name}"
#
# Method info has legacy format (pre 2022-11-10):
#
#   #
#   # Method: "{method_name}":
#   # {access_flags...}
#   #
#
# {{The rest are optional}}
#   {return_type} {class_name}.{method_name}({param_types})  {{= signature}}
#   registers: {...}, inputs: {...}, outputs: {...}
#   {bytecode...} {{multiple lines}}}
#
# Note that {signature} may be broken, and become:
#   {class_name} {method_name}
#
# Method info, has updated format (post 2022-11-10):
#
#   #
#   # Method: "{return_type} {class_name}.{method_name}({param_types})":
#   # {access_flags...}
#   # Residual: "{{Optional line: Similar to Method but obfuscated}}"
#   #
#
# {{Same optional lines as legacy format}}
#
# Notes:
# * Blank lines separate Class info and Method info blocks
# * {param_types...} is delimited by ", ", and may be empty.
# * {access_flags...} is a space-delimited list of words (unused by SuperSize).
# * {bytecode...} may have blank lines, but no lines starting with "#".
#
# Caveats:
# * If ambiguity exists, alternatives are delimited by " <OR> ".
# * Updated format for method info:
#   * "Method:" can be broken, just like {signature}.
#   * Signature data, if it exists, is redundant since it's just "Method:" data.
# * "Residual:" data might be inconsistent with "Method:". Therefore
#   heuristics should be applied to reconcile these.


@dataclasses.dataclass
class DexMethod:
  name: str
  class_name: str
  param_types: list = None
  return_type: str = None
  bytecode: list = None


class DexClass:
  def __init__(self, name):
    self.name = name
    self.methods = []

  def FindMethodByteCode(self, class_name, method_name, param_types,
                         return_type):
    for method in self.methods:
      if (method.name == method_name and method.class_name == class_name
          and method.return_type == return_type
          and method.param_types == param_types):
        return method.bytecode
    return None


class _WrapPeekableNoNewLine:
  """Line iterator decorator with peek(), and strips new line."""

  def __init__(self, it):
    self._it = it
    self._peek_lineno = 1
    self._buf = None  # Look-ahead cache to support peek().
    self._cur = None  # Most recent next() value.

  def _next_internal(self):
    ret = next(self._it, None)
    return None if ret is None else ret.rstrip('\n')

  def __next__(self):
    if self._buf is None:
      self._cur = self._next_internal()
    else:
      self._cur, self._buf = self._buf, None
    self._peek_lineno += 1
    return self._cur

  def peek(self):
    if self._buf is None:
      self._buf = self._next_internal()
    return self._buf

  def format_error(self, expected, is_peek=False):
    return ' '.join(('Line %d:' % (self._peek_lineno - int(not is_peek)),
                     'Expected %s,' % expected,
                     'got %r.' % (self.peek() if is_peek else self._cur)))


# pylint: disable=stop-iteration-return
def _ExtractMethodInfo(it):
  """Extracts coarse method data from R8 DEX dump.

  "Coarse" meaning detailed Method info data are left unparsed.

  Args:
    it: _WrapPeekableNoNewLine iterator for lines of R8 DEX dump.

  Yields:
    method_str: De-quoted string after "# Methods:".
    residual_str: De-quoted string after "# Residual:".
    signature: Signature string.
    byte_code: Lines of disassembled code (blank lines removed).
  """

  def is_end():
    return it.peek() is None or it.peek().startswith('# Class:')

  method_str = None
  while True:
    method_str = ''
    residual_str = ''
    signature = ''
    byte_code = []
    # Seek and read "Method:", possibly exit.
    while not is_end():
      if it.peek().startswith('# Method:'):
        break
      next(it)
    else:
      break
    method_str = next(it).split('\'')[1]
    # Skip access flags, e.g., "# public abstract".
    line = next(it)
    assert line.startswith('# '), it.format_error('comment with access flags')
    # Read "Residual:", made optional for compatibility.
    line = next(it)
    if line.startswith('# Residual:'):
      residual_str = line.split('\'')[1]
      line = next(it)
    # Eat line "#".
    assert line == '#', it.format_error('empty comment')
    # Eat blank line.
    assert next(it) == '', it.format_error('empty line')
    # Seek and read signature, possibly advance to next or exit.
    if it.peek() is None:
      break
    if it.peek().startswith('#'):
      yield (method_str, residual_str, signature, byte_code)
      continue
    assert it.peek() != '', it.format_error('comment or signature', True)
    signature = next(it)
    # Accumulate code, advance to next or exit.
    assert it.peek().startswith('registers:'), it.format_error(
        'registers/inputs/outputs', True)
    while not is_end():
      if it.peek().startswith('#'):
        break
      line = next(it)
      if line:
        byte_code.append(line + '\n')
    else:
      break
    yield (method_str, residual_str, signature, byte_code)

  if method_str:
    yield (method_str, residual_str, signature, byte_code)


def _ExtractClassNameAndMethodInfos(r8_lines_it):
  """Extracts Class and coarse Method info of R8 DEX dump.

  Args:
    r8_lines_it: Iterator for lines of R8 DEX dump.

  Yields:
    class_name: Name of classes.
    method_info: List of coarse Method info from _ExtractMethodInfo().
  """
  it = _WrapPeekableNoNewLine(r8_lines_it)

  while it.peek() is not None:
    line = next(it)
    if not line.startswith('# Class:'):
      continue
    yield line.split('\'')[1], list(_ExtractMethodInfo(it))


def _SplitMethod(signature):
  """Splits a method signature into components.

  Args:
    signature: "{return_type} {class_name}.{method_name}({param_types})".

  Returns:
    return_type: String for "{return_type}".
    class_name: String for "{class_name}".
    method_name: Sting for "{method_name}".
    param_types: List of strings for "{param_types}".
  """
  return_type_name, param_str = signature[:-1].split('(')
  return_type, method_full_name = return_type_name.split(' ')
  last_dot_pos = method_full_name.rfind('.')
  if last_dot_pos < 0:
    class_name = ''
    method_name = method_full_name
  else:
    class_name = method_full_name[:last_dot_pos]
    method_name = method_full_name[last_dot_pos + 1:]
  param_types = param_str.split(', ') if param_str else []
  return return_type, class_name, method_name, param_types


def _ExtractMethodLegacy(method_name, signature):
  """Extracts method info for legacy format.

  Returns: Same as _SplitMethod().
  """
  (return_type, class_name, _, param_types) = _SplitMethod(signature)
  return return_type, class_name, method_name, param_types


def _CompareSignatures(return_type1, param_types1, return_type2, param_types2):
  """Computes similarity score of two method signatures."""
  # Compute score component of return type.
  ret_score = 1 if return_type1 == return_type2 else 0
  # Compute score component of param types.
  if param_types1 == param_types2:
    # Fast path for common case (identical types).
    param_score = 1.0
  else:
    size = min(len(param_types1), len(param_types2))
    if size == 0:
      # Empty params vs. non-empty params: Score 0. Also prevents divide by 0.
      param_score = 0.0
    else:
      # Favor (normalized) param type matches in the same positions.
      pos_matches = sum(param_types1[i] == param_types2[i] for i in range(size))
      pos_matches /= size
      # Favor (normalized) param type multi-set matches.
      counter1 = collections.Counter(param_types1)
      counter2 = collections.Counter(param_types2)
      multi_set_matches = sum((counter1 & counter2).values()) / size
      # Penalize mismatched number of params.
      diff = abs(len(param_types1) - len(param_types2))
      # Estimated weighing factors.
      param_score = pos_matches * 0.6 + multi_set_matches * 0.3 - diff * 0.02
  return ret_score * 0.1 + param_score * 0.9


def _ExtractMethod(method_str, residual_str):
  """Extracts method info for updated format.

  Returns: Same as _SplitMethod().
  """
  residual = _SplitMethod(residual_str) if residual_str else None
  alternatives = method_str.split(' <OR> ')

  if alternatives[0].endswith(')'):
    # {return_type} {class_name}.{method_name}({param_types})
    assert all(f.endswith(')') for f in alternatives)
    method_parts = [_SplitMethod(f) for f in alternatives]
    scores = [(_CompareSignatures(method_parts[i][0], method_parts[i][3],
                                  residual[0], residual[3]), i)
              for i in range(len(alternatives))]
    return method_parts[max(scores)[1]]

  # Broken: {class_name} {method_name}
  assert len(alternatives) == 1
  if residual:
    return residual
  assert False, 'Failed to extract method.'
  return None


def Parse(lines):
  """Parses R8 disassembly lines into DexClass.

  Args:
    lines: Iterator for lines of R8 DEX dump.

  Returns:
    class_obj_map: Dict from class name to DexClass instances.
    anomalies: List [(class_name, extracted_class_name, method_name)] with
      unexplained mismatch between "{class_name}" from "Class:" and class name
      extracted from method signatures.
  """
  class_obj_map = {}
  total_methods = 0
  count_with_code = 0
  count_synthetics = 0
  warning_quota_signature = 8
  warning_quota_synthetic = 8
  anomalies = []
  for class_name, method_infos in _ExtractClassNameAndMethodInfos(lines):
    base_class_name = class_name.split('$$')[0]
    class_obj = DexClass(class_name)
    class_obj_map[class_name] = class_obj
    for method_str, residual_str, signature, byte_code in method_infos:
      total_methods += 1
      if not byte_code:
        continue
      count_with_code += 1
      if ' ' not in method_str:
        return_type, extracted_class_name, method_name, param_types = (
            _ExtractMethodLegacy(method_str, signature))
      else:
        if signature not in ('', method_str):
          if warning_quota_signature > 0:
            warning_quota_signature -= 1
            logging.warning('Found signature not matching "# Method":')
            logging.warning('  %s', signature)
            logging.warning('  %s', method_str)
        return_type, extracted_class_name, method_name, param_types = (
            _ExtractMethod(method_str, residual_str))
      if extracted_class_name != class_name:
        if '.' not in extracted_class_name:
          # |extracted_class_name| is obfuscated,
          pass
        elif extracted_class_name.split('$$')[0] == base_class_name:
          # Name aftet "$$" changed: These are synthetics examples showing
          # "true value" from |class_name| vs. "extracted value":
          # * org.chromium.base.FileUtils$$ExternalSyntheticLambda0 vs.
          #   org.chromium.base.FileUtils$$InternalSyntheticLambda$1${...}$0
          if 'Synthetic' not in class_name:
            if warning_quota_synthetic > 0:
              warning_quota_synthetic -= 1
              logging.warning('Found "$$" in non-synthetic class: %s',
                              class_name)
          count_synthetics += 1
        else:
          # Anomalies: Might be from class merging, but other oddities exist:
          # * androidx.activity.ComponentActivity vs.
          #   androidx.core.app.ComponentActivity;
          # * androidx.recyclerview.widget.SimpleItemAnimator vs.
          #   androidx.recyclerview.widget.RecyclerView$ItemAnimator
          # * kotlin.sequences.SequencesKt vs.
          #   kotlin.sequences.SequencesKt___SequencesKt
          # * com.google.android.play.core.splitinstall.a vs. (!)
          #   com.google.android.play.core.splitinstall.SplitInstallSessionState
          anomalies.append((class_name, extracted_class_name, method_name))
      method_obj = DexMethod(method_name, class_name, param_types, return_type,
                             byte_code)
      class_obj.methods.append(method_obj)
  logging.debug(
      'R8 disassembler method stats: Found %d total, %d with code, '
      '%d affected by synthetics, %d anomalies.', total_methods,
      count_with_code, count_synthetics, len(anomalies))
  return class_obj_map, anomalies


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('input',
                      type=str,
                      help='File containing outputs of deobfuscated R8 '
                      'disassembly output.')
  args = parser.parse_args()
  with open(args.input, 'rt', encoding='utf-8') as fh:
    class_obj_map, anomalies = Parse(fh)
  variables = {'class_obj_map': class_obj_map, 'anomalies': anomalies}
  banner = []
  banner.append('=' * 80)
  banner.append('class_obj_map: {method: DexClass obj}')
  banner.append('anomalies: [(class_name, extracted_class_name, method_name)]')
  code.InteractiveConsole(variables).interact('\n'.join(banner))


if __name__ == '__main__':
  main()
