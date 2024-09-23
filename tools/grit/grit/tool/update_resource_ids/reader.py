# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helpers to read GRD files and estimate resource ID usages.

This module uses grit.grd_reader to estimate resource ID usages in GRD
(and GRDP) files by counting the occurrences of {include, message, structure}
tags. This approach avoids the complexties of conditional inclusions, but
produces a conservative estimate of ID usages.
"""


import collections
import os

from grit import grd_reader
from grit import util
from grit.tool.update_resource_ids import common

TAGS_OF_INTEREST = {'include', 'message', 'structure'}

def _CountResourceUsage(grd, seen_files):
  tag_name_to_count = {tag: set() for tag in TAGS_OF_INTEREST}
  # Pass '_chromium', but '_google_chrome' would produce the same result.
  root = grd_reader.Parse(grd,
                          defines={'_chromium': True},
                          skip_validation_checks=True)
  seen_files.add(grd)
  # Count all descendant tags, regardless of whether they're active.
  for node in root.Preorder():
    if node.name in TAGS_OF_INTEREST:
      tag_name_to_count[node.name].add(node.attrs['name'])
    elif node.name == 'part':
      part_path = os.path.join(os.path.dirname(grd), node.GetInputPath())
      seen_files.add(util.normpath(part_path))
  return {k: len(v) for k, v in tag_name_to_count.items() if v}


def GenerateResourceUsages(item_list, input_file_path, src_dir, fake,
                           seen_files):
  """Visits a list of ItemInfo to generate maps from tag name to usage.

  Args:
    item_list: ID assignments and structure from the parsed resource_ids.
    input_file_path: The path for the resource_ids input.
    src_dir: Absolute directory of Chrome's src/ directory.
    fake: For testing: Sets 10 as usages for all tags, to avoid reading GRD.
    seen_files: A set to collect paths of files read.
  Yields:
    Tuple (item, tag_name_to_usage), where |item| is from |item_list| and
      |tag_name_to_usage| is a dict() mapping tag name to (int) usage.
  """
  if fake:
    for item in item_list:
      tag_name_to_usage = collections.Counter({t.name: 10 for t in item.tags})
      yield item, tag_name_to_usage
    return
  for item in item_list:
    supported_tag_names = {tag.name for tag in item.tags}
    if item.meta and 'sizes' in item.meta:
      # If META has "sizes" field, use it instead of reading GRD.
      tag_name_to_usage = collections.Counter()
      for k, vlist in item.meta['sizes'].items():
        tag_name_to_usage[common.StripPlural(k.val)] = sum(v.val for v in vlist)
      tag_names = set(tag_name_to_usage.keys())
      if tag_names != supported_tag_names:
        raise ValueError('META "sizes" field have identical fields as actual '
                         '"sizes" field.')
    else:
      # Generated GRD start with '<(SHARED_INTERMEDIATE_DIR)'. Just check '<'.
      if item.grd.startswith('<'):
        raise ValueError('%s: Generated GRD must use META with "sizes" field '
                         'to specify size bounds.' % item.grd)
      grd_file = os.path.join(src_dir, item.grd)
      if not os.path.exists(grd_file):
        # Silently skip missing files so that src-internal files do not break
        # public checkouts.
        yield item, {}
        continue
      tag_name_to_usage = _CountResourceUsage(grd_file, seen_files)
      tag_names = set(tag_name_to_usage.keys())
      if not tag_names.issubset(supported_tag_names):
        missing = [t + 's' for t in tag_names - supported_tag_names]
        raise ValueError(
            'Resource ids for %s missing entry for %s. Check %s.' %
            (item.grd, missing, os.path.relpath(input_file_path, src_dir)))
    yield item, tag_name_to_usage
