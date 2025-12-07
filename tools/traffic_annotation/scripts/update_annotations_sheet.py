#!/usr/bin/env vpython3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script updates network traffic annotations sheet. To run the script, you
should first generate annotations.tsv using traffic_annotation_auditor, and then
call:
update_annotations_sheet --config=[settings.json] [path_to_annotations.tsv]

Run update_annotations_sheet --config-help for help on configuration file.

TODO(rhalavati): Add tests.
"""

from __future__ import print_function

import argparse
import csv
import datetime
import httplib2
import io
import json
import os
import re
import sys

from apiclient import discovery, http as googlehttp
from infra_libs import luci_auth
from oauth2client import client
from oauth2client import tools
from oauth2client.file import Storage
from generator_utils import load_tsv_file


SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
SRC_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "../../.."))


def GetCurrentChromeVersion():
  with io.open(os.path.join(SRC_DIR, "chrome/VERSION")) as f:
    contents = f.read()
  version_parts = dict(
      re.match(r"(\w+)=(\d+)", line).groups() for line in contents.split("\n")
      if line)
  return tuple(
      int(version_parts[part]) for part in ["MAJOR", "MINOR", "BUILD", "PATCH"])


def VersionTupleToString(version_tuple):
  return '.'.join(map(str, version_tuple))


class HttpRequestWithRetries(googlehttp.HttpRequest):
  """Same as HttpRequest, but all requests are retried up to 5 times."""

  def execute(self, http=None, num_retries=1):
    return super().execute(http=http, num_retries=5)


class SheetEditor():
  """Loads and updates traffic annotation's sheet."""

  # If modifying these scopes, delete your previously saved credentials.
  SCOPES = "https://www.googleapis.com/auth/spreadsheets"
  APPLICATION_NAME = "Chrome Network Traffic Annotations Spreadsheet Updater"

  def __init__(self, spreadsheet_id, annotations_sheet_name,
               chrome_version_sheet_name, silent_change_columns,
               last_update_column_name, credentials_file_path,
               client_secret_file_path, verbose):
    """ Initializes the SheetEditor. Please refer to 'PrintConfigHelp' function
    for description of input arguments.

    Args:
      spreadsheet_id: str
          ID of annotations spreadsheet.
      annotations_sheet_name: str
          Name of the sheet that contains the annotations.
      chrome_version_sheet_name: str
          Name of the sheet that contains the changes stats.
      silent_change_columns: list of str
          List of the columns whose changes are not reported in the stats.
      last_update_column_name: str
          Header of the column that keeps the latest update date.
      credentials_file_path: str
          Absolute path to read/save user credentials.
      client_secret_file_path: str
          Absolute path to read client_secret.json.
      verbose: bool
          Flag requesting dump of details of actions.
    """
    print("Getting credential to update annotations report.")
    self.service = self._InitializeService(
        self._GetCredentials(credentials_file_path, client_secret_file_path))
    print("Successfully got credential to update annotations report.")
    self.spreadsheet_id = spreadsheet_id
    self.annotations_sheet_name = annotations_sheet_name
    self.chrome_version_sheet_name = chrome_version_sheet_name
    self.silent_change_columns = silent_change_columns
    self.last_update_column_name = last_update_column_name
    self.annotations_sheet_id = self._GetAnnotationsSheetId()
    self.required_row_updates = []
    self.required_cell_updates = []
    self.delete_count = 0
    self.insert_count = 0
    self.update_count = 0
    self.verbose = verbose
    self.today = datetime.datetime.now().strftime("%m/%d/%Y")


  def _InitializeService(self, credentials):
    """ Initializes the Google Sheets API service.

    Args:
      credentials: OAuth2Credentials user credentials.

    Returns:
      googleapiclient.discovery.Resource Spreadsheet API service.
    """
    http = credentials.authorize(httplib2.Http())
    discoveryUrl = ("https://sheets.googleapis.com/$discovery/rest?version=v4")
    return discovery.build("sheets",
                           "v4",
                           http=http,
                           requestBuilder=HttpRequestWithRetries,
                           discoveryServiceUrl=discoveryUrl)


  def _GetCredentials(self, credentials_file_path, client_secret_file_path):
    """ Gets valid user credentials from storage. If nothing has been stored, or
    if the stored credentials are invalid, the OAuth2 flow is completed to
    obtain the new credentials.

    When running in the buildbot, uses LUCI credentials instead.

    Args:
      credentials_file_path: str Absolute path to read/save user credentials.
      client_secret_file_path: str Absolute path to read client_secret.json.

    Returns:
      OAuth2Credentials The obtained user credentials.
    """
    if luci_auth.available():
      return luci_auth.LUCICredentials(scopes=[self.SCOPES])

    store = Storage(credentials_file_path)
    credentials = store.get()
    if not credentials or credentials.invalid:
      flow = client.flow_from_clientsecrets(client_secret_file_path,
                                            self.SCOPES)
      flow.user_agent = self.APPLICATION_NAME
      flags = tools.argparser.parse_args([])
      credentials = tools.run_flow(flow, store, flags)
      print("Storing credentials to " + credentials_file_path)
    return credentials


  def _GetAnnotationsSheetId(self):
    """ Gets the id of the sheet containing annotations table.

    Returns:
      int Id of the sheet.
    """
    response = self.service.spreadsheets().get(
        spreadsheetId=self.spreadsheet_id,
        ranges=self.annotations_sheet_name,
        includeGridData=False).execute()
    return response["sheets"][0]["properties"]["sheetId"]


  def LoadAnnotationsSheet(self):
    """ Loads the sheet's content.

    Returns:
      list of list Table of annotations loaded from the trix.
    """
    result = self.service.spreadsheets().values().get(
        spreadsheetId=self.spreadsheet_id,
        range=self.annotations_sheet_name).execute()
    return result.get("values", [])


  def _CreateInsertRequest(self, row):
    self.required_row_updates.append(
        { "insertDimension": {
            "range": {
              "sheetId": self.annotations_sheet_id,
              "dimension": "ROWS",
              "startIndex": row, # 0 index.
              "endIndex": row + 1
            }
          }
        })
    self.insert_count += 1


  def _CreateAppendRequest(self, row_count):
    self.required_row_updates.append(
        { "appendDimension": {
            "sheetId": self.annotations_sheet_id,
            "dimension": "ROWS",
            "length": row_count
          }
        })
    self.insert_count += row_count


  def _CreateDeleteRequest(self, row):
    self.required_row_updates.append(
        { "deleteDimension": {
            "range": {
              "sheetId": self.annotations_sheet_id,
              "dimension": "ROWS",
              "startIndex": row,
              "endIndex": row + 1
            }
          }
        })
    self.delete_count += 1


  def _CreateUpdateRequest(self, row, column, value):
    # If having more than 26 columns, update cell_name.
    assert(column < 26)
    cell_name = "%s%i" % (chr(65 + column), 1 + row)
    self.required_cell_updates.append(
        { "range": "%s!%s:%s" % (
              self.annotations_sheet_name, cell_name, cell_name),
          "values": [[value]] })


  def GenerateUpdates(self, file_contents):
    """ Generates required updates to refresh the sheet, using the input file
    contents.

    Args:
      file_contents: list of list Table of annotations read from file. Each item
          represents one row of the annotation table, and each row is presented
          as a list of its column values.

    Returns:
      bool Flag specifying if everything was OK or not.
    """
    print("Generating updates for report.")
    sheet_contents = self.LoadAnnotationsSheet()
    if not sheet_contents:
      print("Could not read previous content.")
      return False

    headers = file_contents[0]
    silent_change_column_indices = []
    for title in self.silent_change_columns:
      if title not in headers:
        print("ERROR: Could not find %s column." % title)
        return False
      silent_change_column_indices.append(headers.index(title))

    last_update_column = headers.index(self.last_update_column_name)

    # Step 1: Compare old and new contents, generate add/remove requests so that
    # both contents would become the same size with matching unique ids (at
    # column 0).
    # Ignores header row (row 0).
    old_set = set(row[0] for row in sheet_contents[1:])
    new_set = set(row[0] for row in file_contents[1:])
    removed_ids = old_set - new_set
    added_ids = list(new_set - old_set)
    added_ids.sort()
    if self.verbose:
      for id in removed_ids:
        print("Deleted: %s" % id)
      for id in added_ids:
        print("Added: %s" % id)

    empty_row = [''] * len(file_contents[0])
    # Skip first row (it's the header row).
    row = 1
    while row < len(sheet_contents):
      row_id = sheet_contents[row][0]
      # If a row is removed, remove it from previous sheet.
      if row_id in removed_ids:
        self._CreateDeleteRequest(row)
        sheet_contents.pop(row)
        continue
      # If there are rows to add, and they should be before current row, insert
      # an empty row before current row. The empty row will be filled later.
      if added_ids and added_ids[0] < row_id:
        self._CreateInsertRequest(row)
        sheet_contents.insert(row, empty_row[:])
        added_ids.pop(0)
      row += 1

    # If there are still rows to be added, they should come at the end.
    if added_ids:
      self._CreateAppendRequest(len(added_ids))
      while added_ids:
        sheet_contents.append(empty_row[:])
        added_ids.pop()

    assert(len(file_contents) == len(sheet_contents))

    # Step 2: Compare cells of old and new contents, issue requests to update
    # cells with different values. Ignore headers row.
    for row in range(1, len(file_contents)):
      file_row = file_contents[row]
      sheet_row = sheet_contents[row]

      major_update = False
      for col in range(len(file_row)):
        # Ignore 'Last Update' column for now.
        if col == last_update_column:
          continue
        if file_row[col] != sheet_row[col]:
          self._CreateUpdateRequest(row, col, file_row[col])
          if self.verbose and sheet_row[0]:
            print("Updating: %s - %s" % (file_row[0], file_contents[0][col]))
          if col not in silent_change_column_indices:
            major_update = True
      # If there has been a change in a column that is not silently updated,
      # update the date as well.
      if major_update:
        self._CreateUpdateRequest(row, last_update_column, self.today)
        # If the row is not entirely new, increase the update count.
        if sheet_row[0]:
          self.update_count += 1
    return True


  def ApplyUpdates(self):
    """ Applies the updates stored in |self.required_row_updates| and
    |self.required_cell_updates| to the sheet.
    """
    # Insert/Remove rows.
    print("Applying updates for the report.")
    if self.required_row_updates:
      self.service.spreadsheets().batchUpdate(
          spreadsheetId=self.spreadsheet_id,
          body={"requests": self.required_row_updates}).execute()

    # Refresh Cells.
    if self.required_cell_updates:
      batch_update_values_request_body = {
        "value_input_option": "RAW",
        "data": self.required_cell_updates
      }
      self.service.spreadsheets().values().batchUpdate(
          spreadsheetId=self.spreadsheet_id,
          body=batch_update_values_request_body).execute()


  def GiveUpdateSummary(self):
    return "New annotations: %s, Modified annotations: %s, " \
           "Removed annotations: %s" % (
                self.insert_count, self.update_count, self.delete_count)


  def UpdateChromeVersion(self, version_tuple):
    self.service.spreadsheets().values().update(
        spreadsheetId=self.spreadsheet_id,
        range="%s!A1:A1" % self.chrome_version_sheet_name,
        valueInputOption="RAW",
        body={
            "values": [[VersionTupleToString(version_tuple)]]
        }).execute()

  def GetChromeVersionFromSheet(self):
    response = self.service.spreadsheets().values().get(
        spreadsheetId=self.spreadsheet_id,
        range="%s!A1:A1" % self.chrome_version_sheet_name).execute()
    version_string = response["values"][0][0]
    return tuple(int(part) for part in version_string.split('.'))


