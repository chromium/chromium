#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A utility for generating classes for structured metrics events.

Takes as input a structured.xml file describing all events and produces a C++
header and implementation file exposing builders for those events.

TODO(crbug.com/1249222): Rename this to gen_events.py once migration is
complete.
"""

import argparse
import sys

import codegen
import model
import templates_mojo_events as templates

parser = argparse.ArgumentParser(
    description='Generate structured metrics mojo events')
parser.add_argument('--input', help='Path to structured.xml')
parser.add_argument('--output', help='Path to generated files.')


def main():
  args = parser.parse_args()
  data = model.Model(open(args.input).read())

  codegen.Template(
      data,
      args.output,
      'structured_mojo_events.h',
      file_template=templates.HEADER_FILE_TEMPLATE,
      project_template=templates.HEADER_PROJECT_TEMPLATE,
      event_template=templates.HEADER_EVENT_TEMPLATE,
      metric_template=templates.HEADER_METRIC_TEMPLATE).write_file()

  codegen.Template(data,
                   args.output,
                   'structured_mojo_events.cc',
                   file_template=templates.IMPL_FILE_TEMPLATE,
                   project_template=templates.IMPL_PROJECT_TEMPLATE,
                   event_template=templates.IMPL_EVENT_TEMPLATE,
                   metric_template=templates.IMPL_METRIC_TEMPLATE).write_file()

  return 0


if __name__ == '__main__':
  sys.exit(main())
