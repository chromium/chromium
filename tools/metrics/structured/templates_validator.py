# -*- coding: utf-8 -*-
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

HEADER_FILE_TEMPLATE = """\
// Generated from gen_validator.py. DO NOT EDIT!
// source: structured.xml

#ifndef {file.guard_path}
#define {file.guard_path}

#include <string>

#include "components/metrics/structured/project_validator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace metrics {{
namespace structured {{
namespace validator {{

absl::optional<const ProjectValidator*>
  GetProjectValidator(const std::string& project_name);

}} // namespace validator
}}  // namespace structured
}}  // namespace metrics

#endif  // {file.guard_path}\
"""

IMPL_FILE_TEMPLATE = """\
// Generated from gen_validator.py. DO NOT EDIT!
// source: structured.xml

#include "components/metrics/structured/structured_metrics_validator.h"

#include <cstdint>
#include <string>

#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_piece.h"
#include "components/metrics/structured/enums.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/event_validator.h"
#include "components/metrics/structured/project_validator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics {{
namespace structured {{

namespace {{

//---------------------EventValidator Classes----------------------------------
{event_code}
//---------------------EventValidator Declarations------------------------------
{event_validators}
//---------------------EventValidator Maps--------------------------------------
{project_event_maps}
//---------------------ProjectValidator Classes---------------------------------
{projects_code}
//---------------------ProjectValidator Declarations----------------------------
{project_validators}
//---------------------ProjectValidator Map-------------------------------------
{project_map}

}}

namespace validator {{

absl::optional<const ProjectValidator*>
  GetProjectValidator(const std::string& project_name) {{
  const auto it = kStructuredMetricsProjectValidatorMap.find(project_name);
   if (it == kStructuredMetricsProjectValidatorMap.end())
      return absl::nullopt;
   return it->second;
}}

}} // namespace validator
}}  // namespace structured
}}  // namespace metrics\
"""

IMPL_PROJECT_VALIDATOR_TEMPLATE = """\
class {project.validator} final :
    public ::metrics::structured::ProjectValidator {{
  public:
    {project.validator}();
    ~{project.validator}();

    absl::optional<const EventValidator*> GetEventValidator(
      const std::string& event_name) const override;

    static constexpr uint64_t kProjectNameHash = UINT64_C({project.name_hash});
    static constexpr IdType kIdType = IdType::{project.id_type};
    static constexpr IdScope kIdScope = IdScope::{project.id_scope};
    static constexpr EventType kEventType =
        StructuredEventProto_EventType_{project.event_type};
    static constexpr int kKeyRotationPeriod =
        {project.key_rotation_period};
}};

{project.validator}::{project.validator}() :
  ::metrics::structured::ProjectValidator(
  {project.validator}::kProjectNameHash,
  {project.validator}::kIdType,
  {project.validator}::kIdScope,
  {project.validator}::kEventType,
  {project.validator}::kKeyRotationPeriod
)
  {{}}

{project.validator}::~{project.validator}() = default;

absl::optional<const EventValidator*> {project.validator}::GetEventValidator(
                                        const std::string& event_name) const {{
   const auto it = k{project.validator}EventMap.find(event_name);
   if (it == k{project.validator}EventMap.end())
      return absl::nullopt;
   return it->second;
}}
"""

IMPL_PROJECT_EVENT_MAP_TEMPLATE = """\
static constexpr auto k{project.validator}EventMap = base::MakeFixedFlatMap
  <base::StringPiece, const EventValidator*>({{
  {event_validator_map}
}});
"""

IMPL_PROJECT_MAP_TEMPLATE = """\
static constexpr auto kStructuredMetricsProjectValidatorMap =
  base::MakeFixedFlatMap<base::StringPiece, const ProjectValidator*>({{
    {project_map}
}});
"""

IMPL_EVENT_VALIDATOR_TEMPLATE = """\
class {event.validator_name} final :
    public ::metrics::structured::EventValidator {{
  public:
    {event.validator_name}();
    ~{event.validator_name}() override;

    static constexpr uint64_t kEventNameHash = UINT64_C({event.name_hash});

    absl::optional<MetricMetadata>
      GetMetricMetadata(const std::string& metric_name) const override;
}};

{event.validator_name}::{event.validator_name}() :
  ::metrics::structured::EventValidator({event.validator_name}::kEventNameHash)
  {{}}

{event.validator_name}::~{event.validator_name}() = default;

absl::optional<EventValidator::MetricMetadata>
{event.validator_name}::GetMetricMetadata(const std::string& metric_name)
const {{
  {get_metrics_metadata_impl}
}}
"""

IMPL_GET_METRICS_METADATA = """\
static constexpr auto metric_hash_map = base::MakeFixedFlatMap<
      base::StringPiece, EventValidator::MetricMetadata>({{
    {metric_hash_map}
   }});
   const auto* it = metric_hash_map.find(metric_name);
   if (it == metric_hash_map.end())
      return absl::nullopt;
   return it->second;
"""
