# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Migrates histogram_suffixes to patterned histograms"""

import argparse
import logging
import os

from xml.dom import minidom

import extract_histograms
import histogram_configuration_model
import histogram_paths
import path_util

HISTOGRAM_SUFFIXES_LIST_PATH = path_util.GetInputFile(
    'tools/metrics/histograms/histograms_xml/histogram_suffixes_list.xml')


def _ExtractObsoleteNode(node):
  """Extracts obsolete child from |node|. Returns None if not exists."""
  obsolete = node.getElementsByTagName('obsolete')
  if not obsolete:
    return None
  assert len(obsolete) == 1, (
      'Node %s should at most contain one obsolete node.' %
      node.getAttribute('name'))
  return obsolete[0]


def _ExtractOwnerNodes(node):
  """Extracts all owners from |node|. Returns None if not exists."""
  return node.getElementsByTagName('owner')


def _RemoveSuffixesComment(node, histogram_suffixes_name):
  """Remove suffixes related comments from |node|."""
  for child in node.childNodes:
    if child.nodeType == minidom.Node.COMMENT_NODE:
      if ('Name completed by' in child.data
          and histogram_suffixes_name in child.data):
        node.removeChild(child)


def _UpdateSummary(histogram, histogram_suffixes_name):
  """Appends a placeholder string to the |histogram|'s summary node."""
  summary = histogram.getElementsByTagName('summary')
  assert len(summary) == 1, 'A histogram should have a single summary node.'
  summary = summary[0]
  if summary.firstChild.nodeType != summary.TEXT_NODE:
    raise ValueError('summary_node doesn\'t contain text.')
  summary.firstChild.replaceWholeText(
      '%s {%s}' % (summary.firstChild.data.strip(), histogram_suffixes_name))


def _GetSuffixesDictWithSingleAffectedHistogram(nodes):
  """Gets a dict of histogram-suffixes with a single affected histogram.

  Returns a dict where the keys are the histogram-suffixes' affected histogram
  name and the values are the histogram_suffixes nodes that have only one
  affected-histogram. These histograms-suffixes can be converted to inline
  patterned histograms.

  Args:
    nodes: A Nodelist of histograms_suffixes nodes.

  Returns:
    A dict of histograms-suffixes nodes keyed by their names.
  """
  histogram_suffixes_dict = {}
  for histogram_suffixes in nodes:
    affected_histograms = histogram_suffixes.getElementsByTagName(
        'affected-histogram')
    if len(affected_histograms) == 1:
      affected_histogram = affected_histograms[0].getAttribute('name')
      histogram_suffixes_dict[affected_histogram] = histogram_suffixes
  return histogram_suffixes_dict


def _GetBaseVariant(doc, histogram):
  """Returns a <variant> node whose name is an empty string as the base variant.

  If histogram has attribute `base = True`, it means that the base histogram
  should be marked as obsolete.

  Args:
    doc: A Document object which is used to create a new <variant> node.
    histogram: The <histogram> node to check whether its base is true or not.

  Returns:
     A <variant> node.
  """
  is_base = False
  if histogram.hasAttribute('base'):
    is_base = histogram.getAttribute('base').lower() == 'true'
    histogram.removeAttribute('base')
  base_variant = doc.createElement('variant')
  base_variant.setAttribute('name', '')
  if is_base:
    base_obsolete_node = doc.createElement('obsolete')
    base_obsolete_node.appendChild(
        doc.createTextNode(
            extract_histograms.DEFAULT_BASE_HISTOGRAM_OBSOLETE_REASON))
    base_variant.appendChild(base_obsolete_node)
  return base_variant


def _PopulateVariantsWithSuffixes(doc, node, histogram_suffixes):
  """Populates <variant> nodes to |node| from <suffix>.

  This function returns True if none of the suffixes contains 'base' attribute.
  If this function returns false, the caller's histogram node will not be
  updated. This is mainly because base suffix is a much more complicated case
  and thus it can not be automatically updated at least for now.

  Args:
    doc: A Document object which is used to create a new <variant> node.
    node: The node to be populated. it should be either <token> for inline
      variants or <variants> for out-of-line variants.
    histogram_suffixes: A <histogram_suffixes> node.

  Returns:
    True if the node can be updated automatically.
  """
  separator = histogram_suffixes.getAttribute('separator')
  suffixes_owners = _ExtractOwnerNodes(histogram_suffixes)
  for suffix in histogram_suffixes.getElementsByTagName('suffix'):
    # The base suffix is a much more complicated case. It might require manually
    # effort to migrate them so skip this case for now.
    if suffix.hasAttribute('base'):
      return False
    suffix_name = suffix.getAttribute('name')
    # Suffix name might be empty. In this case, in order not to collide with the
    # base variant, remove the base variant first before populating this.
    if not suffix_name:
      base_variant = node.firstChild
      if not base_variant.getAttribute('name'):
        node.removeChild(base_variant)
    variant = doc.createElement('variant')
    if histogram_suffixes.hasAttribute('ordering'):
      variant.setAttribute('name', suffix_name + separator)
    else:
      variant.setAttribute('name', separator + suffix_name)
    if suffix.hasAttribute('label'):
      variant.setAttribute('summary', suffix.getAttribute('label'))
    # Obsolete the obsolete node from suffix to the new variant.
    obsolete = _ExtractObsoleteNode(suffix)
    if obsolete:
      variant.appendChild(obsolete)
    # Populate owner's node from histogram suffixes to each new variant.
    for owner in suffixes_owners:
      variant.appendChild(owner)
    node.appendChild(variant)
  return True


