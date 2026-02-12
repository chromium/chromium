# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A template for generating hash decoding code."""

import setup_modules

import chromium_src.tools.metrics.ukm.codegen as codegen

HEADER = codegen.Template(basename="ukm_decode.h",
                          file_template="""
// Generated from gen_builders.py.  DO NOT EDIT!
// source: ukm.xml

#ifndef {file.guard_path}
#define {file.guard_path}

#include <cstdint>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"

namespace ukm {{
namespace builders {{

typedef base::flat_map<uint64_t, const char*> MetricDecodeMap;
struct EntryDecoder {{
  const char* name;
  MetricDecodeMap metric_map;
}};
typedef base::flat_map<uint64_t, EntryDecoder> DecodeMap;
const DecodeMap& GetDecodeMap();

}}  // namespace builders
}}  // namespace ukm

#endif  // {file.guard_path}
""",
                          event_template="",
                          metric_template="")

IMPL = codegen.Template(basename="ukm_decode.cc",
                        file_template="""
// Generated from gen_builders.py.  DO NOT EDIT!
// source: ukm.xml

#include "{file.dir_path}/ukm_decode.h"
#include "{file.dir_path}/ukm_builders.h"

namespace ukm {{
namespace builders {{

const DecodeMap& GetDecodeMap() {{
  static const base::NoDestructor<DecodeMap> decode_map({{
    {event_code}
  }});
  return *decode_map;
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