def PrintConfigHelp():
  print("The config.json file should have the following items:\n"
        "spreadsheet_id:\n"
        "  ID of annotations spreadsheet.\n"
        "annotations_sheet_name:\n"
        "  Name of the sheet that contains the annotations.\n"
        "chrome_version_sheet_name:\n"
        "  Name of the sheet that contains the Chrome version.\n"
        "silent_change_columns:\n"
        "  List of the columns whose changes don't affect the Last Update "
        "column.\n"
        "last_update_column_name:\n"
        "  Header of the column that keeps the latest update date.\n"
        "credentials_file_path:\n"
        "  Absolute path of the file that keeps user credentials.\n"
        "client_secret_file_path:\n"
        "  Absolute path of the file that keeps client_secret.json. The file\n"
        "  can be created as specified in:\n"
        "  https://developers.google.com/sheets/api/quickstart/python")


def main():
  parser = argparse.ArgumentParser(
      description="Network Traffic Annotations Sheet Updater")
  parser.add_argument(
      "--config-file",
      help="Configurations file.")
  parser.add_argument(
      "--annotations-file",
      help="TSV annotations file exported from auditor.")
  parser.add_argument(
      '--verbose', action='store_true',
      help='Reports all updates.')
  parser.add_argument('--yes',
                      action='store_true',
                      help='Performs all actions without confirmation.')
  parser.add_argument(
      '--force',
      action='store_true',
      help='Performs all actions without confirmation, regardless of the '
      'sheet being older or newer than this version. Implies --yes.')
  parser.add_argument(
      '--config-help', action='store_true',
      help='Shows the configurations help.')
  args = parser.parse_args()
  if args.force:
    args.yes = True

  print("Updating annotations sheet.")
  if args.config_help:
    PrintConfigHelp()
    return 0

  # Load and parse config file.
  with open(args.config_file) as config_file:
    config = json.load(config_file)

  # Load and parse annotations file.
  file_content = load_tsv_file(args.annotations_file, args.verbose)
  if not file_content:
    print("Could not read annotations file.")
    return -1

  sheet_editor = SheetEditor(
      spreadsheet_id=config["spreadsheet_id"],
      annotations_sheet_name=config["annotations_sheet_name"],
      chrome_version_sheet_name=config["chrome_version_sheet_name"],
      silent_change_columns=config["silent_change_columns"],
      last_update_column_name=config["last_update_column_name"],
      credentials_file_path=config.get("credentials_file_path", None),
      client_secret_file_path=config.get("client_secret_file_path", None),
      verbose=args.verbose)

  current_version = GetCurrentChromeVersion()
  current_version_string = VersionTupleToString(current_version)
  print("This is Chrome version %s" % current_version_string)

  sheet_version = sheet_editor.GetChromeVersionFromSheet()
  sheet_version_string = VersionTupleToString(sheet_version)
  print("Sheet contains Chrome version %s" % sheet_version_string)

  if sheet_version > current_version and not args.force:
    print("Sheet is already newer than this Chrome version. Aborting.")
    return 0

  if not sheet_editor.GenerateUpdates(file_content):
    print("Error generating updates for file content.")
    return -1

  main_sheet_needs_update = (sheet_editor.required_cell_updates
                             or sheet_editor.required_row_updates)
  version_needs_update = current_version != sheet_version

  if main_sheet_needs_update or version_needs_update:
    if main_sheet_needs_update:
      print("%s" % sheet_editor.GiveUpdateSummary())
    else:
      print("No updates to annotations required.")

    if current_version != sheet_version:
      print("The '%s' sheet will be updated to '%s'." %
            (sheet_editor.chrome_version_sheet_name, current_version_string))

    if not args.yes:
      print("Proceed with update?")
      if raw_input("(Y/n): ").strip().lower() != "y":
        return -1

    if main_sheet_needs_update:
      sheet_editor.ApplyUpdates()
    if version_needs_update:
      sheet_editor.UpdateChromeVersion(current_version)
    print("Updates applied.")

  else:
    print("No updates required.")

  return 0


if __name__ == "__main__":
  sys.exit(main())
