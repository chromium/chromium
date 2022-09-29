#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Contains the parsers for .tsv and .xml files, annotations.tsv and
grouping.xml  respectively. Also includes methods to parse the json object
returned by the Google Doc API's .get() method.

These parsers are used to populate the duplicated Google Doc template with
several placeholders, and, to populate the traffic annotations with their
relevant attributes, e.g. description, policy, etc.
"""

from __future__ import print_function
from collections import namedtuple
from collections import OrderedDict
import xml.etree.ElementTree
import enum
import json
import csv
import sys
import io
import re

TrafficAnnotation = namedtuple(
    "TrafficAnnotation",
    ["unique_id", "description", "trigger", "data", "settings", "policy"])


class Placeholder(str, enum.Enum):
  GROUP = "group"
  SENDER = "sender"
  ANNOTATION = "annotation"
  ANNOTATION_BOLD = "annotation_bold"


PLACEHOLDER_STYLES = {
    Placeholder.GROUP: {
        "bold": False,
        "font": "Roboto",
        "fontSize": 20,
        "namedStyleType": "HEADING_1"
    },
    Placeholder.SENDER: {
        "bold": True,
        "font": "Roboto",
        "fontSize": 14,
        "namedStyleType": "HEADING_2"
    },
    Placeholder.ANNOTATION: {
        "bold": False,
        "font": "Roboto",
        "fontSize": 9
    },
    Placeholder.ANNOTATION_BOLD: {
        "bold": True,
        "font": "Roboto",
        "fontSize": 9
    }
}


def load_tsv_file(file_path, verbose):
  """ Loads annotations TSV file.

  Args:
    file_path: str
      Path to the TSV file.
    verbose: bool
      Whether to print messages about ignored rows.

  Returns:
    list of list Table of loaded annotations.
  """
  rows = []
  with io.open(file_path, mode="r", encoding="utf-8") as csvfile:
    reader = csv.reader(csvfile.readlines(), delimiter="\t")
    for row in reader:
      # If the last column of the file_row is empty, the row belongs to a
      # platform different from the one that TSV file is generated on, hence it
      # should be ignored.
      if row[-1]:
        rows.append(row)
      elif verbose:
        print("Ignored from other platforms: %s" % row[0])
  return rows


def map_annotations(tsv_contents):
  """Creates a mapping between the unique_id of a given annotation and its
  relevant attributes, e.g. description, trigger, data, etc.

  Args:
    tsv_contents: List[List]
      Table of loaded annotations.

  Returns:
    unique_id_rel_attributes_map: <Dict[str, TrafficAnnotation]>
  """
  unique_id_rel_attributes_map = {}
  for annotation_row in tsv_contents:
    unique_id = annotation_row[0].encode("utf-8")
    description = annotation_row[3].encode("utf-8")
    trigger = annotation_row[4].encode("utf-8")
    data = annotation_row[5].encode("utf-8")
    settings = annotation_row[9].encode("utf-8")
    policy = annotation_row[10].encode("utf-8")
    payload = [unique_id, description, trigger, data, settings, policy]

    unique_id_rel_attributes_map[unique_id] = TrafficAnnotation._make(payload)
  return unique_id_rel_attributes_map


class XMLParser:
  """Parses grouping.xml with the aim of generating the placeholders list"""

  def __init__(self, file_path, annotations_mapping):
    """
    Args:
      file_path: str
        The file path to the xml to parse. Ostensibly, grouping.xml located
        within traffic_annotation/summary.
      annotations_mapping: Dict[str, dict]
          The mapping between a given annotation's unique_id and its relevant
          attributes, e.g. description, policy, data, etc.
    """
    self.parsed_xml = {}
    self.annotations_mapping = annotations_mapping

    self.parse_xml(file_path)

  def parse_xml(self, file_path):
    """Parses the grouping.xml file and populates self.parsed_xml.

    self.parsed_xml: <{Group1: {sender: [traffic_annotations]}, ...}>
    """
    tree = xml.etree.ElementTree.parse(file_path)
    root = tree.getroot()
    for group in root.iter("group"):
      assert group.tag == "group"
      group_name = group.attrib["name"]
      # Suppress if hidden="true" in the group block. Will not include any of
      # the senders and annotations in the block.
      if group.attrib.get("hidden", "") == "true":
        continue
      self.parsed_xml[group_name] = {}

      for sender in group.iter("sender"):
        sender_name = sender.attrib["name"]
        # Suppress if hidden="true" (or hidden is even mentioned) in the given
        # annotation, don't include in traffic_annotations.
        traffic_annotations = sorted([
            t_annotation.attrib["unique_id"]
            for t_annotation in sender.iter("traffic_annotation")
            if t_annotation.attrib.get("hidden", "") != "true"
        ])
        self.parsed_xml[group_name][sender_name] = traffic_annotations

  def _sort_parsed_xml(self):
    """Sort on the group and sender keys in alphabetical order, note that
    annotations are already sorted."""
    self.parsed_xml = {
        k: OrderedDict(sorted(v.items()))
        for k, v in self.parsed_xml.items()
    }
    self.parsed_xml = OrderedDict(
        sorted(self.parsed_xml.items(), key=lambda t: t[0]))

  def _add_group_placeholder(self, name):
    return {"type": Placeholder.GROUP, "name": name}

  def _add_sender_placeholder(self, name):
    return {"type": Placeholder.SENDER, "name": name}

  def _add_annotation_placeholder(self, unique_id):
    """
    Args:
      unique_id: str
        The annotation's unique_id.
    """
    traffic_annotation = self.annotations_mapping.get(unique_id, None)
    is_complete = traffic_annotation and all(traffic_annotation)
    if not is_complete:
      print(
          "Warning: {} row is empty in annotations.tsv but is in grouping.xml".
          format(unique_id))
      traffic_annotation = TrafficAnnotation(unique_id, "NA", "NA", "NA", "NA",
                                             "NA")

    return {
        "type": Placeholder.ANNOTATION,
        "traffic_annotation": traffic_annotation
    }

  def build_placeholders(self):
    """
    Returns:
      The placeholders <list> to be added in the order of their appearance.
      The annotations are the TrafficAnnotation objects with the relevant
      information.
    """
    self._sort_parsed_xml()
    placeholders = []

    for group, senders in self.parsed_xml.items():
      placeholders.append(self._add_group_placeholder(group))
      for sender, annotations in senders.items():
        placeholders.append(self._add_sender_placeholder(sender))
        for annotation in annotations:
          placeholders.append(self._add_annotation_placeholder(annotation))
    return placeholders


def jprint(msg):
  print(json.dumps(msg, indent=4), file=sys.stderr)


def extract_body(document=None, target="body", json_file_path="template.json"):
  """Google Doc API returns a .json object. Parse this doc object to obtain its
  body.

  The |template.json| object of the current state of
  the document can be obtained by running the update_annotations_doc.py script
  using the --debug flag.
  """
  if document:
    doc = document
  else:
    try:
      with open(json_file_path) as json_file:
        doc = json.load(json_file)
    except IOError:
      print("Couldn't find the .json file.")

  if target == "all":
    return doc
  return doc[target]


def find_first_index(doc):
  """Finds the cursor index (location) that comes right after the Introduction
  section. Namely, the endIndex of the paragraph block the |target_text| belongs
  to.

  Returns: int
    The first cursor index (loc) of the template document, right after the
    Introduction section.
  """
  target_text = "The policy, if one exists, to control this type of network"
  padding = 1  # We pad so as to overwrite cleanly.

  body = extract_body(document=doc)
  contents = body["content"]
  for element in contents:
    if "paragraph" in element:
      end_index = element["endIndex"]
      lines = element["paragraph"]["elements"]
      for text_run in lines:
        if target_text in text_run["textRun"]["content"]:
          return end_index + padding


def find_last_index(doc):
  """
  Returns: int
    The last cursor index (loc) of the template document.
  """
  body = extract_body(document=doc)
  contents = body["content"]
  last_index = contents[-1]["endIndex"]
  return last_index - 1


def find_chrome_browser_version(doc):
  """Finds what the current chrome browser version is in the document.

  We grab the current "Chrome Browser version MAJOR.MINOR.BUILD.PATCH" from the
  document's header.

  Returns: str
    The chrome browser version string.
  """
  # Only one header.
  header = extract_body(document=doc, target="headers").values()[0]
  header_elements = header["content"][0]["paragraph"]["elements"]
  text = header_elements[0]["textRun"]["content"]
  current_version = re.search(r"([\d.]+)", text).group()
  return current_version


def find_bold_ranges(doc, debug=False):
  """Finds parts to bold given the targets of "trigger", "data", etc.

  Returns:
    The startIndex <int> and endIndex <int> tuple pairs as a list for all
    occurrences of the targets. <List[Tuple[int, int]]>
  """
  bold_ranges = []
  targets = ["Trigger", "Data", "Settings", "Policy"]
  content = extract_body(document=doc)["content"]

  for i, element in enumerate(content):
    element_type = list(element.keys())[-1]

    if element_type != "table":
      continue

    # Recall that table is 1x2 in Docs, first cell contains unique_id, second
    # cell has traffic annotation relevant attributes.

    # Unique id column, messy parsing through. You can inspect the json output
    # with jprint() to confirm/debug if broken.
    unique_id_col = element["table"]["tableRows"][0]["tableCells"][0][
        "content"][0]["paragraph"]["elements"][0]
    if debug:
      jprint(unique_id_col)
    assert "textRun" in unique_id_col, "Not the correct unique_id cell"

    start_index = unique_id_col["startIndex"]
    end_index = unique_id_col["endIndex"]
    bold_ranges.append((start_index, end_index))

    start_index, end_index = None, None  # Reset

    # The info column, messy parsing through. You can inspect the json output
    # with jprint() to confirm/debug if broken.
    info_elements = element["table"]["tableRows"][0]["tableCells"][1]["content"]
    for i, info_col in enumerate(info_elements):
      info_col = info_elements[i]

      start_index = info_col["startIndex"]
      content = info_col["paragraph"]["elements"][0]["textRun"]["content"]
      # To find the end_index, run through and find something in targets.
      for target in targets:
        if content.find("{}:".format(target)) != -1:
          # Contains the string "|target|:"
          end_index = start_index + len(target) + 1
          bold_ranges.append((start_index, end_index))
          break

      if debug:
        jprint(info_col)
        print("#" * 30)

  return bold_ranges
