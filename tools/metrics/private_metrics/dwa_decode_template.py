# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A template for generating hash decoding code."""

import private_metrics_codegen

HEADER = private_metrics_codegen.Template(basename="dwa_decode.h",
                                          file_template="""\
// Generated from gen_private_metrics_builders.py.  DO NOT EDIT!
// source: dwa.xml

#ifndef {file.guard_path}
#define {file.guard_path}

#include <cstdint>
#include <map>

namespace dwa::builders {{

typedef std::map<uint64_t, const char*> MetricDecodeMap;
typedef std::map<uint32_t, const char*> StudyDecodeMap;
struct EntryDecoder {{
  const char* name;
  const MetricDecodeMap metric_map;
  const StudyDecodeMap study_map;
}};
typedef std::map<uint64_t, EntryDecoder> DecodeMap;
DecodeMap CreateDecodeMap();

}}  // namespace dwa::builders

#endif  // {file.guard_path}
""",
                                          event_template="",
                                          metric_template="",
                                          study_template="")

IMPL = private_metrics_codegen.Template(basename="dwa_decode.cc",
                                        file_template="""\
// Generated from gen_private_metrics_builders.py.  DO NOT EDIT!
// source: dwa.xml

#include "{file.dir_path}dwa_decode.h"
#include "{file.dir_path}dwa_builders.h"

namespace dwa::builders {{

std::map<uint64_t, EntryDecoder> CreateDecodeMap() {{
  return {{
{event_code}
  }};
}}

}}  // namespace dwa::builders
""",
                                        event_template="""
    {{
      UINT64_C({event.hash}),
      {{
        {event.name}::kEntryName,
        {{
{metric_code}
        }},
        {{
{study_code}
        }}
      }}
    }},
""",
                                        metric_template="""
    {{{event.name}::k{metric.name}NameHash, {event.name}::k{metric.name}Name}},
""",
                                        study_template="""
    {{{event.name}::k{study.name}NameHash, {event.name}::k{study.name}Name}},
""")


def WriteFiles(outdir, relpath, data):
  HEADER.WriteFile(outdir, relpath, data)
  IMPL.WriteFile(outdir, relpath, data)
