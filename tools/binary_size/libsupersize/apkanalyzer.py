# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs apkanalyzer to parse dex files in an apk.

Assumes that apk_path.mapping and apk_path.jar.info is available.
"""

import collections
import logging
import os
import posixpath
import re
import subprocess
import zipfile

import models
import path_util
import parallel


_TOTAL_NODE_NAME = '<TOTAL>'
_OUTLINED_PREFIX = '$$Outlined$'


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

  def Normalize(self, class_path, full_name):
    # Make d8 desugared lambdas look the same as Desugar ones.
    # Desugar lambda: org.Promise$Nested1$$Lambda$0
    # 1) Need to prefix with proper class name so that they will show as nested.
    # 2) Need to suffix with number so that they diff better.
    # Original name will be kept as "object_path".
    # See tests for a more comprehensive list of what d8 currently generates.

    # Map nested classes to outer class.
    outer_class = _TruncateFrom(class_path, '$')

    # $$ is the convention for a synthetic class and all known desugared lambda
    # classes have 'Lambda' in the synthetic part of its name. If it doesn't
    # then it's almost certainly not a desugared lambda class.
    if 'Lambda' not in class_path[class_path.find('$$'):]:
      return outer_class, full_name

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
      return outer_class, full_name.replace(class_path, new_name)
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
      return outer_class, full_name.replace(class_path, new_name)
    # Example: package.FirebaseInstallationsRegistrar$$Lambda$1
    match = re.fullmatch(r'(?P<base_name>.+)\$\$Lambda\$\d+', class_path)
    if match:
      # Although these are already valid names, re-number them to avoid name
      # collisions with renamed InternalSyntheticLambdas.
      new_name = self._GetLambdaName(class_path=class_path,
                                     base_name=match.group('base_name'))
      return outer_class, full_name.replace(class_path, new_name)
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
      return outer_class, full_name.replace(class_path, new_name)
    assert False, (
        'No valid match for new lambda name format: ' + class_path + '\n'
        'Please update https://crbug.com/1208385 with this error so we can '
        'update the lambda normalization code.')
    return None


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
  # Create a special meta-directory for outlined lambdas to easily monitor their
  # total size and spot regressions.
  if name.startswith(_OUTLINED_PREFIX):
    object_path = posixpath.join(models.APK_PREFIX_PATH, 'Outlined',
                                 *old_package.split('.'))
  else:
    object_path = posixpath.join(models.APK_PREFIX_PATH,
                                 *old_package.split('.'))
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


def CreateDexSymbols(apk_analyzer_async_result, dex_total_size,
                     size_info_prefix):
  """Creates DEX symbols given apk_analyzer output.

  Args:
    apk_analyzer_async_result: Return value from RunApkAnalyzerAsync().
    dex_total_size: Sum of the sizes of all .dex files in the apk.
    size_info_prefix: Path such as: out/Release/size-info/BaseName.

  Returns:
    A tuple of (section_ranges, raw_symbols).
  """
  logging.debug('Waiting for apkanalyzer to finish')
  apk_analyzer_result = apk_analyzer_async_result.get()
  logging.debug('Analyzing DEX - processing results')
  source_map = _ParseJarInfoFile(size_info_prefix + '.jar.info')

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

  dex_method_size = round(sum(s.pss for s in dex_method_symbols))
  dex_other_size = round(sum(s.pss for s in dex_other_symbols))

  unattributed_dex = dex_total_size - dex_method_size - dex_other_size
  # Compare against -5 instead of 0 to guard against round-off errors.
  assert unattributed_dex >= -5, (
      'sum(dex_symbols.size) > filesize(classes.dex). {} vs {}'.format(
          dex_method_size + dex_other_size, dex_total_size))

  if unattributed_dex > 0:
    dex_other_symbols.append(
        models.Symbol(
            models.SECTION_DEX,
            unattributed_dex,
            full_name='** .dex (unattributed - includes string literals)'))

  # We can't meaningfully track section size of dex methods vs other, so
  # just fake the size of dex methods as the sum of symbols, and make
  # "dex other" responsible for any unattributed bytes.
  section_ranges = {
      models.SECTION_DEX_METHOD: (0, dex_method_size),
      models.SECTION_DEX: (0, dex_total_size - dex_method_size),
  }

  dex_other_symbols.extend(dex_method_symbols)
  return section_ranges, dex_other_symbols
