# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for demangling C++ symbols."""

import collections
import itertools
import logging
import re
import subprocess

import path_util

_LOWER_HEX_PATTERN = re.compile(r'^[0-9a-f]*$')
_PROMOTED_GLOBAL_NAME_DEMANGLED_PATTERN = re.compile(
    r' \((\.\d+)?\.llvm\.\d+\)$')
_PROMOTED_GLOBAL_NAME_RAW_PATTERN = re.compile(r'(\.\d+)?\.llvm\.\d+$')

def StripLlvmPromotedGlobalNames(name):
  """Strips LLVM promoted global names suffix, and returns the result.

  LLVM can promote global names by adding the suffix '.llvm.1234', or
  '.1.llvm.1234', where the last numeric suffix is a hash. If demangle is
  sucessful, the suffix transforms into, e.g., ' (.llvm.1234)' or
  ' (.1.llvm.1234)'. Otherwise the suffix is left as is. This function strips
  the suffix to prevent it from intefering with name comparison.
  """
  llvm_pos = name.find('.llvm.')
  if llvm_pos < 0:
    return name  # Handles most cases.
  if name.endswith(')'):
    return _PROMOTED_GLOBAL_NAME_DEMANGLED_PATTERN.sub('', name)
  return _PROMOTED_GLOBAL_NAME_RAW_PATTERN.sub('', name)


def _CanDemangle(name):
  return name.startswith('_Z') or name.startswith('.Lswitch.table._Z')


def _ExtractDemanglablePart(names):
  """For each name in |names|, yields the part that can be demangled."""

  def _IsLowerHex(s):
    return _LOWER_HEX_PATTERN.match(s) is not None

  for name in names:
    # Strip prefixes before '_Z', e.g., '.Lswitch.table.'.
    pos = name.find('_Z')
    if pos > 0:
      name = name[pos:]

    # Some mangled symbols end with '$' followed by 32 lower-case hex digits,
    # and possibly '.cfi'. These interfere with demangling by c++filt, and
    # should be stripped.
    if name.endswith('.cfi'):
      name = name[:-4]
    if len(name) > 33 and name[-33] == '$' and _IsLowerHex(name[-32:]):
      yield name[:-33]
    else:
      yield name


def _PostProcessDemangledSymbol(old_name, new_name):
  new_name = StripLlvmPromotedGlobalNames(new_name)
  if old_name.startswith('.Lswitch.table.'):
    new_name = 'Switch table for ' + new_name  # Becomes ... [Switch table].
  return new_name


def _DemangleNames(names, tool_prefix):
  """Uses c++filt to demangle a list of names."""
  proc = subprocess.Popen([path_util.GetCppFiltPath(tool_prefix)],
                          stdin=subprocess.PIPE, stdout=subprocess.PIPE)
  stdout = proc.communicate('\n'.join(_ExtractDemanglablePart(names)))[0]
  assert proc.returncode == 0
  ret = [
      _PostProcessDemangledSymbol(old_name, new_name)
      for (old_name, new_name) in itertools.izip(names, stdout.splitlines())
  ]
  if logging.getLogger().isEnabledFor(logging.INFO):
    fail_count = sum(1 for s in ret if _CanDemangle(s))
    if fail_count:
      logging.info('* Failed to demangle %d/%d items', fail_count, len(ret))
  return ret


def DemangleRemainingSymbols(raw_symbols, tool_prefix):
  """Demangles any symbols that need it."""
  to_process = [s for s in raw_symbols if _CanDemangle(s.full_name)]
  if not to_process:
    return

  logging.info('Demangling %d symbols', len(to_process))
  names = _DemangleNames([s.full_name for s in to_process], tool_prefix)
  for i, name in enumerate(names):
    to_process[i].full_name = name


def DemangleSetsInDicts(key_to_names, tool_prefix):
  """Demangles values as sets, and returns the result.

  |key_to_names| is a dict from key to sets (or lists) of mangled names.
  """
  all_names = []
  for names in key_to_names.itervalues():
    all_names.extend(n for n in names if _CanDemangle(n))
  if not all_names:
    return key_to_names

  logging.info('Demangling %d values', len(all_names))
  it = iter(_DemangleNames(all_names, tool_prefix))
  ret = {}
  for key, names in key_to_names.iteritems():
    ret[key] = set(next(it) if _CanDemangle(n) else n for n in names)
  assert(next(it, None) is None)
  return ret


def DemangleKeysAndMergeLists(name_to_list, tool_prefix):
  """Demangles keys of a dict of lists, and returns the result.

  Keys may demangle to a common name. When this happens, the corresponding lists
  are merged in arbitrary order.
  """
  keys = [key for key in name_to_list if _CanDemangle(key)]
  if not keys:
    return name_to_list

  logging.info('Demangling %d keys', len(keys))
  key_iter = iter(_DemangleNames(keys, tool_prefix))
  ret = collections.defaultdict(list)
  for key, val in name_to_list.iteritems():
    ret[next(key_iter) if _CanDemangle(key) else key] += val
  assert(next(key_iter, None) is None)
  logging.info('* %d keys become %d keys' % (len(name_to_list), len(ret)))
  return ret
