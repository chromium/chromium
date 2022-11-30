#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Parses a markdown file, extracts documentation for UMA metrics from the doc,
and writes that into histograms.xml file.

The syntax for the markdown this script processes is as follows:
  . The first line for each UMA metric should be: '## [metric name]'.
  . The following lines should include the additional information about the
    metric, in a markdown list, in '[name]: [value]' format. For example:

    * units: pixels
    * owners: first@chromium.org, second@example.com

  . The description, and explanation, of the metric should be after an empty
    line after the list of attributes.
  . Each UMA metric section should end with a line '---'. If there are non-UMA
    sections at the beginning of the doc, then the first UMA section should be
    preceeded by a '---' line.

A complete example:

=== sample.md
# A sample markdown document.
This is a sample markdown. It has some documentation for UMA metrics too.

# Motivation
The purpose of this sample doc is to be a guide for writing such docs.

---
## ExampleMetric.First
* units: smiles
* owners: firstowner@chromium.org, second@example.org
* os: windows, mac
* added: 2018-03-01
* expires: 2023-01-01

ExampleMetric.First measures the first example.
---
## ExampleMetric.Second
* units: happiness

This measures the second example.

"""

import datetime
import os
import re
import sys
import time
import xml.dom.minidom

sys.path.append(os.path.join(os.path.dirname(__file__), 'common'))
import path_util

sys.path.append(os.path.join(os.path.dirname(__file__), 'histograms'))
import pretty_print

SupportedTags = [
    "added",
    "expires",
    "enum",
    "os",
    "owners",
    "tags",
    "units",
]

def IsTagKnown(tag):
  return tag in SupportedTags


def IsTagValid(tag, value):
  assert IsTagKnown(tag)
  if tag == 'added' or tag == 'expires':
    if re.match('^M[0-9]{2,3}$', value):
      return True
    date = re.match('^([0-9]{4})-([0-9]{2})-([0-9]{2})$', value)
    return date and datetime.date(int(date.group(1)), int(date.group(2)),
                                  int(date.group(3)))
  return True


class Trace:
  def __init__(self, msg):
    self.msg_ = msg
    self.start_ = None

  def __enter__(self):
    self.start_ = time.time()
    sys.stdout.write('%s ...' % (self.msg_))
    sys.stdout.flush()

  def __exit__(self, exc_type, exc_val, exc_tb):
    sys.stdout.write(' Done (%.3f sec)\n' % (time.time() - self.start_))


def GetMetricsFromMdFile(mdfile):
  """Returns an array of metrics parsed from the markdown file. See the top of
  the file for documentation on the format of the markdown file.
  """
  with open(mdfile) as f:
    raw_md = f.read()
  metrics = []
  sections = re.split('\n---+\n', raw_md)
  tag_pattern = re.compile('^\* ([^:]*): (.*)$')
  for section in sections:
    if len(section.strip()) == 0: break
    lines = section.strip().split('\n')
    # The first line should have the header, containing the name of the metric.
    header_match = re.match('^##+ ', lines[0])
    if not header_match: continue
    metric = {}
    metric['name'] = lines[0][len(header_match.group(0)):]
    for i in range(1, len(lines)):
      if len(lines[i]) == 0:
        i += 1
        break
      match = tag_pattern.match(lines[i])
      assert match
      assert IsTagKnown(match.group(1)), 'Unknown tag: "%s".' % (match.group(1))
      assert IsTagValid(match.group(1), match.group(2)), 'Invalid value "%s" ' \
          'for tag "%s".' % (match.group(2), match.group(1))
      metric[match.group(1)] = match.group(2)
    assert i < len(lines), 'No summary found for "%s"' % metric['name']
    metric['summary'] = '\n'.join(lines[i:])
    assert 'owners' in metric, 'Must have owners for "%s"' % metric['name']
    assert 'enum' in metric or 'units' in metric, 'Metric "%s" must have ' \
        'a unit listed in "enum" or "units".' % metric['name']
    metrics.append(metric)
  return metrics


def CreateNode(tree, tag, text):
  node = tree.createElement(tag)
  node.appendChild(tree.createTextNode(text))
  return node


def main():
  """
  argv[1]: The path to the md file.
  argv[2]: The relative path of the xml file to be added.
  """
  if len(sys.argv) != 3:
    sys.stderr.write('Usage: %s <path-to-md-file> <path-to-histograms-file>\n' %
                     (sys.argv[0]))
    sys.exit(1)

  rel_path = sys.argv[2]
  with Trace('Reading histograms.xml') as t:
    xml_path = path_util.GetInputFile(
        os.path.join('tools', 'metrics', 'histograms', rel_path))
    with open(xml_path, 'rb') as f:
      raw_xml = f.read()

  with Trace('Parsing xml') as t:
    tree = xml.dom.minidom.parseString(raw_xml)
    histograms = tree.getElementsByTagName('histograms')
    if histograms.length != 1:
      sys.stderr.write('histograms.xml should have exactly one "histograms" '
                       'section.\n');
      sys.exit(1)
    histograms = histograms[0]

  with Trace('Parsing md file %s' % (sys.argv[1])) as t:
    metrics = GetMetricsFromMdFile(sys.argv[1])

  with Trace('Adding parsed metrics') as t:
    for metric in metrics:
      node = tree.createElement('histogram')
      node.setAttribute('name', metric['name'])
      if 'units' in metric:
        node.setAttribute('units', metric['units'])
      elif 'enum' in metric:
        node.setAttribute('enum', metric['enum'])
      owners = metric['owners'].split(',')
      for owner in owners:
        node.appendChild(CreateNode(tree, 'owner', owner))
      node.appendChild(CreateNode(tree, 'summary', metric['summary']))
      # TODO(sad): This always appends the metric to the list. This should
      # also update if there is an already existing metric, instead of adding a
      # new one.
      histograms.appendChild(node)

  with Trace('Pretty printing into histograms.xml') as t:
    new_xml = pretty_print.PrettyPrintHistogramsTree(tree)
    with open(xml_path, 'wb') as f:
      f.write(new_xml)


if __name__ == '__main__':
  main()
