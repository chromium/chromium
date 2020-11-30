# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs apkanalyzer to parse dex files in an apk.

Assumes that apk_path.mapping and apk_path.jar.info is available.
"""

import collections
import logging
import os
import posixpath
import subprocess
import zipfile

import models
import path_util


_TOTAL_NODE_NAME = '<TOTAL>'
_DEX_PATH_COMPONENT = 'prebuilt'


def _ParseJarInfoFile(file_name):
  with open(file_name, 'r') as info:
    source_map = dict()
    for line in info:
      package_path, file_path = line.strip().split(',', 1)
      source_map[package_path] = file_path
  return source_map


def _RunApkAnalyzer(apk_path, mapping_path):
  args = [path_util.GetApkAnalyzerPath(), 'dex', 'packages', apk_path]
  if mapping_path and os.path.exists(mapping_path):
    args.extend(['--proguard-mappings', mapping_path])
  env = os.environ.copy()
  env['JAVA_HOME'] = path_util.GetJavaHome()
  output = subprocess.check_output(args, env=env).decode('ascii')
  data = []
  for line in output.splitlines():
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


def _ExpectedDexTotalSize(apk_path):
  dex_total = 0
  with zipfile.ZipFile(apk_path) as z:
    for zip_info in z.infolist():
      if not zip_info.filename.endswith('.dex'):
        continue
      dex_total += zip_info.file_size
  return dex_total


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

  def Normalize(self, package, name):
    # Make d8 desugared lambdas look the same as Desugar ones.
    # D8 lambda: org.-$$Lambda$Promise$Nested1$kjevdDQ8V2zqCrdieLqWLHzk.dex
    #     D8 lambdas may also have no .dex suffix.
    # Desugar lambda: org.Promise$Nested1$$Lambda$0
    # 1) Need to prefix with proper class name so that they will show as nested.
    # 2) Need to suffix with number so that they diff better.
    # Original name will be kept as "object_path".
    lambda_start_idx = package.find('-$$Lambda$')
    class_path = package
    if lambda_start_idx != -1:
      dex_suffix_idx = package.find('.dex')
      if dex_suffix_idx == -1:
        lambda_end_idx = len(package)
      else:
        lambda_end_idx = dex_suffix_idx + len('.dex')
      old_lambda_name = package[lambda_start_idx:lambda_end_idx]
      class_path = package.replace('-$$Lambda$', '')
      base_name = _TruncateFrom(class_path, '$', rfind=True)
      # Map all methods of the lambda class to the same nested number.
      lambda_number = self._lambda_name_to_nested_number.get(class_path)
      if lambda_number is None:
        # First time we've seen this lambda, increment nested class count.
        lambda_number = self._lambda_by_class_counter[base_name]
        self._lambda_name_to_nested_number[class_path] = lambda_number
        self._lambda_by_class_counter[base_name] = lambda_number + 1

      new_lambda_name = '{}$$Lambda${}'.format(base_name[lambda_start_idx:],
                                               lambda_number)
      name = name.replace(old_lambda_name, new_lambda_name)

    # Map nested classes to outer class.
    outer_class = _TruncateFrom(class_path, '$')
    return outer_class, name


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
  object_path = posixpath.join(models.APK_PREFIX_PATH, *old_package.split('.'))
  if name.endswith(')'):
    section_name = models.SECTION_DEX_METHOD
  else:
    section_name = models.SECTION_DEX

  return models.Symbol(section_name,
                       size,
                       full_name=name,
                       object_path=object_path,
                       source_path=source_path)


def CreateDexSymbols(apk_path, mapping_path, size_info_prefix):
  source_map = _ParseJarInfoFile(size_info_prefix + '.jar.info')

  nodes = _RunApkAnalyzer(apk_path, mapping_path)
  nodes = UndoHierarchicalSizing(nodes)

  dex_expected_size = _ExpectedDexTotalSize(apk_path)
  total_node_size = sum([x[2] for x in nodes])
  # TODO(agrieve): Figure out why this log is triggering for
  #     ChromeModernPublic.apk (https://crbug.com/851535).
  # Reporting: dex_expected_size=6546088 total_node_size=6559549
  if dex_expected_size < total_node_size:
    logging.error(
      'Node size too large, check for node processing errors. '
      'dex_expected_size=%d total_node_size=%d', dex_expected_size,
      total_node_size)
  # Use (DEX_METHODS, DEX) buckets to speed up sorting.
  symbols = ([], [])
  lambda_normalizer = LambdaNormalizer()
  for _, name, node_size in nodes:
    symbol = CreateDexSymbol(name, node_size, source_map, lambda_normalizer)
    if symbol:
      symbols[int(symbol.section_name is models.SECTION_DEX)].append(symbol)

  symbols[0].sort(key=lambda s: s.full_name)
  symbols[1].sort(key=lambda s: s.full_name)
  symbols[0].extend(symbols[1])
  return symbols[0]
