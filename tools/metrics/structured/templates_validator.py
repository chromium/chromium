# -*- coding: utf-8 -*-
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

HEADER_FILE_TEMPLATE = """\
// Generated from gen_validator.py. DO NOT EDIT!
// source: structured.xml

#ifndef {file.guard_path}
#define {file.guard_path}

#include <memory>
#include <string>
#include <unordered_map>

#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "components/metrics/structured/project_validator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace metrics {{
namespace structured {{
namespace validator {{

class Validators final {{

public:
  Validators();

  Validators(const Validators&) = delete;
  Validators& operator=(const Validators&) = delete;

  void Initialize();

  absl::optional<const ProjectValidator*>
    GetProjectValidator(const std::string& project_name);

  static Validators* Get();

private:
  friend class base::NoDestructor<Validators>;

  std::unordered_map<base::StringPiece, std::unique_ptr<ProjectValidator>>
      validators_;
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

#include <cstdint>
#include <string>

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
//---------------------ProjectValidator Classes---------------------------------
{projects_code}

}}

namespace validator {{

Validators::Validators() {{
  Initialize();
}}

void Validators::Initialize() {{
  {project_map};
}}

absl::optional<const ProjectValidator*>
  Validators::GetProjectValidator(const std::string& project_name) {{
    const auto it = validators_.find(project_name);
     if (it == validators_.end())
        return absl::nullopt;
     return it->second.get();
}}

// static
Validators* Validators::Get() {{
  static base::NoDestructor<Validators> validators;
  return validators.get();
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

    void Initialize();

    static constexpr uint64_t kProjectNameHash = UINT64_C({project.name_hash});
    static constexpr IdType kIdType = IdType::{project.id_type};
    static constexpr IdScope kIdScope = IdScope::{project.id_scope};
    static constexpr EventType kEventType =
        StructuredEventProto_EventType_{project.event_type};
    static constexpr int kKeyRotationPeriod =
        {project.key_rotation_period};

  private:
    std::unordered_map<base::StringPiece,
        std::unique_ptr<EventValidator>> event_validators_;
}};

{project.validator}::{project.validator}() :
  ::metrics::structured::ProjectValidator(
  {project.validator}::kProjectNameHash,
  {project.validator}::kIdType,
  {project.validator}::kIdScope,
  {project.validator}::kEventType,
  {project.validator}::kKeyRotationPeriod
)
  {{
  Initialize();
}}

void {project.validator}::Initialize() {{
  {event_validator_map};
}}

{project.validator}::~{project.validator}() = default;

absl::optional<const EventValidator*> {project.validator}::GetEventValidator(
                                        const std::string& event_name) const {{
   const auto it = event_validators_.find(event_name);
   if (it == event_validators_.end())
      return absl::nullopt;
   return it->second.get();
}}
"""

IMPL_EVENT_VALIDATOR_TEMPLATE = """\
class {event.validator_name} final :
    public ::metrics::structured::EventValidator {{
  public:
    {event.validator_name}();
    ~{event.validator_name}() override;

    void Initialize();

    static constexpr uint64_t kEventNameHash = UINT64_C({event.name_hash});

    absl::optional<MetricMetadata>
      GetMetricMetadata(const std::string& metric_name) const override;

  private:
    std::unordered_map<base::StringPiece, EventValidator::MetricMetadata>
        metric_metadata_;
}};

{event.validator_name}::{event.validator_name}() :
  ::metrics::structured::EventValidator({event.validator_name}::kEventNameHash,
                                        {event.force_record})
  {{
  Initialize();
}}

{event.validator_name}::~{event.validator_name}() = default;

void {event.validator_name}::Initialize() {{
  metric_metadata_ = {{
    {metric_hash_map}
   }};
}}

absl::optional<EventValidator::MetricMetadata>
{event.validator_name}::GetMetricMetadata(const std::string& metric_name)
const {{
   const auto it = metric_metadata_.find(metric_name);
   if (it == metric_metadata_.end())
      return absl::nullopt;
   return it->second;
}}
"""
