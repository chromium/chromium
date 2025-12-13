#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import datetime
import json
import os
import subprocess
import sys

################################################################################
# Utils

def count_matching_files(abs_directory,
                         include_file_content_strings=None,
                         content_match_strings=None,
                         include_filename_strings=None,
                         exclude_filename_strings=None):
  """
    Returns the number of files under `abs_directory` that match the include and
    exclude criteria. It will also return a sum of the content matches these
    files have with the `content_match_strings` list.

    Args:
        abs_directory (str): The absolute directory to search in.
        include_file_content_strings (list, optional): List of strings. A file
                                                       must contain at least one
                                                       of these (if defined) in
                                                       order to be included in
                                                       the count.
        content_match_strings (list, optional): List of strings. Files that pass
                                                the match criteria will also
                                                have their content checked for
                                                matches in this list. Any
                                                matches will contribute to the
                                                sum of content string matches
                                                returned.
        include_filename_strings (list, optional): List of strings. All must be
                                                   present in the filename.
        exclude_filename_strings (list, optional): List of strings. If any are
                                                   present in the filename, the
                                                   file is excluded.

    Returns:
        tuple: The count of files passing the include / exclude criteria, and
               the number of times `content_match_strings` are found within
               these files.
    """

  if not os.path.isdir(abs_directory):
    print(f"Error: Directory '{abs_directory}' not found.")
    return 0, 0

  items_to_scan = []
  for dirpath, _, filenames in os.walk(abs_directory):
    for filename in filenames:
      items_to_scan.append(os.path.join(dirpath, filename))

  matched_file_count = 0
  string_matches_count = 0
  for filepath in items_to_scan:
    filename_only = os.path.basename(filepath)

    # 1. Filename include filter.
    if include_filename_strings:
      if not all(inc_str in filename_only
                 for inc_str in include_filename_strings):
        continue  # Skip if not all include_filename_strings are present.

    # 2. Extension include filter.
    if not any(inc_str in filename_only for inc_str in [".h", ".cc", ".mm"]):
      continue  # Skip if not a valid source file.

    # 3. Filename exclude filter.
    if exclude_filename_strings:
      if any(exc_str in filename_only for exc_str in exclude_filename_strings):
        continue  # Skip if any exclude_filename_strings are present.

    # Open the matching file for steps 4 and 5.
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
      content = f.read()

    # 4. File content filter.
    if include_file_content_strings:
      if not any(s_str in content for s_str in include_file_content_strings):
        continue  # Skip if no include strings to match.
    matched_file_count += 1

    # 5. Content string counts.
    if content_match_strings:
      file_string_matches_count = 0
      for s_str in content_match_strings:
        if s_str:  # Avoid issues with empty search strings.
          file_string_matches_count += content.count(s_str)
      string_matches_count += file_string_matches_count

  return matched_file_count, string_matches_count


def count_lines(filenames):
  """
    Counts number of lines in the list of `filenames`.

    Args:
        filenames (list): A list of absolute paths to the files.

    Returns:
        int: The total line count for all `filenames`.
    """
  total_count = 0
  for filename in filenames:
    with open(filename, 'r') as file:
      lines = file.readlines()
      total_count += len(lines)
  return total_count


def get_shell_metrics(command_str):
  """
    Runs a shell command and returns the output as an int.

    Args:
        command_str (str): The shell command to run.

    Returns:
        int: The output cast to an int.
    """
  result = subprocess.run(command_str,
                          shell=True,
                          capture_output=True,
                          text=True,
                          check=True)

  # Get the standard output and strip any leading/trailing whitespace (like
  # newlines).
  output_str = result.stdout.strip()

  # Convert the cleaned string to an integer.
  number = int(output_str)

  return number


def count_browser_owned_objects(abs_directory):
  """
    Returns a count of feature instances directly owned by Browser.

    Args:
        abs_directory (str): The absolute directory to search in.

    Returns:
        int: The count of BrowserUserData instances.
    """
  count = 0
  with open(os.path.join(abs_directory, "ui/browser.h"), 'r') as f:
    for line in f:
      # Do not match KeepAlive instances or any lines that may be part of a
      # method signature.
      if any(el in line for el in ["KeepAlive", ",", "("]):
        continue

      # Explicitly include BookmarkBar::State and SigninViewController features.
      if any(el in line for el in [
          "std::unique_ptr<", "BookmarkBar::State bookmark_bar_state_",
          "SigninViewController signin_view_controller_"
      ]):
        count += 1

  return count


def count_browser_user_data(abs_directory):
  """
    Returns a count of the number of BrowserUserData instances.

    Args:
        abs_directory (str): The absolute directory to search in.

    Returns:
        int: The count of BrowserUserData instances.
    """
  total_count = 0
  for root, _, files in os.walk(abs_directory):
    for filename in files:
      file_path = os.path.join(root, filename)
      with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
        total_count += f.read().count('public BrowserUserData<')
  return total_count


################################################################################
# Metrics Calculation

