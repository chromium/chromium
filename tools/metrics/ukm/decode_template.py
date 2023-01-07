# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A template for generating hash decoding code."""

import codegen

HEADER = codegen.Template(
basename="ukm_decode.h",
file_template="""
// Generated from gen_builders.py.  DO NOT EDIT!
// source: ukm.xml

#ifndef {file.guard_path}
#define {file.guard_path}

#include <cstdint>
#include <map>

namespace ukm {{
namespace builders {{

typedef std::map<uint64_t, const char*> MetricDecodeMap;
struct EntryDecoder {{
  const char* name;
  const MetricDecodeMap metric_map;
}};
typedef std::map<uint64_t, EntryDecoder> DecodeMap;
DecodeMap CreateDecodeMap();

}}  // namespace builders
}}  // namespace ukm

#endif  // {file.guard_path}
""",
event_template="",
metric_template="")

IMPL = codegen.Template(
basename="ukm_decode.cc",
file_template="""
// Generated from gen_builders.py.  DO NOT EDIT!
// source: ukm.xml

#include "{file.dir_path}/ukm_decode.h"
#include "{file.dir_path}/ukm_builders.h"

namespace ukm {{
namespace builders {{

std::map<uint64_t, EntryDecoder> CreateDecodeMap() {{
  return {{
    {event_code}
  }};
}}

}}  // namespace builders
}}  // namespace ukm
""",
event_template="""
    {{
      UINT64_C({event.hash}),
      {{
        {event.name}::kEntryName,
        {{
          {metric_code}
        }}
      }}
    }},
""",
metric_template="""
    {{{event.name}::k{metric.name}NameHash, {event.name}::k{metric.name}Name}},
""")


def WriteFiles(outdir, relpath, data):
  HEADER.WriteFile(outdir, relpath, data)
  IMPL.WriteFile(outdir, relpath, data)
