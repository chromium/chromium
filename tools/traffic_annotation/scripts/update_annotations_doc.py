#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script updates the Chrome Browser Network Traffic Annotations document.
To run the script, you should first generate annotations.tsv using
traffic_annotation_auditor.

To run the script, call: `update_annotations_doc --config-file=[config.json]
--annotations-file=[path_to_annotations.tsv]`

Run `update_annotations_doc --config-help` for help on the config.json
configuration file.
"""

from __future__ import print_function
import argparse
import datetime
import httplib2
import time
import json
import sys
import os

from apiclient import discovery
from infra_libs import luci_auth
from oauth2client import client
from oauth2client import tools
from oauth2client.file import Storage

import generator_utils
from generator_utils import (XMLParser, map_annotations, load_tsv_file,
                             Placeholder, PLACEHOLDER_STYLES)

# Absolute path to chrome/src.
SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
SRC_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "../../.."))


class NetworkTrafficAnnotationsDoc:
  SCOPES = "https://www.googleapis.com/auth/documents"
  APPLICATION_NAME = "Chrome Network Traffic Annotations Document Updater"

  # Colors are given as RGB percentages
  BLUE = {"red": 0.812, "green": 0.886, "blue": 0.953}
  WHITE = {"red": 1.0, "green": 1.0, "blue": 1.0}

  def __init__(self,
               doc_id,
               doc_name,
               credentials_file_path,
               client_token_file_path,
               verbose,
               index=None):
    """
    Args:
      doc_id: str
        ID of the annotations document for clients. This is the destination
        document where updates are made.
      doc_name: str
        Name of the document that contains the annotations for clients.
      credentials_file_path: str
        Path relative to src to read user credentials (credentials.json).
      client_token_file_path: str
        Path relative to src to read/save user credentials (token.pickle).
      verbose: bool
        Flag requesting dump of API status calls.
      index: int
        Where to begin adding content to. If index=None, will automatically
        find the index corresponding to the end of the template file.
    """
    self.destination_id = doc_id
    self.doc_name = doc_name
    self.index = index
    self._docs_service = None
    self._color_bool = True
    self._credentials_file_path = credentials_file_path
    self._client_token_file_path = client_token_file_path

    self.verbose = verbose

  def update_doc(self, placeholders):
    """Updates the chrome version of the destination document and includes all
    the annotations within the grouping.xml file.

    Args:
        placeholders:
          Contains the order of the placeholders to construct template
    """
    self._docs_service = self._initialize_service(
      self._get_credentials(
        self._credentials_file_path, self._client_token_file_path))
    doc = self._get_doc_contents(self.destination_id)
    self._update_chrome_version(doc)
    self.index = self._clear_destination_contents(doc)
    self._insert_placeholders(placeholders)
    self._to_all_bold()

    if self.verbose:
      self._get_doc_contents(self.destination_id, save=True)

    print("Done, please review the document before sharing it with clients.")

  def _initialize_service(self, credentials):
    """Initializes the Google Docs API services.

    Args:
      credentials: OAuth2Credentials user credentials.
        The path to the user's credentials.

    Returns:
      googleapiclient.discovery.Resource Doc API service, v1.
    """
    http = credentials.authorize(httplib2.Http())
    return discovery.build("docs", "v1", http=http)

  def _get_credentials(self, credentials_file_path, client_token_file_path):
    """ Gets valid user credentials from storage. If nothing has been stored, or
    if the stored credentials are invalid, the OAuth2 flow is completed to
    obtain the new credentials.

    When running in the buildbot, uses LUCI credentials instead.

    Args:
      credentials_file_path: str
        Absolute path to read credentials.json.
      client_token_file_path: str
        Absolute path to read/save user secret token.

    Returns:
      OAuth2Credentials The obtained user credentials.
    """
    if luci_auth.available():
      return luci_auth.LUCICredentials(scopes=[self.SCOPES])

    store = Storage(os.path.join(SRC_DIR, client_token_file_path))
    credentials = store.get()

    if not credentials or credentials.invalid:
      flow = client.flow_from_clientsecrets(
          os.path.join(SRC_DIR, credentials_file_path), self.SCOPES)
      flow.user_agent = self.APPLICATION_NAME
      flags = tools.argparser.parse_args([])
      credentials = tools.run_flow(flow, store, flags)
      print("Storing credentials to " + credentials_file_path)
    return credentials

  def _get_doc_contents(self, document_id, save=False):
    document = self._docs_service.documents().get(
        documentId=document_id).execute()
    if save:
      with open(os.path.join(SRC_DIR,
      "tools/traffic_annotation/scripts/template.json"), "w") as out_file:
        json.dump(document, out_file)
      print("Saved template.json.")

    if self.verbose:
      print(document)
    return document

  def _update_chrome_version(self, doc):
    """Gets the chrome version (MAJOR.MINOR.BUILD.PATCH) from src/chrome/VERSION
    and updates the doc to reflect the correct version.
    """
    version = ""
    with open(os.path.join(SRC_DIR, "chrome/VERSION"), "r") as version_file:
      version = ".".join(line.strip().split("=")[1]
                         for line in version_file.readlines())

    current_version = generator_utils.find_chrome_browser_version(doc)
    replacement = "Chrome Browser version {}".format(version)
    target = "Chrome Browser version {}".format(current_version)

    if replacement == target:
      print("Document chrome version is already up to date.")
      return

    req = [{
        "replaceAllText": {
            "containsText": {
                "text": target,
                "matchCase": True
            },
            "replaceText": replacement
        }
    }]
    self._perform_requests(req)
    print("Updated document chrome version {} --> {}".format(
        current_version, version))

  def _clear_destination_contents(self, doc):
    """Will clear the contents of the destination document from the end of the
    "Introduction" section onwards.

    Return: Integer of where to start writing, i.e. the index.
    """
    print("Overwriting the destination document.")
    first_index = generator_utils.find_first_index(doc)
    last_index = generator_utils.find_last_index(doc)

    if self.verbose:
      print("First index, last index", first_index, last_index)

    if first_index >= last_index:
      print("Nothing to overwrite.")
      return first_index

    req = [{
        "deleteContentRange": {
            "range": {
                "startIndex": first_index,
                "endIndex": last_index
            }
        }
    }]
    self._perform_requests(req)
    return first_index

  def _perform_requests(self, reqs):
    """Performs the requests |reqs| using batch update.
    """
    if not reqs:
      print("Warning, no requests provided. Returning.")
      return

    status = self._docs_service.documents().batchUpdate(
        body={
            "requests": reqs
        }, documentId=self.destination_id, fields="").execute()
    if self.verbose:
      print("#"*30)
      print(status)
      print("#"*30)
    return status

  def _insert_placeholders(self, placeholders):
    """Placeholders (e.g. groups, senders, traffic annotations) are inserted in
    the document in their order of appearance.

    Increment the self.index value to ensure that placeholders are inserted at
    the correct locations. Because placeholders are sorted in order of
    appearance, self.index is strictly increasing.
    """
    reqs = []
    for placeholder in placeholders:
      placeholder_type = placeholder["type"]

      if placeholder_type == Placeholder.ANNOTATION:
        req, index = self._create_annotation_request(
            placeholder["traffic_annotation"],
            self.index,
            color=self._color_bool)
        self._color_bool = not self._color_bool
      else:
        # is either a group or sender placeholder
        req, index = self._create_group_or_sender_request(
            placeholder["name"], self.index, placeholder_type)

      reqs += req
      self.index += index

    status = self._perform_requests(reqs)
    print("Added all {} placeholders!\n".format(len(placeholders)))

  def _create_text_request(self, text, index):
    """
    Returns:
      The request to insert raw text without formatting and the length of the
      text for appropriately incrementing |self.index|.
    """
    return {
        "insertText": {
          "location": {"index": index},
          "text": text
        }
    }, len(text)

  def _format_text(self, start_index, end_index, placeholder_type):
    """Format the text in between |start_index| and |end_index| using the styles
    specified by |generator_utils.PLACEHOLDER_STYLES|.

    Returns: The request to format the text in between |start_index| and
      |end_index|.
    """
    return {
        "updateTextStyle": {
            "range": {
                "startIndex": start_index,
                "endIndex": end_index
            },
            "textStyle": {
                "bold": PLACEHOLDER_STYLES[placeholder_type]["bold"],
                "fontSize": {
                    "magnitude":
                    PLACEHOLDER_STYLES[placeholder_type]["fontSize"],
                    "unit": "PT"
                },
                "weightedFontFamily": {
                    "fontFamily": PLACEHOLDER_STYLES[placeholder_type]["font"],
                    "weight": 400
                }
            },
            "fields": "*"
        }
    }

  def _create_group_or_sender_request(self, text, index, placeholder_type):
    """Returns the request for inserting the group or sender placeholders using
    the styling of |generator_utils.PLACEHOLDER_STYLES|.
    """
    assert placeholder_type in [Placeholder.GROUP, Placeholder.SENDER]
    text += "\n"
    req, idx = self._create_text_request(text, index)
    reqs = [req]
    reqs.append({
        "updateParagraphStyle": {
            "range": {
                "startIndex": index,
                "endIndex": index + idx
            },
            "paragraphStyle": {
                "namedStyleType":
                PLACEHOLDER_STYLES[placeholder_type]["namedStyleType"],
                "direction": "LEFT_TO_RIGHT",
                "spacingMode": "NEVER_COLLAPSE",
                "spaceAbove": {"unit": "PT"}
            },
            "fields": "*"
        }
    })
    reqs.append(self._format_text(index, index + idx, placeholder_type))
    return reqs, idx

  def _create_annotation_request(self, traffic_annotation, index, color=False):
    """Returns the request (dict) for inserting the annotations table. Refer to
    the template document for a visual.

    Args:
      traffic_annotation: generator_utils.TrafficAnnotation
        The TrafficAnnotation object with all the relevant information, e.g.
        unique_id, description, etc.
      index: int
        Where the annotation should be added in the document.
      color: bool
        If True, make the table blue, otherwise white.
    """
    # Hardcoded due to intrinsic of tables in Google Docs API.
    idx = 8
    offset = 2

    # Create the 1x2 table -- col 1 contains the unique_id placeholder, col 2
    # contains the remaining placeholders, e.g. trigger, description, etc.
    padding_req, _ = self._create_text_request("\n", index)
    reqs = [padding_req]
    reqs.append({
        "insertTable": {
            "rows": 1,
            "columns": 2,
            "location": {"index": index}
        }
    })

    # Writing the annotation's relevant information directly to the table,
    # within the left cell |left_text| and the right cell |right_text|.
    left_text = traffic_annotation.unique_id
    right_text = "{}\nTrigger: {}\nData: {}\nSettings: {}\nPolicy: {}".format(
        traffic_annotation.description, traffic_annotation.trigger,
        traffic_annotation.data, traffic_annotation.settings,
        traffic_annotation.policy)

    # +4 hardcoded due to intrinsic of tables in Google Docs API.
    start_index = index + 4
    left_req, left_increment = self._create_text_request(left_text, start_index)
    right_req, right_increment = self._create_text_request(
        right_text, start_index + left_increment + offset)

    reqs.append(left_req)
    reqs.append(right_req)

    end_index = index + left_increment + right_increment + idx

    # This sizes the table correctly such as making the right cell's width
    # greater than that of the left cell.
    col_properties = [{
        "columnIndices": [0],
        "width": 153
    }, {
        "columnIndices": [1],
        "width": 534
    }]
    for properties in col_properties:
      reqs.append({
          "updateTableColumnProperties": {
              "tableStartLocation": {"index": index + 1},
              "columnIndices": properties["columnIndices"],
              "fields": "*",
              "tableColumnProperties": {
                  "widthType": "FIXED_WIDTH",
                  "width": {
                      "magnitude": properties["width"],
                      "unit": "PT"
                  }
              }
          }
      })

    # Changing the table's color and ensuring that the borders are "turned off"
    # (really they're given the same color as the background).
    color = self.BLUE if color else self.WHITE
    color_and_border_req = {
        "updateTableCellStyle": {
            "tableStartLocation": {"index": index + 1},
            "fields": "*",
            "tableCellStyle": {
                "rowSpan": 1,
                "columnSpan": 1,
                "backgroundColor": {
                    "color": {
                        "rgbColor": color
                    }
                }
            }
        }
    }
    # make the table borders 'invisible' and adjust the padding to site text in
    # the cell correctly.
    for direction in ["Left", "Right", "Top", "Bottom"]:
      color_and_border_req["updateTableCellStyle"]["tableCellStyle"][
          "border" + direction] = {
              "color": {"color": {"rgbColor": color}},
              "width": {"unit": "PT"},
              "dashStyle": "SOLID"
          }
      color_and_border_req["updateTableCellStyle"]["tableCellStyle"][
          "padding" + direction] = {"magnitude": 1.44, "unit": "PT"}
    reqs.append(color_and_border_req)

    # Text formatting (normal text, linespacing, etc.) adds space below the
    # lines within a cell.
    reqs.append({
        "updateParagraphStyle": {
            "range": {
                "startIndex": start_index,
                "endIndex": end_index - 1
            },
            "paragraphStyle": {
                "namedStyleType": "NORMAL_TEXT",
                "lineSpacing": 100,
                "direction": "LEFT_TO_RIGHT",
                "spacingMode": "NEVER_COLLAPSE",
                "spaceBelow": {"magnitude": 4, "unit": "PT"},
                "avoidWidowAndOrphan": False
            },
            "fields": "*"
        }
    })
    reqs.append(
        self._format_text(start_index, end_index - 1, Placeholder.ANNOTATION))
    return reqs, end_index - index

  def _to_bold(self, start_index, end_index):
    """Bold the text between start_index and end_index. Uses the same formatting
    as the annotation bold."""
    return self._format_text(start_index, end_index,
                             Placeholder.ANNOTATION_BOLD)

  def _to_all_bold(self):
    """Bold the unique_id, description, trigger, etc. in the tables to
    correspond exactly to the template."""
    # Get recent doc after all the substitutions with the annotations. At this
    # point, document has all the content.
    print("Finding everything to bold...")
    doc = self._get_doc_contents(self.destination_id)

    # the ranges to bold using the updateTextStyle request
    bold_ranges = generator_utils.find_bold_ranges(doc)
    reqs = []
    for i, (start_index, end_index) in enumerate(bold_ranges):
      if end_index > start_index:
        reqs.append(self._to_bold(start_index, end_index))
    self._perform_requests(reqs)


def print_config_help():
  print("The config.json file should have the following items:\n"
        "doc_id:\n"
        "  ID of the destination document.\n"
        "doc_name:\n"
        "  Name of the document.\n"
        "credentials_file_path:\n"
        "  Absolute path of the file that keeps user credentials.\n"
        "client_token_file_path:\n"
        "  Absolute path of the token.pickle which keeps the users credentials."
        "  The file can be created as specified in:\n"
        "  https://developers.google.com/docs/api/quickstart/python")


def main():
  args_parser = argparse.ArgumentParser(
      description="Updates 'Chrome Browser Network Traffic Annotations' doc.")
  args_parser.add_argument("--config-file", help="Configurations file.")
  args_parser.add_argument("--annotations-file",
                           help="TSV annotations file exported from auditor.")
  args_parser.add_argument("--verbose",
                           action="store_true",
                           help="Reports all updates. "
                           " Also creates a scripts/template.json file "
                           " outlining the document's current structure.")
  args_parser.add_argument("--config-help",
                           action="store_true",
                           help="Shows the configurations help.")
  args = args_parser.parse_args()

  if args.config_help:
    print_config_help()
    return 0

  # Load and parse config file.
  with open(os.path.join(SRC_DIR, args.config_file)) as config_file:
    config = json.load(config_file)

  tsv_contents = load_tsv_file(
    os.path.join(SRC_DIR, args.annotations_file), False)
  if not tsv_contents:
    print("Could not read annotations file.")
    return -1

  xml_parser = XMLParser(
      os.path.join(SRC_DIR, "tools/traffic_annotation/summary/grouping.xml"),
      map_annotations(tsv_contents))
  placeholders = xml_parser.build_placeholders()
  print("#" * 40)
  print("There are:", len(placeholders), "placeholders")
  if args.verbose:
    print(placeholders)
  print("#" * 40)

  network_traffic_doc = NetworkTrafficAnnotationsDoc(
      doc_id=config["doc_id"],
      doc_name=config["doc_name"],
      credentials_file_path=config["credentials_file_path"],
      client_token_file_path=config["client_token_file_path"],
      verbose=args.verbose)

  if not network_traffic_doc.update_doc(placeholders):
    return -1

  return 0


if __name__ == "__main__":
  sys.exit(main())
