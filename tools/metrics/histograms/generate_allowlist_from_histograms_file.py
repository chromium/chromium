#!/usr/bin/env python
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys

import extract_histograms
import xml.dom.minidom

_SCRIPT_NAME = "generate_allowlist_from_histograms_file.py"
_FILE = """// Generated from {script_name}. Do not edit!

#ifndef {include_guard}
#define {include_guard}

#include <algorithm>
#include <string_view>

namespace {namespace} {{

inline constexpr std::string_view k{allow_list_name}AllowList[] = {{
{values}
}};

constexpr bool IsValid{allow_list_name}(std::string_view s) {{
  return std::binary_search(
    std::cbegin(k{allow_list_name}AllowList),
    std::cend(k{allow_list_name}AllowList),
    s);
}}

}}  // namespace {namespace}

#endif  // {include_guard}
"""


class Error(Exception):
  pass


def _GenerateStaticFile(file_path, namespace, values, allow_list_name):
  """Generates a header file with constexpr facilities to check for the
    existence of a variant or enum in a histograms.xml file.

  Args:
    namespace: A namespace to contain generated array.
    values(List[str|int]]): A list of variant or enum values.
    allow_list_name: A name of the variant list for an allow list.
  Returns:
    String with the generated header file content.
  """
  values = sorted(values, key=lambda d: str(d))
  include_guard = file_path.replace('\\', '_').replace('/', '_').replace(
      '.', '_').upper() + "_"

  values_string = "\n".join(
      ["  \"{name}\",".format(name=value) for value in values])
  return _FILE.format(script_name=_SCRIPT_NAME,
                      include_guard=include_guard,
                      namespace=namespace,
                      values=values_string,
                      allow_list_name=allow_list_name)


def _GenerateValueList(histograms, tag, allow_list_name):
  if tag == "variant":
    values, had_errors = extract_histograms.ExtractVariantsFromXmlTree(
        histograms)
  elif tag == "enum":
    values, had_errors = extract_histograms.ExtractEnumsFromXmlTree(histograms)
  else:
    raise Error("'tag' must be either 'variant' or 'enum'")

  if had_errors:
    raise Error("Error parsing inputs.")

  if (allow_list_name not in values):
    raise Error("AllowListName is missing in variants list")

  if tag == "variant":
    return [value["name"] for value in values[allow_list_name]]
  else:
    return list(values[allow_list_name]["values"].keys())


def _GenerateFile(arguments):
  """Generates C++ header file containing values of a variant or enum from
  a .xml file.

  Args:
    arguments: An object with the following attributes:
      arguments.input: An xml file with histogram descriptions.
      arguments.file: A filename of the generated source file.
      arguments.tag: A XML tag, can be "enum" or "variant".
      arguments.namespace: A namespace to contain generated array.
      arguments.output_dir: A directory to put the generated file.
      arguments.allow_list_name: A name of the variant or enum list.
  """
  histograms = xml.dom.minidom.parse(arguments.input)
  values = _GenerateValueList(histograms, arguments.tag,
                              arguments.allow_list_name)

  static_check_header_file_content = _GenerateStaticFile(
      arguments.file, arguments.namespace, values, arguments.allow_list_name)
  with open(os.path.join(arguments.output_dir, arguments.file),
            "w") as generated_file:
    generated_file.write(static_check_header_file_content)


def _ParseArguments():
  """Defines and parses arguments from the command line."""
  arg_parser = argparse.ArgumentParser(
      description="Generate an array of allowlist from a histograms.xml file."
  )
  arg_parser.add_argument("--output_dir",
                          required=True,
                          help="Base directory to for generated files.")
  arg_parser.add_argument("--file",
                          required=True,
                          help="File name of the generated file.")
  arg_parser.add_argument(
      "--allow_list_name",
      required=True,
      help="Name of the variant / enum list in the histograms.xml file.")
  arg_parser.add_argument("--namespace",
                          required=True,
                          help="Namespace of the allow list array.")
  arg_parser.add_argument("--tag",
                          required=True,
                          help="XML tag name of either 'enum' or 'variant'.")
  arg_parser.add_argument("--input",
                          help="Path to .xml file with histogram descriptions.")
  return arg_parser.parse_args()


def main():
  arguments = _ParseArguments()
  _GenerateFile(arguments)


if __name__ == "__main__":
  sys.exit(main())