def main():
  parser = argparse.ArgumentParser(
      description="Produces project Bedrock metrics.")
  parser.add_argument("src_directory", type=str, help="Input directory path.")
  parser.add_argument("build_directory",
                      type=str,
                      help="Build directory relative to src.")
  parser.add_argument("output_file",
                      type=str,
                      help="Path to the output JSON file.")
  args = parser.parse_args()

  # Store the chrome/browser directory as an absolute path.
  abs_cb_directory = os.path.abspath(
      os.path.join(args.src_directory, "./chrome/browser"))

  # Record the git commit hash and datetime.
  commit_hash = subprocess.run("git rev-parse HEAD",
                               shell=True,
                               capture_output=True,
                               text=True,
                               check=True).stdout.strip()
  git_timestamp = subprocess.run("git show -s --format=%cI",
                                 shell=True,
                                 capture_output=True,
                                 text=True,
                                 check=True).stdout.strip()
  timestamp = datetime.datetime.fromisoformat(git_timestamp).strftime(
      "%Y-%m-%d %H:%M:%S")

  # Calculate the use of Browser fixtures and utilities in unit tests.
  unittest_ref_files, unittest_ref_matches = count_matching_files(
      abs_cb_directory, [
          "TestBrowserWindow", "BrowserWithTestWindowTest",
          "TestWithBrowserView", "CreateBrowserWithTestWindowForParams"
      ], [
          "TestBrowserWindow", "BrowserWithTestWindowTest",
          "TestWithBrowserView", "CreateBrowserWithTestWindowForParams",
          "Browser*", "raw_ptr<Browser>", "BrowserView*",
          "raw_ptr<BrowserView>", "browser_view()", "GetBrowserView("
      ], ["unittest"], None)
  unittest_total, _ = count_matching_files(abs_cb_directory, None, None,
                                           ["unittest"], None)

  # Calculate the use of Browser and BrowserView in production code.
  production_ref_files, production_ref_matches = count_matching_files(
      abs_cb_directory, [
          "Browser*", "raw_ptr<Browser>", "BrowserView*",
          "raw_ptr<BrowserView>", "browser_view()", "GetBrowserView("
      ], [
          "Browser*", "raw_ptr<Browser>", "BrowserView*",
          "raw_ptr<BrowserView>", "browser_view()", "GetBrowserView("
      ], None, ["test"])
  production_total, _ = count_matching_files(abs_cb_directory, None, None, None,
                                             ["test"])

  # Calculate total LOC for both Browser and BrowserView.
  browser_lc = count_lines([
      os.path.join(abs_cb_directory, "./ui/browser.h"),
      os.path.join(abs_cb_directory, "./ui/browser.cc")
  ])
  browser_view_lc = count_lines([
      os.path.join(abs_cb_directory, "./ui/views/frame/browser_view.h"),
      os.path.join(abs_cb_directory, "./ui/views/frame/browser_view.cc")
  ])

  # Calculate number of sources for both :browser and :ui targets.
  browser_sources = get_shell_metrics(
      f"gn desc {args.build_directory} chrome/browser:browser sources | wc -l")
  browser_ui_sources = get_shell_metrics(
      f"gn desc {args.build_directory} chrome/browser/ui:ui sources | wc -l")

  # Calculate number of circular references for both :browser and :ui targets.
  browser_sources_circular = get_shell_metrics(
      f"gn desc {args.build_directory} chrome/browser:browser "
      "allow_circular_includes_from | xargs -I{} gn desc out/Default {} "
      "sources | wc -l")
  browser_ui_sources_circular = get_shell_metrics(
      f"gn desc {args.build_directory} chrome/browser/ui:ui "
      "allow_circular_includes_from | xargs -I{} gn desc out/Default {} "
      "sources | wc -l")

  # Calculate the number of features / classes directly owned by Browser.
  browser_owned_data = count_browser_owned_objects(
      abs_cb_directory) + count_browser_user_data(abs_cb_directory)

  # Define the JSON data.
  data_to_write = {
      "commit_hash": commit_hash,
      "timestamp": timestamp,
      "unittest_ref_files": unittest_ref_files,
      "unittest_ref_matches": unittest_ref_matches,
      "unittest_total": unittest_total,
      "production_ref_files": production_ref_files,
      "production_ref_matches": production_ref_matches,
      "production_total": production_total,
      "browser_lc": browser_lc,
      "browser_view_lc": browser_view_lc,
      "browser_sources": browser_sources,
      "browser_ui_sources": browser_ui_sources,
      "browser_sources_circular": browser_sources_circular,
      "browser_ui_sources_circular": browser_ui_sources_circular,
      "browser_owned_data": browser_owned_data,
  }

  # Write the JSON data to the output file.
  try:
    with open(args.output_file, 'w') as f:
      json.dump(data_to_write, f)
    print(f"Successfully wrote JSON to '{args.output_file}'")
  except IOError as e:
    print(f"Error: Could not write to file '{args.output_file}': {e}",
          file=sys.stderr)
    sys.exit(1)
  except Exception as e:
    print(f"An unexpected error occurred: {e}", file=sys.stderr)
    sys.exit(1)

if __name__ == "__main__":
  main()
