# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A template for generating hash decoding code."""

import os

import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.ukm.codegen as codegen
import chromium_src.tools.metrics.ukm.ukm_model as ukm_model

HEADER = codegen.Template(
  basename='ukm_decode.h',
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
  event_template='',
  metric_template='',
)

_IMPL_FILE_TEMPLATE = """
// Generated from gen_builders.py.  DO NOT EDIT!
// source: ukm.xml

#include "{file.dir_path}/ukm_decode.h"

#include <cstdint>
#include <iterator>
#include <string_view>
#include <utility>
#include <vector>

#include "base/metrics/metrics_hashes.h"

namespace ukm {{
namespace builders {{

namespace {{

// Entry and metric names, in decode-map order: each entry name is
// followed by the names of its metrics.
// TODO(crbug.com/541711739): These strings duplicate the kEntryName /
// k*Name constants in ukm_builders.cc. Make ukm_builders.cc share this
// string table instead of defining its own copies.
constexpr char kNames[] =
{name_strings};

// Number of metrics for each entry, in the order the entry names appear
// in kNames.
constexpr uint16_t kMetricCounts[] = {{
{metric_count_rows}}};

}}  // namespace

const DecodeMap& GetDecodeMap() {{
  // The hashes are computed at runtime rather than emitted as
  // codegen-time constants: this is an intentional trade-off that keeps
  // ukm_decode.cc as compact name tables instead of a per-metric
  // initializer with an 8-byte hash immediate each, saving binary size.
  // The one-time construction cost (low single-digit ms) is paid when
  // the first UKM entry is recorded.
  static const base::NoDestructor<DecodeMap> decode_map([] {{
    std::vector<std::pair<uint64_t, EntryDecoder>> entries;
    entries.reserve(std::size(kMetricCounts));
    std::string_view name = kNames;
    for (const uint16_t metric_count : kMetricCounts) {{
      const std::string_view entry_name = name;
      // data() + size() points at the NUL separator; + 1 steps to the
      // next name.
      name = std::string_view(name.data() + name.size() + 1);
      std::vector<std::pair<uint64_t, const char*>> metrics;
      metrics.reserve(metric_count);
      for (uint32_t i = 0; i < metric_count; ++i) {{
        metrics.emplace_back(base::HashMetricName(name), name.data());
        name = std::string_view(name.data() + name.size() + 1);
      }}
      entries.emplace_back(
          base::HashMetricName(entry_name),
          EntryDecoder{{entry_name.data(),
                        MetricDecodeMap(std::move(metrics))}});
    }}
    return DecodeMap(std::move(entries));
  }}());
  return *decode_map;
}}

}}  // namespace builders
}}  // namespace ukm
"""

_NAME_STRING_TEMPLATE = '    "{name}\\0"\n'
_METRIC_COUNT_ROW_TEMPLATE = '    {metric_count},\n'


class DecodeImplTemplate:
  """Template for producing ukm_decode.cc from ukm.xml.

  Emits the entry and metric names as data tables rather than per-metric
  code, which is considerably smaller in the compiled binary.
  """

  def __init__(self):
    self.basename = 'ukm_decode.cc'

  def _stamp_file_code(self, relpath, data):
    file_info = codegen.codegen_shared.FileInfo(relpath, self.basename)
    names = []
    metric_count_rows = ''
    for event in data[ukm_model._EVENT_TYPE.tag]:
      event_info = codegen.EventInfo(event)
      metrics = event[ukm_model._METRIC_TYPE.tag]
      assert len(metrics) <= 0xFFFF, (
        f'Too many metrics in {event_info.raw_name} for uint16_t count'
      )
      names.append(event_info.raw_name)
      names.extend(codegen.MetricInfo(metric).raw_name for metric in metrics)
      metric_count_rows += _METRIC_COUNT_ROW_TEMPLATE.format(
        metric_count=len(metrics)
      )

    name_strings = ''.join(
      _NAME_STRING_TEMPLATE.format(name=name) for name in names
    )
    return _IMPL_FILE_TEMPLATE.format(
      file=file_info,
      name_strings=name_strings,
      metric_count_rows=metric_count_rows,
    )

  def write_file(self, outdir, relpath, data):
    with open(os.path.join(outdir, self.basename), 'w') as output:
      output.write(self._stamp_file_code(relpath, data))


IMPL = DecodeImplTemplate()


def write_files(outdir, relpath, data):
  HEADER.write_file(outdir, relpath, data)
  IMPL.write_file(outdir, relpath, data)
