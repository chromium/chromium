#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import datetime
import hashlib
import logging
import os
import re
import sys

import extract_histograms
import merge_xml

_DATE_FILE_RE = re.compile(r".*MAJOR_BRANCH_DATE=(.+).*")
_CURRENT_MILESTONE_RE = re.compile(r"MAJOR=([0-9]{2,3})\n")
_MILESTONE_EXPIRY_RE = re.compile(r"\AM([0-9]{2,3})")

_SCRIPT_NAME = "generate_expired_histograms_array.py"
_HASH_DATATYPE = "uint64_t"
_HEADER = """// Generated from {script_name}. Do not edit!

#ifndef {include_guard}
#define {include_guard}

#include <stdint.h>

namespace {namespace} {{

// Contains hashes of expired histograms.
const {hash_datatype} kExpiredHistogramsHashes[] = {{
{hashes}
}};

const size_t kNumExpiredHistograms = {hashes_size};

}}  // namespace {namespace}

#endif  // {include_guard}
"""

_DATE_FORMAT_ERROR = "Unable to parse expiry {date} in histogram {name}."

# Some extra "grace" time is given to expired histograms during which they
# will contintue to be collected and reported.  The dashboard should ignore
# data from this period making the expiry noticeable and giving time for
# owners to re-enable them without any discontinuity of data. Releases are
# geneally 6 weeks apart but sometimes 7 so +2 to be safe.
_EXPIRE_GRACE_MSTONES = 2
_EXPIRE_GRACE_WEEKS = _EXPIRE_GRACE_MSTONES * 6 + 2


class Error(Exception):
  pass


def _GetExpiredHistograms(histograms, base_date, current_milestone):
  """Filters histograms to find expired ones if date format is used.

  Args:
    histograms(Dict[str, Dict]): Histogram descriptions in the form
      {name: content}.
    base_date(datetime.date): A date to check expiry dates against.

  Returns:
    List of strings with names of expired histograms.

  Raises:
    Error if there is an expiry date that doesn't match expected format.
  """
  expired_histograms_names = []
  for name, content in histograms.items():
    if "obsolete" in content or "expires_after" not in content:
      continue
    expiry_str = content["expires_after"]
    if expiry_str == "never":
      continue

    match = _MILESTONE_EXPIRY_RE.search(expiry_str)
    if match:
      # if there is match then expiry is in Chrome milsetone format.
      if int(match.group(1)) < current_milestone:
        expired_histograms_names.append(name)
    else:
      # if no match then we try the date format.
      try:
        expiry_date = datetime.datetime.strptime(
            expiry_str, extract_histograms.EXPIRY_DATE_PATTERN).date()
      except ValueError:
        raise Error(_DATE_FORMAT_ERROR.
                    format(date=expiry_str, name=name))
      if expiry_date < base_date:
        expired_histograms_names.append(name)
  return expired_histograms_names


def _FindMatch(content, regex, group_num):
  match_result = regex.search(content)
  if not match_result:
    raise Error("Unable to match {pattern} with provided content: {content}".
                format(pattern=regex.pattern, content=content))
  return match_result.group(group_num)


def _GetBaseDate(content, regex):
  """Fetches base date from |content| to compare expiry dates with.

  Args:
   content: A string with the base date.
   regex: A regular expression object that matches the base date.

  Returns:
   A base date as datetime.date object.

  Raises:
    Error if |content| doesn't match |regex| or the matched date has invalid
    format.
  """
  base_date_str = _FindMatch(content, regex, 1)
  if not base_date_str:
    return None
  try:
    base_date = datetime.datetime.strptime(
        base_date_str, extract_histograms.EXPIRY_DATE_PATTERN).date()
    return base_date
  except ValueError:
    raise Error("Unable to parse base date {date} from {content}.".
                format(date=base_date_str, content=content))


def _GetCurrentMilestone(content, regex):
  """Extracts current milestone from |content|.

  Args:
   content: A string with the version information.
   regex: A regular expression object that matches milestone.

  Returns:
   A milestone  as int.

  Raises:
    Error if |content| doesn't match |regex|.
  """
  return int(_FindMatch(content, regex, 1))


def _HashName(name):
  """Returns hash for the given histogram |name|."""
  return "0x" + hashlib.md5(name.encode()).hexdigest()[:16]


def _GetHashToNameMap(histograms_names):
  """Returns dictionary {hash: histogram_name}."""
  hash_to_name_map = dict()
  for name in histograms_names:
    hash_to_name_map[_HashName(name)] = name
  return hash_to_name_map


