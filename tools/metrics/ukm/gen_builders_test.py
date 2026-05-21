#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import os

import setup_modules  # pylint: disable=unused-import

from chromium_src.tools.metrics.ukm.codegen import EventInfo
from chromium_src.tools.metrics.ukm.codegen import MetricInfo
from chromium_src.tools.metrics.ukm.builders_template import HEADER as BUILDERS_HEADER_TEMPLATE
from chromium_src.tools.metrics.ukm.builders_template import IMPL as BUILDERS_IMPL_TEMPLATE
from chromium_src.tools.metrics.ukm.decode_template import HEADER as DECODE_HEADER_TEMPLATE
from chromium_src.tools.metrics.ukm.decode_template import IMPL as DECODE_IMPL_TEMPLATE
import chromium_src.tools.metrics.ukm.ukm_model as ukm_model
import chromium_src.tools.metrics.ukm.gen_builders as gen_builders

_FILE_DIR = os.path.dirname(__file__)

class GenBuildersTest(unittest.TestCase):

  def test_filter_obsolete_metrics(self):
    data = gen_builders.read_filtered_data(_FILE_DIR + '/ukm.xml')
    for event in data[ukm_model._EVENT_TYPE.tag]:
      self.assertTrue(ukm_model.is_not_obsolete(event))
      for metric in event[ukm_model._METRIC_TYPE.tag]:
        self.assertTrue(ukm_model.is_not_obsolete(metric))

  def test_generate_code(self):
    relpath = '.'
    with open(_FILE_DIR + '/ukm.xml') as f:
      data = ukm_model.UKM_XML_TYPE.Parse(f.read())
    event = data[ukm_model._EVENT_TYPE.tag][0]
    metric = event[ukm_model._METRIC_TYPE.tag][0]
    self.assertIsNotNone(event)
    self.assertIsNotNone(metric)
    event_info = EventInfo(event)
    metric_info = MetricInfo(metric)

    # Not using codegen.Template.WriteFile to avoid non-deterministic test
    # behaviour after writing to disk.
    builders_header_output = BUILDERS_HEADER_TEMPLATE._stamp_file_code(
        relpath, data)
    self.assertIsNotNone(builders_header_output)
    self.assertIn("// Generated from gen_builders.py.  DO NOT EDIT!",
                  builders_header_output)
    self.assertIn("namespace builders", builders_header_output)
    self.assertIn(
        """
class {name} final : public ::ukm::internal::UkmEntryBuilderBase {{
 public:
  explicit {name}(ukm::SourceId source_id);
  explicit {name}(ukm::SourceIdObj source_id);
  {name}({name}&&);
  {name}& operator=({name}&&);
  ~{name}() override;

  static const char kEntryName[];
  static constexpr uint64_t kEntryNameHash = UINT64_C({hash});""".format(
            name=event_info.name, hash=event_info.hash), builders_header_output)

    self.assertIn(
        """
  static const char k{metricName}Name[];
  static constexpr uint64_t k{metricName}NameHash = UINT64_C({metricHash});
  {eventName}& Set{metricName}(int64_t value);
""".format(eventName=event_info.name,
           metricName=metric_info.name,
           metricHash=metric_info.hash), builders_header_output)

    builders_impl_output = BUILDERS_IMPL_TEMPLATE._stamp_file_code(
        relpath, data)
    self.assertIsNotNone(builders_impl_output)
    self.assertIn("// Generated from gen_builders.py.  DO NOT EDIT!",
                  builders_impl_output)
    self.assertIn("namespace builders", builders_impl_output)
    self.assertIn(
        """
const char {name}::kEntryName[] = "{rawName}";
const uint64_t {name}::kEntryNameHash;

{name}::{name}(ukm::SourceId source_id) :
  ::ukm::internal::UkmEntryBuilderBase(source_id, kEntryNameHash) {{
}}

{name}::{name}(ukm::SourceIdObj source_id) :
  ::ukm::internal::UkmEntryBuilderBase(source_id, kEntryNameHash) {{
}}""".format(name=event_info.name, rawName=event_info.raw_name),
        builders_impl_output)

    self.assertIn(
        """
const char {eventName}::k{metricName}Name[] = "{metricRawName}";
const uint64_t {eventName}::k{metricName}NameHash;

{eventName}& {eventName}::Set{metricName}(int64_t value) {{
  SetMetricInternal(k{metricName}NameHash, value);
  return *this;
}}
""".format(eventName=event_info.name,
           metricName=metric_info.name,
           metricRawName=metric_info.raw_name), builders_impl_output)

    decode_header_output = DECODE_HEADER_TEMPLATE._stamp_file_code(
        relpath, data)
    self.assertIsNotNone(decode_header_output)
    self.assertIn("// Generated from gen_builders.py.  DO NOT EDIT!",
                  decode_header_output)
    self.assertIn("namespace builders", decode_header_output)
    self.assertIn(
        """typedef base::flat_map<uint64_t, const char*> MetricDecodeMap;
struct EntryDecoder {
  const char* name;
  MetricDecodeMap metric_map;
};
typedef base::flat_map<uint64_t, EntryDecoder> DecodeMap;
const DecodeMap& GetDecodeMap();""", decode_header_output)

    decode_impl_output = DECODE_IMPL_TEMPLATE._stamp_file_code(relpath, data)
    self.assertIsNotNone(decode_impl_output)
    self.assertIn("// Generated from gen_builders.py.  DO NOT EDIT!",
                  decode_impl_output)
    self.assertIn("namespace builders", decode_impl_output)
    self.assertIn(
        """
    {{{eventName}::k{metricName}NameHash, {eventName}::k{metricName}Name}},""".
        format(eventName=event_info.name,
               metricName=metric_info.name), decode_impl_output)


if __name__ == '__main__':
  unittest.main()
