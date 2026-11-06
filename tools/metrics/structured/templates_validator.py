# -*- coding: utf-8 -*-
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

HEADER_FILE_TEMPLATE = """\
// Generated from gen_validator.py. DO NOT EDIT!
// source: structured.xml

#ifndef {file.guard_path}
#define {file.guard_path}

#include <cstdint>
#include <optional>
#include <string_view>

#include "base/containers/span.h"
#include "components/metrics/structured/event_validator.h"
#include "components/metrics/structured/project_validator.h"

namespace metrics {{
namespace structured {{
namespace validator {{

class Validators final {{
 public:
  Validators(const Validators&) = delete;
  Validators& operator=(const Validators&) = delete;

  const ProjectValidator* GetProjectValidator(
      std::string_view project_name) const;

  std::optional<std::string_view> GetProjectName(
      uint64_t project_name_hash) const;

  static Validators* Get();

  static std::string_view GetString(uint16_t offset, uint16_t len);
  static base::span<const EventValidator::MetricEntry> GetMetrics(
      uint16_t offset,
      uint8_t count);
  static base::span<const EventValidator> GetEvents(uint16_t offset,
                                                    uint16_t count);

 private:
  constexpr Validators() = default;
}};

}}  // namespace validator
}}  // namespace structured
}}  // namespace metrics

#endif  // {file.guard_path}\
"""

IMPL_FILE_TEMPLATE = """\
// Generated from gen_validator.py. DO NOT EDIT!
// source: structured.xml

#include "components/metrics/structured/structured_metrics_validator.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

#include "base/containers/span.h"
#include "base/types/to_address.h"
#include "components/metrics/structured/enums.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/event_validator.h"
#include "components/metrics/structured/project_validator.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics {{
namespace structured {{
namespace {{

constexpr char kStringsStorage[] =
{string_table_literal};

static_assert(sizeof(kStringsStorage) - 1 == {string_table_len});

constexpr std::array<EventValidator::MetricEntry, {num_metrics}> kMetrics =
    {metrics_init};

constexpr std::array<EventValidator, {num_events}> kEvents =
    {events_init};

constexpr std::array<ProjectValidator, {num_projects}> kProjects =
    {projects_init};

}}  // namespace

namespace validator {{

const ProjectValidator* Validators::GetProjectValidator(
    std::string_view project_name) const {{
  const auto projects = base::span(kProjects);
  const auto it = std::ranges::lower_bound(
      projects, project_name, std::less{{}},
      [](const ProjectValidator& project) {{
        return GetString(project.name_offset(), project.name_len());
      }});
  if (it == projects.end() ||
      GetString(it->name_offset(), it->name_len()) != project_name) {{
    return nullptr;
  }}
  return base::to_address(it);
}}

std::optional<std::string_view> Validators::GetProjectName(
    uint64_t project_name_hash) const {{
  for (const ProjectValidator& project : kProjects) {{
    if (project.project_hash() == project_name_hash) {{
      return GetString(project.name_offset(), project.name_len());
    }}
  }}
  return std::nullopt;
}}

// static
Validators* Validators::Get() {{
  static Validators validators;
  return &validators;
}}

// static
std::string_view Validators::GetString(uint16_t offset, uint16_t len) {{
  return std::string_view(kStringsStorage, {string_table_len})
      .substr(offset, len);
}}

// static
base::span<const EventValidator::MetricEntry> Validators::GetMetrics(
    uint16_t offset,
    uint8_t count) {{
  return base::span(kMetrics).subspan(offset, count);
}}

// static
base::span<const EventValidator> Validators::GetEvents(uint16_t offset,
                                                       uint16_t count) {{
  return base::span(kEvents).subspan(offset, count);
}}

}}  // namespace validator
}}  // namespace structured
}}  // namespace metrics
"""
