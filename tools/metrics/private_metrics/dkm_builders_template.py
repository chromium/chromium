# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Templates for generating builder classes for DKM entries."""

import private_metrics_codegen

HEADER = private_metrics_codegen.Template(  #
    basename="dkm_builders.h",
    file_template="""\
// Generated from gen_private_metrics_builders.py.  DO NOT EDIT!
// source: dkm.xml

#ifndef {file.guard_path}
#define {file.guard_path}

#include <cstdint>
#include <string_view>

#include "components/metrics/private_metrics/dkm_entry_builder_base.h"

namespace metrics::private_metrics::builders::dkm {{
{event_code}
}}  // namespace metrics::private_metrics::builders::dkm

#endif  // {file.guard_path}
""",
    event_template="""
class {event.name} final :
  public internal::DkmEntryBuilderBase {{
 public:
  explicit {event.name}(ukm::SourceIdObj source_id);
  {event.name}({event.name}&&);
  {event.name}& operator=({event.name}&&);
  ~{event.name}() override;

  static const char kEntryName[];
  static constexpr uint64_t kEntryNameHash = UINT64_C({event.hash});

{metric_code}
{study_code}
}};
""",
    metric_template="""
  static const char k{metric.name}Name[];
  static constexpr uint64_t k{metric.name}NameHash = UINT64_C({metric.hash});
  {event.name}& Set{metric.name}(int64_t value);
""",
    study_template="""
  static constexpr char k{study.name}Name[] = "{study.raw_name}";
  static constexpr uint32_t k{study.name}NameHash = UINT32_C({study.hash});
""")

IMPL = private_metrics_codegen.Template(
    basename="dkm_builders.cc",
    file_template="""\
// Generated from gen_private_metrics_builders.py.  DO NOT EDIT!
// source: dkm.xml

#include "{file.dir_path}dkm_builders.h"

#include "base/metrics/metrics_hashes.h"

namespace metrics::private_metrics::builders::dkm {{

{event_code}
}}  // namespace metrics::private_metrics::builders::dkm
""",
    event_template="""\
const char {event.name}::kEntryName[] = "{event.raw_name}";
const uint64_t {event.name}::kEntryNameHash;

{event.name}::{event.name}(ukm::SourceIdObj source_id) :
  internal::DkmEntryBuilderBase(source_id, kEntryNameHash) {{
{study_code}
}}

{event.name}::{event.name}({event.name}&&) = default;

{event.name}& {event.name}::operator=({event.name}&&) = default;

{event.name}::~{event.name}() = default;

{metric_code}
""",
    metric_template="""
const char {event.name}::k{metric.name}Name[] = "{metric.raw_name}";
const uint64_t {event.name}::k{metric.name}NameHash;

{event.name}& {event.name}::Set{metric.name}(int64_t value) {{
  SetMetricInternal(k{metric.name}NameHash, value);
  return *this;
}}
""",
    study_template="""\
  AddToStudiesOfInterestInternal(k{study.name}Name);
""",
)


def WriteFiles(outdir, relpath, data):
  HEADER.WriteFile(outdir, relpath, data)
  IMPL.WriteFile(outdir, relpath, data)
