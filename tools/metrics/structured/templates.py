# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Templates for generating event classes for structured metrics."""

HEADER_FILE_TEMPLATE = """\
// Generated from gen_events.py. DO NOT EDIT!
// source: structured.xml

#ifndef {file.guard_path}
#define {file.guard_path}

#include <cstdint>
#include <string>

#include "components/metrics/structured/event_base.h"

namespace metrics {{
namespace structured {{
namespace events {{

constexpr uint64_t kProjectNameHashes[] = {project_hashes};

{project_code}

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
class {event.name} final : public ::metrics::structured::EventBase {{
 public:
  {event.name}();
  ~{event.name}() override;

  static constexpr uint64_t kEventNameHash = UINT64_C({event.name_hash});
  static constexpr uint64_t kProjectNameHash = UINT64_C({project.name_hash});
  static constexpr IdentifierType kIdType = IdentifierType::{project.id_type};

{metric_code}\
}};

"""

HEADER_METRIC_TEMPLATE = """\
  static constexpr uint64_t k{metric.name}NameHash = UINT64_C({metric.hash});
  {event.name}& Set{metric.name}(const {metric.type} value);

"""

IMPL_FILE_TEMPLATE = """\
// Generated from gen_events.py. DO NOT EDIT!
// source: structured.xml

// #include "{file.dirname}/structured_events.h"
#include "components/metrics/structured/structured_events.h"

namespace metrics {{
namespace structured {{
namespace events {{
{project_code}
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
  ::metrics::structured::EventBase(kEventNameHash, kProjectNameHash) {{}}
{event.name}::~{event.name}() = default;
{metric_code}\
"""

IMPL_METRIC_TEMPLATE = """\
{event.name}& {event.name}::Set{metric.name}(const {metric.type} value) {{
  {metric.setter}(k{metric.name}NameHash, value);
  return *this;
}}

"""
