#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Prints expanded histograms."""

import argparse
import re
import xml.dom.minidom

import extract_histograms
import histogram_paths
import histogram_configuration_model
import merge_xml


def _ConstructHistogram(doc, name, histogram_dict):
  """Constructs a histogram node based on the |histogram_dict|."""
  histogram = doc.createElement('histogram')
  # Set histogram node attributes.
  histogram.setAttribute('name', name)
  if 'enum' in histogram_dict:
    histogram.setAttribute('enum', histogram_dict['enum']['name'])
  else:
    histogram.setAttribute('units', histogram_dict['units'])
  if 'expires_after' in histogram_dict:
    histogram.setAttribute('expires_after', histogram_dict['expires_after'])
  if histogram_dict.get('base', False):
    histogram.setAttribute('base', 'true')
  # Populate owner nodes.
  for owner in histogram_dict.get('owners', []):
    owner_node = doc.createElement('owner')
    owner_node.appendChild(doc.createTextNode(owner))
    histogram.appendChild(owner_node)
  # Populate the summary nodes.
  if 'summary' in histogram_dict:
    summary_node = doc.createElement('summary')
    summary_node.appendChild(doc.createTextNode(histogram_dict['summary']))
    histogram.appendChild(summary_node)
  return histogram


def main(args):
  try:
    pattern = re.compile(args.pattern)
  except re.error:
    print("Non valid regex pattern.")
    return

  # Extract all histograms into a dict.
  doc = merge_xml.MergeFiles(filenames=histogram_paths.ALL_XMLS,
                             expand_owners_and_extract_components=True)
  histograms, had_errors = extract_histograms.ExtractHistogramsFromDom(doc)
  if had_errors:
    raise ValueError("Error parsing inputs.")
  # Construct a dom tree that is similar to the normal histograms.xml so that
  # we can use histogram_configuration_model to pretty print it.
  doc = xml.dom.minidom.Document()
  configuration = doc.createElement('histogram-configuration')
  histograms_node = doc.createElement('histograms')
  for name, histogram in histograms.items():
    if re.match(pattern, name):
      histograms_node.appendChild(_ConstructHistogram(doc, name, histogram))
  configuration.appendChild(histograms_node)
  doc.appendChild(configuration)
  print(histogram_configuration_model.PrettifyTree(doc))


if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='Print expanded histograms.')
  parser.add_argument('--pattern',
                      type=str,
                      default='.*',
                      help='The histogram regex you want to print.')
  args = parser.parse_args()
  main(args)
