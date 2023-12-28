#!/usr/bin/env python
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys

import extract_histograms
import xml.dom.minidom

_SCRIPT_NAME = "generate_histograms_variants_allowlist.py"
_FILE = """// Generated from {script_name}. Do not edit!

#ifndef {include_guard}
#define {include_guard}

#include <algorithm>
#include <string_view>

namespace {namespace} {{

inline constexpr std::string_view k{variant_name}VariantAllowList[] = {{
{variants}
}};

constexpr bool IsValid{variant_name}Variant(std::string_view s) {{
  return std::binary_search(
    std::cbegin(k{variant_name}VariantAllowList),
    std::cend(k{variant_name}VariantAllowList),
    s);
}}

}}  // namespace {namespace}

#endif  // {include_guard}
"""


class Error(Exception):
  pass


def _GenerateStaticFile(file_path, namespace, variant_list, allow_list_name):
  """Generates a header file with constexpr facilities to check for the
    existence of a variant.

  Args:
    namespace: A namespace to contain generated array.
    variant_list(List[Dict]]):
      A list of variant objects [{variant: {name, summary, ...}}]
    allow_list_name: A name of the variant list for an allow list.
  Returns:
    String with the generated content.
  """
  variant_list = sorted(variant_list, key=lambda d: d['name'])
  include_guard = file_path.replace('\\', '_').replace('/', '_').replace(
      '.', '_').upper() + "_"

  variants = "\n".join(
      ["  \"{name}\",".format(name=value['name']) for value in variant_list])
  return _FILE.format(script_name=_SCRIPT_NAME,
                      include_guard=include_guard,
                      namespace=namespace,
                      variants=variants,
                      variant_name=allow_list_name)


def _GenerateVariantList(histograms, allow_list_name):
  all_variants, had_errors = extract_histograms.ExtractVariantsFromXmlTree(
      histograms)
  if had_errors:
    raise Error("Error parsing inputs.")

  if (allow_list_name not in all_variants):
    raise Error("AllowListName is missing in variants list")

  return all_variants[allow_list_name]


def _GenerateFile(arguments):
  """Generates header file containing array with Variant names.

  Args:
    arguments: An object with the following attributes:
      arguments.input: An xml file with histogram descriptions.
      arguments.file: A filename of the generated source file.
      arguments.namespace: A namespace to contain generated array.
      arguments.output_dir: A directory to put the generated file.
      arguments.allow_list_name: A name of the variant list for an allow list.
  """
  histograms = xml.dom.minidom.parse(arguments.input)
  variants = _GenerateVariantList(histograms, arguments.allow_list_name)

  static_check_header_file_content = _GenerateStaticFile(
      arguments.file, arguments.namespace, variants, arguments.allow_list_name)
  with open(os.path.join(arguments.output_dir, arguments.file),
            "w") as generated_file:
    generated_file.write(static_check_header_file_content)


def _ParseArguments():
  """Defines and parses arguments from the command line."""
  arg_parser = argparse.ArgumentParser(
      description="Generate an array of AllowList based on variants.")
  arg_parser.add_argument("--output_dir",
                          "-o",
                          required=True,
                          help="Base directory to for generated files.")
  arg_parser.add_argument("--file",
                          "-f",
                          required=True,
                          help="File name of the generated file.")
  arg_parser.add_argument(
      "--allow_list_name",
      "-a",
      required=True,
      help="Name of variant list that should be part of the allow list.")
  arg_parser.add_argument("--namespace",
                          "-n",
                          required=True,
                          help="Namespace of the allow list array.")
  arg_parser.add_argument("--input",
                          "-i",
                          help="Path to .xml file with histogram descriptions.")
  return arg_parser.parse_args()


def main():
  arguments = _ParseArguments()
  _GenerateFile(arguments)


if __name__ == "__main__":
  sys.exit(main())
