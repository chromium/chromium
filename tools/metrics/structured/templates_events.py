# -*- coding: utf-8 -*-
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Templates for generating event builder classes for structured metrics."""

HEADER_FILE_TEMPLATE = """
// Generated from gen_events.py. DO NOT EDIT!
// source: structured.xml

#ifndef {file.guard_path}
#define {file.guard_path}

#include <cstdint>
#include <string>

#include "components/metrics/structured/event.h"

namespace metrics {{
namespace structured {{
namespace events {{
namespace v2 {{

{project_code}

}}  // namespace v2
}}  // namespace events
}}  // namespace structured
}}  // namespace metrics

#endif  // {file.guard_path}\
"""

HEADER_PROJECT_TEMPLATE = """\
namespace {project.namespace} {{

{event_code}\
}}  // namespace {project.namespace}

"""

HEADER_EVENT_TEMPLATE = """\
class {event.name} final : public ::metrics::structured::Event {{
 public:
  {event.name}();
  ~{event.name}() override;

  {metric_code}\
}};

"""

HEADER_METRIC_TEMPLATE = """\
  {event.name}& Set{metric.name}(const {metric.type} value);
"""

IMPL_FILE_TEMPLATE = """\
// Generated from gen_events.py. DO NOT EDIT!
// source: structured.xml

#include "components/metrics/structured/structured_events.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace metrics {{
namespace structured {{
namespace events {{
namespace v2 {{

{project_code}
}}  // namespace v2
}}  // namespace events
}}  // namespace structured
}}  // namespace metrics\
"""

IMPL_PROJECT_TEMPLATE = """\
namespace {project.namespace} {{

{event_code}\
}}  // namespace {project.namespace}

"""

IMPL_EVENT_TEMPLATE = """\
{event.name}::{event.name}() :
  ::metrics::structured::Event(\"{event.project_name}\",
                               \"{event.name}\",
                               {event.is_event_sequence}) {{}}
{event.name}::~{event.name}() = default;

{metric_code}\
"""

IMPL_METRIC_TEMPLATE = """\
{event.name}& {event.name}::Set{metric.name}(const {metric.type} value) {{
  AddMetric(\"{metric.name}\", Event::MetricType::{metric.type_enum},
            {metric.base_value});
  return *this;
}}

"""
