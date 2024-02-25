#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Build rules to generate metadata schema versions."""

import argparse
import shutil
import sys
import os

PLACEHOLDER_STRING = "{LATEST_METADATA_PARSER_VERSION}"


def extract_version_from_file_comment(metadata_schema_file: str):
    """Extracts the schema semantic version from a comment in the schema file.

    Args:
        metadata_schema_file: The file path of the metadata schema file which contains
        the schema semantic version.
    Returns:
        A string of the schema semantic version.

    Raises:
        ValueError: An error occurred accessing the schema semantic version.
    """
    with open(metadata_schema_file, 'r') as file:
        for line in list(file):
            if "Schema Semantic version:" in line:
                return line.split(":", 1)[1].strip()
    raise ValueError("Schema semantic version not found.")


def copy_template(template: str, directory: str):
    """Copies header template into target directory if template exists and
    contains placeholder string.

    Args:
        template: The file path of the header template.
        directory: The desired directory of the new metadata parser.
    Raises:
        ValueError: An error occurred finding the placeholder string in the template.
    """
    with open(template, 'r') as file:
        if PLACEHOLDER_STRING not in file.read():
            raise ValueError("Placeholder string: " + PLACEHOLDER_STRING +
                             " not found in template")
    os.makedirs(os.path.dirname(directory), exist_ok=True)
    shutil.copyfile(template, directory)


def replace_version_in_template(template_copy: str, version: str):
    """Replaces the parser version placeholder string with a version number.

    Args:
        template_copy: The file path of the copied header template.
        version: The metadata parser version.
    Raises:
        ValueError: An error occurred finding the placeholder string in the template copy.
    """
    with open(template_copy, 'r') as file:
        filedata = file.read()
        if PLACEHOLDER_STRING not in filedata:
            raise ValueError("Placeholder string: " + PLACEHOLDER_STRING +
                             " not found in template copy")
        filedata = filedata.replace(PLACEHOLDER_STRING, version)
    with open(template_copy, 'w') as file:
        file.write(filedata)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--schema',
                        required = True,
                        help = 'File path of metadata schema file.')
    parser.add_argument('--template',
                        required = True,
                        help = 'File path of header template file.')
    parser.add_argument('--output',
                        required = True,
                        help = 'Desired output directory of metadata parser header.')
    args = parser.parse_args()

    version = extract_version_from_file_comment(args.schema)
    copy_template(args.template, args.output)
    replace_version_in_template(args.output, version)

    return 0


if __name__ == '__main__':
    sys.exit(main())
