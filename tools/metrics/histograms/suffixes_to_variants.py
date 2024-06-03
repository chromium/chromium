# Copyright 2020 The Chromium Authors
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
    'tools/metrics/histograms/metadata/histogram_suffixes_list.xml')


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


def _AreAllAffectedHistogramsFound(affected_histograms, histograms):
  """Checks that are all affected histograms found in |histograms|."""
  histogram_names = [histogram.getAttribute('name') for histogram in histograms]
  return all(
      affected_histogram.getAttribute('name') in histogram_names
      for affected_histogram in affected_histograms)


def _GetSuffixesDict(nodes, all_histograms):
  """Gets a dict of simple histogram-suffixes to be used in the migration.

  Returns two dicts of histogram-suffixes to be migrated to the new patterned
  histograms syntax.

  The first dict: the keys are the histogram-suffixes' affected histogram name
  and the values are the histogram_suffixes nodes that have only one
  affected-histogram. These histograms-suffixes can be converted to inline
  patterned histograms.

  The second dict: the keys are the histogram_suffixes name and the values
  are the histogram_suffixes nodes whose affected-histograms are all present in
  the |all_histograms|. These histogram suffixes can be converted to out-of-line
  variants.

  Args:
    nodes: A Nodelist of histograms_suffixes nodes.
    all_histograms: A Nodelist of all chosen histograms.

  Returns:
    A dict of histograms-suffixes nodes keyed by their names.
  """

  single_affected = {}
  all_affected_found = {}
  for histogram_suffixes in nodes:
    affected_histograms = histogram_suffixes.getElementsByTagName(
        'affected-histogram')
    if len(affected_histograms) == 1:
      affected_histogram = affected_histograms[0].getAttribute('name')
      single_affected[affected_histogram] = histogram_suffixes
    elif _AreAllAffectedHistogramsFound(affected_histograms, all_histograms):
      for affected_histogram in affected_histograms:
        affected_histogram = affected_histogram.getAttribute('name')
        if affected_histogram in all_affected_found:
          logging.warning(
              'Histogram %s is already associated with other suffixes. '
              'Please manually migrate it.', affected_histogram)
          continue
        all_affected_found[affected_histogram] = histogram_suffixes
  return single_affected, all_affected_found


def _GetBaseVariant(doc, histogram):
  """Returns a <variant> node whose name is an empty string as the base variant.

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
  suffixes_name = histogram_suffixes.getAttribute('name')
  for suffix in histogram_suffixes.getElementsByTagName('suffix'):
    # The base suffix is a much more complicated case. It might require manual
    # effort to migrate them so skip this case for now.
    suffix_name = suffix.getAttribute('name')
    if suffix.hasAttribute('base'):
      logging.warning(
          'suffix: %s in histogram_suffixes %s has base attribute. Please '
          'manually migrate it.', suffix_name, suffixes_name)
      return False
    # Suffix name might be empty. In this case, in order not to collide with the
    # base variant, remove the base variant first before populating this.
    if not suffix_name:
      logging.warning(
          'histogram suffixes: %s contains empty string suffix and thus we '
          'have to manually update the empty string variant in these base '
          'histograms: %s.', suffixes_name, ','.join(
              h.getAttribute('name') for h in
              histogram_suffixes.getElementsByTagName('affected-histogram')))
      return False
    variant = doc.createElement('variant')
    if histogram_suffixes.hasAttribute('ordering'):
      variant.setAttribute('name', suffix_name + separator)
    else:
      variant.setAttribute('name', separator + suffix_name)
    if suffix.hasAttribute('label'):
      variant.setAttribute('summary', suffix.getAttribute('label'))
    # Populate owner's node from histogram suffixes to each new variant.
    for owner in suffixes_owners:
      variant.appendChild(owner.cloneNode(deep=True))
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

  # Populate <variant>s to the inline <token> node.
  if not _PopulateVariantsWithSuffixes(doc, token, histogram_suffixes):
    logging.warning('histogram_suffixes: %s needs manually effort',
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


def MigrateToOutOflinePatterenedHistogram(doc, histogram, histogram_suffixes):
  """Migates a histogram suffixes to out-of-line patterned histogram."""
  # Update histogram's name with the histogram_suffixes' name.
  histogram_suffixes_name = histogram_suffixes.getAttribute('name')
  _UpdateHistogramName(histogram, histogram_suffixes)

  # Append |histogram_suffixes_name| placeholder string to the summary text.
  _UpdateSummary(histogram, histogram_suffixes_name)

  # Create a <token> node that links to an out-of-line <variants>.
  token = doc.createElement('token')
  token.setAttribute('key', histogram_suffixes_name)
  token.setAttribute('variants', histogram_suffixes_name)
  token.appendChild(_GetBaseVariant(doc, histogram))
  histogram.appendChild(token)
  # Remove obsolete comments from the histogram node.
  _RemoveSuffixesComment(histogram, histogram_suffixes_name)


def _MigrateOutOfLineVariants(doc, histograms, suffixes_to_convert):
  """Converts a histogram-suffixes node to an out-of-line variants."""
  histograms_node = histograms.getElementsByTagName('histograms')
  assert len(histograms_node) == 1, (
      'Every histograms.xml should have only one <histograms> node.')
  for suffixes in suffixes_to_convert:
    histogram_suffixes_name = suffixes.getAttribute('name')
    variants = doc.createElement('variants')
    variants.setAttribute('name', histogram_suffixes_name)
    if not _PopulateVariantsWithSuffixes(doc, variants, suffixes):
      logging.warning('histogram_suffixes: %s needs manually effort',
                      histogram_suffixes_name)
    else:
      histograms_node[0].appendChild(variants)
      suffixes.parentNode.removeChild(suffixes)


def ChooseFiles(args):
  """Chooses a set of files to process so that we can migrate incrementally."""
  paths = []
  for path in sorted(histogram_paths.HISTOGRAMS_XMLS):
    if 'metadata' in path and path.endswith('histograms.xml'):
      name = os.path.basename(os.path.dirname(path))
      if args.start <= name[0] <= args.end:
        paths.append(path)
  return paths


def SuffixesToVariantsMigration(args):
  """Migates all histogram suffixes to patterned histograms."""
  histogram_suffixes_list = minidom.parse(open(HISTOGRAM_SUFFIXES_LIST_PATH))
  histogram_suffixes_nodes = histogram_suffixes_list.getElementsByTagName(
      'histogram_suffixes')

  doc = minidom.Document()
  for histograms_file in ChooseFiles(args):
    histograms = minidom.parse(open(histograms_file))
    single_affected, all_affected_found = _GetSuffixesDict(
        histogram_suffixes_nodes, histograms.getElementsByTagName('histogram'))
    suffixes_to_convert = set()
    for histogram in histograms.getElementsByTagName('histogram'):
      name = histogram.getAttribute('name')
      # Migrate inline patterned histograms.
      if name in single_affected.keys():
        MigrateToInlinePatterenedHistogram(doc, histogram,
                                           single_affected[name])
      elif name in all_affected_found.keys():
        suffixes_to_convert.add(all_affected_found[name])
        MigrateToOutOflinePatterenedHistogram(doc, histogram,
                                              all_affected_found[name])

    _MigrateOutOfLineVariants(doc, histograms, suffixes_to_convert)

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
  args = parser.parse_args()
  assert len(args.start) == 1 and len(args.end) == 1, (
      'start and end flag should only contain a single letter.')
  SuffixesToVariantsMigration(args)
