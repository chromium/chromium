# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Templates for generating builder classes for DWA entries."""

import codegen

HEADER = codegen.Template(basename="dwa_builders.h",
                          file_template="""
// Generated from gen_builders.py.  DO NOT EDIT!
// source: dwa.xml

#ifndef {file.guard_path}
#define {file.guard_path}

#include <cstdint>
#include <string_view>

#include "components/metrics/dwa/dwa_entry_builder_base.h"

namespace dwa {{
namespace builders {{

{event_code}

}}  // namespace builders
}}  // namespace dwa

#endif  // {file.guard_path}
""",
                          event_template="""
class {event.name} final : public ::dwa::internal::DwaEntryBuilderBase {{
 public:
  explicit {event.name}();
  {event.name}({event.name}&&);
  {event.name}& operator=({event.name}&&);
  ~{event.name}() override;

  static const char kEntryName[];
  static constexpr uint64_t kEntryNameHash = UINT64_C({event.hash});

  {event.name}& SetContent(std::string_view content);

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

IMPL = codegen.Template(
    basename="dwa_builders.cc",
    file_template="""
// Generated from gen_builders.py.  DO NOT EDIT!
// source: dwa.xml

#include "{file.dir_path}dwa_builders.h"

#include "base/metrics/metrics_hashes.h"

namespace dwa {{
namespace builders {{

{event_code}

}}  // namespace builders
}}  // namespace dwa
""",
    event_template="""
const char {event.name}::kEntryName[] = "{event.raw_name}";
const uint64_t {event.name}::kEntryNameHash;

{event.name}::{event.name}() :
  ::dwa::internal::DwaEntryBuilderBase(kEntryNameHash) {{
  {study_code}
}}

{event.name}::{event.name}({event.name}&&) = default;

{event.name}& {event.name}::operator=({event.name}&&) = default;

{event.name}::~{event.name}() = default;

{event.name}& {event.name}::SetContent(std::string_view content) {{
  SetContentInternal(base::HashMetricName(content));
  return *this;
}}

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
    study_template="""
  AddToStudiesOfInterestInternal(k{study.name}Name);
""",
)


def WriteFiles(outdir, relpath, data):
  HEADER.WriteFile(outdir, relpath, data)
  IMPL.WriteFile(outdir, relpath, data)