def _GenerateHeaderFileContent(header_filename, namespace,
                               histograms_map):
  """Generates header file content.

  Args:
    header_filename: A filename of the generated header file.
    namespace: A namespace to contain generated array.
    histograms_map(Dict[str, str]): A dictionary {hash: histogram_name}.

  Returns:
    String with the generated content.
  """
  include_guard = re.sub("[^A-Z]", "_", header_filename.upper()) + "_"
  if not histograms_map:
    # Some platforms don't allow creating empty arrays.
    histograms_map["0x0000000000000000"] = "Dummy.Histogram"
  hashes = "\n".join([
      "  {hash},  // {name}".format(hash=value, name=histograms_map[value])
      for value in sorted(histograms_map.keys())
  ])
  return _HEADER.format(
      script_name=_SCRIPT_NAME,
      include_guard=include_guard,
      namespace=namespace,
      hash_datatype=_HASH_DATATYPE,
      hashes=hashes,
      hashes_size=len(histograms_map))


def _GenerateFileContent(descriptions, branch_file_content,
                         mstone_file_content, header_filename, namespace):
  """Generates header file containing array with hashes of expired histograms.

  Args:
    descriptions: Combined histogram descriptions.
    branch_file_content: Content of file with base date.
    mstone_file_content: Content of file with milestone information.
    header_filename: A filename of the generated header file.
    namespace: A namespace to contain generated array.

  Raises:
    Error if there is an error in input xml files.
  """
  histograms, had_errors = (
      extract_histograms.ExtractHistogramsFromDom(descriptions))
  if had_errors:
    raise Error("Error parsing inputs.")
  base_date = _GetBaseDate(branch_file_content, _DATE_FILE_RE)
  base_date -= datetime.timedelta(weeks=_EXPIRE_GRACE_WEEKS)
  current_milestone = _GetCurrentMilestone(
      mstone_file_content, _CURRENT_MILESTONE_RE)
  current_milestone -= _EXPIRE_GRACE_MSTONES

  expired_histograms_names = _GetExpiredHistograms(
      histograms, base_date, current_milestone)
  expired_histograms_map = _GetHashToNameMap(expired_histograms_names)
  header_file_content = _GenerateHeaderFileContent(
      header_filename, namespace, expired_histograms_map)
  return header_file_content


def _GenerateFile(arguments):
  """Generates header file containing array with hashes of expired histograms.

  Args:
    arguments: An object with the following attributes:
      arguments.inputs: A list of xml files with histogram descriptions.
      arguments.header_filename: A filename of the generated header file.
      arguments.namespace: A namespace to contain generated array.
      arguments.output_dir: A directory to put the generated file.
      arguments.major_branch_date_filepath: File path for base date.
      arguments.milestone_filepath: File path for milestone information.
  """
  descriptions = merge_xml.MergeFiles(arguments.inputs)
  with open(arguments.major_branch_date_filepath, "r") as date_file:
    branch_file_content = date_file.read()
  with open(arguments.milestone_filepath, "r") as milestone_file:
    mstone_file_content = milestone_file.read()

  header_file_content = _GenerateFileContent(
      descriptions, branch_file_content, mstone_file_content,
      arguments.header_filename, arguments.namespace)

  with open(os.path.join(arguments.output_dir, arguments.header_filename),
            "w") as generated_file:
    generated_file.write(header_file_content)


def _ParseArguments():
  """Defines and parses arguments from the command line."""
  arg_parser = argparse.ArgumentParser(
      description="Generate array of expired histograms' hashes.")
  arg_parser.add_argument(
      "--output_dir",
      "-o",
      required=True,
      help="Base directory to for generated files.")
  arg_parser.add_argument(
      "--header_filename",
      "-H",
      required=True,
      help="File name of the generated header file.")
  arg_parser.add_argument(
      "--namespace",
      "-n",
      default="",
      help="Namespace of the generated factory function (code will be in "
      "the global namespace if this is omitted).")
  arg_parser.add_argument(
      "--major_branch_date_filepath",
      "-d",
      required=True,
      help="A path to the file with the base date.")
  arg_parser.add_argument(
      "--milestone_filepath",
      "-m",
      required=True,
      help="A path to the file with the milestone information.")
  arg_parser.add_argument(
      "inputs",
      nargs="+",
      help="Paths to .xml files with histogram descriptions.")
  return arg_parser.parse_args()


def main():
  arguments = _ParseArguments()
  _GenerateFile(arguments)


if __name__ == "__main__":
  sys.exit(main())