def _UpdateHistogramName(histogram, histogram_suffixes):
  """Adds histogram_suffixes's placeholder to the histogram name."""
  histogram_name = histogram.getAttribute('name')
  histogram_suffixes_name = histogram_suffixes.getAttribute('name')
  ordering = histogram_suffixes.getAttribute('ordering')
  if not ordering:
    histogram.setAttribute('name',
                           '%s{%s}' % (histogram_name, histogram_suffixes_name))
  else:
    parts = ordering.split(',')
    placement = 1
    if len(parts) > 1:
      placement = int(parts[1])
    sections = histogram_name.split('.')
    cluster = '.'.join(sections[0:placement]) + '.'
    reminder = '.'.join(sections[placement:])
    histogram.setAttribute(
        'name', '%s{%s}%s' % (cluster, histogram_suffixes_name, reminder))


def MigrateToInlinePatterenedHistogram(doc, histogram, histogram_suffixes):
  """Migates a single histogram suffixes to an inline patterned histogram."""
  # Keep a deep copy in case when the |histogram| fails to be migrated.
  old_histogram = histogram.cloneNode(deep=True)
  # Update histogram's name with the histogram_suffixes' name.
  histogram_suffixes_name = histogram_suffixes.getAttribute('name')
  _UpdateHistogramName(histogram, histogram_suffixes)

  # Append |histogram_suffixes_name| placeholder string to the summary text.
  _UpdateSummary(histogram, histogram_suffixes_name)

  # Create an inline <token> node.
  token = doc.createElement('token')
  token.setAttribute('key', histogram_suffixes_name)
  token.appendChild(_GetBaseVariant(doc, histogram))

  # Popluate <variant>s to the inline <token> node.
  if not _PopulateVariantsWithSuffixes(doc, token, histogram_suffixes):
    logging.info('histogram_suffixes: %s needs manually effort',
                 histogram_suffixes_name)
    histograms = histogram.parentNode
    histograms.removeChild(histogram)
    # Restore old histogram when we the script fails to migrate it.
    histograms.appendChild(old_histogram)
  else:
    histogram.appendChild(token)
    histogram_suffixes.parentNode.removeChild(histogram_suffixes)
    # Remove obsolete comments from the histogram node.
    _RemoveSuffixesComment(histogram, histogram_suffixes_name)


def ChooseFiles(args):
  """Chooses a set of files to process so that we can migrate incrementally."""
  paths = []
  for path in sorted(histogram_paths.HISTOGRAMS_XMLS):
    if 'histograms_xml' in path and path.endswith('histograms.xml'):
      name = os.path.basename(os.path.dirname(path))
      if args.start <= name[0] <= args.end:
        paths.append(path)

  if args.obsolete:
    paths.append(histogram_paths.OBSOLETE_XML)
  return paths


def SuffixesToVariantsMigration(args):
  """Migates all histogram suffixes to patterned histograms."""
  histogram_suffixes_list = minidom.parse(open(HISTOGRAM_SUFFIXES_LIST_PATH))
  histogram_suffixes_nodes = histogram_suffixes_list.getElementsByTagName(
      'histogram_suffixes')

  single_affected_histogram = _GetSuffixesDictWithSingleAffectedHistogram(
      histogram_suffixes_nodes)

  doc = minidom.Document()
  for histograms_file in ChooseFiles(args):
    histograms = minidom.parse(open(histograms_file))
    for histogram in histograms.getElementsByTagName('histogram'):
      name = histogram.getAttribute('name')
      # Migrate inline patterned histograms.
      if name in single_affected_histogram.keys():
        MigrateToInlinePatterenedHistogram(doc, histogram,
                                           single_affected_histogram[name])

    # Update histograms.xml with patterned histograms.
    with open(histograms_file, 'w') as f:
      pretty_xml_string = histogram_configuration_model.PrettifyTree(histograms)
      f.write(pretty_xml_string)

  # Remove histogram_suffixes that have already been migrated.
  with open(HISTOGRAM_SUFFIXES_LIST_PATH, 'w') as f:
    pretty_xml_string = histogram_configuration_model.PrettifyTree(
        histogram_suffixes_list)
    f.write(pretty_xml_string)


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--start',
      help='Start migration from a certain character (inclusive).',
      default='a')
  parser.add_argument('--end',
                      help='End migration at a certain character (inclusive).',
                      default='z')
  parser.add_argument('--obsolete',
                      help='Whether to migrate obsolete_histograms.xml',
                      default=False)
  args = parser.parse_args()
  assert len(args.start) == 1 and len(args.end) == 1, (
      'start and end flag should only contain a single letter.')
  SuffixesToVariantsMigration(args)
