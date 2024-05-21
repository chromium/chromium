#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A utility for generating classes for structured metrics events.

Takes as input a structured.xml file describing all events and produces a C++
header and implementation file exposing builders for those events.
"""

import argparse
import sys

import code_generator_cpp
import code_generator_ts
from sync import model
import templates_events as templates

parser = argparse.ArgumentParser(
    description='Generate structured metrics events')
parser.add_argument('--input', help='Path to structured.xml')
parser.add_argument('--output', help='Path to generated files.')
parser.add_argument('--target',
                    help='Target for generating events, e.g. chromium, webui.')


def main():
  args = parser.parse_args()
  data = model.Model(open(args.input, encoding='utf-8').read(), 'chrome')

  if (args.target == 'chromium'):
    code_generator_cpp.TemplateCpp(
        data,
        args.output,
        'structured_events.h',
        file_template=templates.HEADER_FILE_TEMPLATE,
        project_template=templates.HEADER_PROJECT_TEMPLATE,
        enum_template=templates.HEADER_ENUM_TEMPLATE,
        event_template=templates.HEADER_EVENT_TEMPLATE,
        metric_template=templates.HEADER_METRIC_TEMPLATE,
        header=True).write_file()

    code_generator_cpp.TemplateCpp(
        data,
        args.output,
        'structured_events.cc',
        file_template=templates.IMPL_FILE_TEMPLATE,
        project_template=templates.IMPL_PROJECT_TEMPLATE,
        event_template=templates.IMPL_EVENT_TEMPLATE,
        # Enums are only generated in the header file.
        enum_template=None,
        metric_template=templates.IMPL_METRIC_TEMPLATE,
        header=False).write_file()

  elif (args.target == 'webui'):
    code_generator_ts.TemplateTypescript(
        data,
        args.output,
        'structured_events.ts',
        file_template=templates.TS_FILE_TEMPLATE,
        project_template=templates.TS_PROJECT_TEMPLATE,
        enum_template=templates.TS_ENUM_TEMPLATE,
        event_template=templates.TS_EVENT_TEMPLATE,
        metric_template=templates.TS_METRIC_TEMPLATE,
        metric_field_template=templates.TS_METRIC_FIELD_TEMPLATE,
        metric_build_code_template=templates.TS_METRIC_BUILD_TEMPLATE
    ).write_file()

  return 0


if __name__ == '__main__':
  sys.exit(main())
