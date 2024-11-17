#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import codegen
import builders_template
import decode_template
import dwa_model
import os

_FILE_DIR = os.path.dirname(__file__)


class GenBuildersTest(unittest.TestCase):

  def setUp(self) -> None:
    self.relpath = '.'
    with open(_FILE_DIR + '/dwa.xml') as f:
      self.data = dwa_model.DWA_XML_TYPE.Parse(f.read())
    with open(_FILE_DIR + '/dwa_test.xml') as f:
      self.dwa_test_data = dwa_model.DWA_XML_TYPE.Parse(f.read())
    event = self.data[dwa_model._EVENT_TYPE.tag][0]
    metric = event[dwa_model._METRIC_TYPE.tag][0]
    study = event[dwa_model._STUDY_TYPE.tag][0]
    self.assertIsNotNone(event)
    self.assertIsNotNone(metric)
    self.assertIsNotNone(study)
    self.event_info = codegen.EventInfo(event)
    self.metric_info = codegen.MetricInfo(metric)
    self.study_info = codegen.StudyInfo(study)

  def testBuildersHeaderOutput(self) -> None:
    # Not using codegen.Template.WriteFile to avoid non-deterministic test
    # behaviour after writing to disk.
    builders_header_output = builders_template.HEADER._StampFileCode(
        self.relpath, self.data)
    self.assertIsNotNone(builders_header_output)
    self.assertIn("// Generated from gen_builders.py.  DO NOT EDIT!",
                  builders_header_output)
    self.assertIn("namespace builders", builders_header_output)
    self.assertIn(
        """
class {name} final : public ::dwa::internal::DwaEntryBuilderBase {{
 public:
  explicit {name}();
  {name}({name}&&);
  {name}& operator=({name}&&);
  ~{name}() override;

  static const char kEntryName[];
  static constexpr uint64_t kEntryNameHash = UINT64_C({hash});

  {name}& SetContent(std::string_view content);
""".format(name=self.event_info.name, hash=self.event_info.hash),
        builders_header_output)

    self.assertIn(
        """
  static const char k{metricName}Name[];
  static constexpr uint64_t k{metricName}NameHash = UINT64_C({metricHash});
  {eventName}& Set{metricName}(int64_t value);
""".format(eventName=self.event_info.name,
           metricName=self.metric_info.name,
           metricHash=self.metric_info.hash), builders_header_output)
    self.assertIn(
        """
  static constexpr char k{studyName}Name[] = "{studyRawName}";
  static constexpr uint32_t k{studyName}NameHash = UINT32_C({studyHash});
""".format(eventName=self.event_info.name,
           studyRawName=self.study_info.raw_name,
           studyName=self.study_info.name,
           studyHash=self.study_info.hash), builders_header_output)

  def testBuildersImplementationOutput(self) -> None:
    builders_impl_output = builders_template.IMPL._StampFileCode(
        self.relpath, self.data)
    self.assertIsNotNone(builders_impl_output)
    self.assertIn("// Generated from gen_builders.py.  DO NOT EDIT!",
                  builders_impl_output)
    self.assertIn("namespace builders", builders_impl_output)
    self.assertIn(
        """
const char {name}::kEntryName[] = "{rawName}";
const uint64_t {name}::kEntryNameHash;

{name}::{name}() :
  ::dwa::internal::DwaEntryBuilderBase(kEntryNameHash)""".format(
            name=self.event_info.name, rawName=self.event_info.raw_name),
        builders_impl_output)

    self.assertIn(
        """
{name}& {name}::SetContent(std::string_view content) {{
  SetContentInternal(base::HashMetricName(content));
  return *this;
}}
""".format(name=self.event_info.name, rawName=self.event_info.raw_name),
        builders_impl_output)

    self.assertIn(
        """
const char {eventName}::k{metricName}Name[] = "{metricRawName}";
const uint64_t {eventName}::k{metricName}NameHash;

{eventName}& {eventName}::Set{metricName}(int64_t value) {{
  SetMetricInternal(k{metricName}NameHash, value);
  return *this;
}}
""".format(eventName=self.event_info.name,
           metricName=self.metric_info.name,
           metricRawName=self.metric_info.raw_name), builders_impl_output)
    self.assertIn(
        """
  AddToStudiesOfInterestInternal(k{studyName}Name);
""".format(eventName=self.event_info.name,
           studyName=self.study_info.name,
           studyRawName=self.study_info.raw_name), builders_impl_output)

  def testDecodeHeaderOutput(self) -> None:
    decode_header_output = decode_template.HEADER._StampFileCode(
        self.relpath, self.data)
    self.assertIsNotNone(decode_header_output)
    self.assertIn("// Generated from gen_builders.py.  DO NOT EDIT!",
                  decode_header_output)
    self.assertIn("namespace builders", decode_header_output)
    self.assertIn(
        """typedef std::map<uint64_t, const char*> MetricDecodeMap;
typedef std::map<uint32_t, const char*> StudyDecodeMap;
struct EntryDecoder {
  const char* name;
  const MetricDecodeMap metric_map;
  const StudyDecodeMap study_map;
};
typedef std::map<uint64_t, EntryDecoder> DecodeMap;
DecodeMap CreateDecodeMap();""", decode_header_output)

  def testDecodeImplementationOutput(self) -> None:
    decode_impl_output = decode_template.IMPL._StampFileCode(
        self.relpath, self.data)
    self.assertIsNotNone(decode_impl_output)
    self.assertIn("// Generated from gen_builders.py.  DO NOT EDIT!",
                  decode_impl_output)
    self.assertIn("namespace builders", decode_impl_output)
    self.assertIn(
        """
    {{{eventName}::k{metricName}NameHash, {eventName}::k{metricName}Name}},""".
        format(eventName=self.event_info.name,
               metricName=self.metric_info.name), decode_impl_output)
    self.assertIn(
        """
    {{{eventName}::k{studyName}NameHash, {eventName}::k{studyName}Name}},""".
        format(eventName=self.event_info.name,
               studyName=self.study_info.name), decode_impl_output)

  def testBuildersHeaderOutputFromDwaTest(self) -> None:
    builders_header_output = builders_template.HEADER._StampFileCode(
        self.relpath, self.dwa_test_data)
    self.assertEqual(
        builders_header_output, """
// Generated from gen_builders.py.  DO NOT EDIT!
// source: dwa.xml

#ifndef __DWA_BUILDERS_H
#define __DWA_BUILDERS_H

#include <cstdint>
#include <string_view>

#include "components/metrics/dwa/dwa_entry_builder_base.h"

namespace dwa {
namespace builders {


class DwaTestMetric final : public ::dwa::internal::DwaEntryBuilderBase {
 public:
  explicit DwaTestMetric();
  DwaTestMetric(DwaTestMetric&&);
  DwaTestMetric& operator=(DwaTestMetric&&);
  ~DwaTestMetric() override;

  static const char kEntryName[];
  static constexpr uint64_t kEntryNameHash = UINT64_C(10084375266486398523);

  DwaTestMetric& SetContent(std::string_view content);


  static const char kHasVideoName[];
  static constexpr uint64_t kHasVideoNameHash = UINT64_C(14168404852977906041);
  DwaTestMetric& SetHasVideo(int64_t value);

  static const char kLatencyName[];
  static constexpr uint64_t kLatencyNameHash = UINT64_C(2787301409000765636);
  DwaTestMetric& SetLatency(int64_t value);


  static constexpr char kACTStudyName[] = "ACTStudy";
  static constexpr uint32_t kACTStudyNameHash = UINT32_C(3460185587);

  static constexpr char kLegacyStudyName[] = "LegacyStudy";
  static constexpr uint32_t kLegacyStudyNameHash = UINT32_C(1271574520);

  static constexpr char kModeBStudyName[] = "ModeBStudy";
  static constexpr uint32_t kModeBStudyNameHash = UINT32_C(4194840450);

};


}  // namespace builders
}  // namespace dwa

#endif  // __DWA_BUILDERS_H
""")

  def testBuildersImplementationOutputFromDwaTest(self) -> None:
    builders_impl_output = builders_template.IMPL._StampFileCode(
        self.relpath, self.dwa_test_data)
    self.assertEqual(
        builders_impl_output, """
// Generated from gen_builders.py.  DO NOT EDIT!
// source: dwa.xml

#include ".dwa_builders.h"

#include "base/metrics/metrics_hashes.h"

namespace dwa {
namespace builders {


const char DwaTestMetric::kEntryName[] = "DwaTestMetric";
const uint64_t DwaTestMetric::kEntryNameHash;

DwaTestMetric::DwaTestMetric() :
  ::dwa::internal::DwaEntryBuilderBase(kEntryNameHash) {
  
  AddToStudiesOfInterestInternal(kACTStudyName);

  AddToStudiesOfInterestInternal(kLegacyStudyName);

  AddToStudiesOfInterestInternal(kModeBStudyName);

}

DwaTestMetric::DwaTestMetric(DwaTestMetric&&) = default;

DwaTestMetric& DwaTestMetric::operator=(DwaTestMetric&&) = default;

DwaTestMetric::~DwaTestMetric() = default;

DwaTestMetric& DwaTestMetric::SetContent(std::string_view content) {
  SetContentInternal(base::HashMetricName(content));
  return *this;
}


const char DwaTestMetric::kHasVideoName[] = "HasVideo";
const uint64_t DwaTestMetric::kHasVideoNameHash;

DwaTestMetric& DwaTestMetric::SetHasVideo(int64_t value) {
  SetMetricInternal(kHasVideoNameHash, value);
  return *this;
}

const char DwaTestMetric::kLatencyName[] = "Latency";
const uint64_t DwaTestMetric::kLatencyNameHash;

DwaTestMetric& DwaTestMetric::SetLatency(int64_t value) {
  SetMetricInternal(kLatencyNameHash, value);
  return *this;
}



}  // namespace builders
}  // namespace dwa
""")

  def testDecodeHeaderOutputFromDwaTest(self) -> None:
    decode_header_output = decode_template.HEADER._StampFileCode(
        self.relpath, self.dwa_test_data)
    self.assertEqual(
        decode_header_output, """
// Generated from gen_builders.py.  DO NOT EDIT!
// source: dwa.xml

#ifndef __DWA_DECODE_H
#define __DWA_DECODE_H

#include <cstdint>
#include <map>

namespace dwa {
namespace builders {

typedef std::map<uint64_t, const char*> MetricDecodeMap;
typedef std::map<uint32_t, const char*> StudyDecodeMap;
struct EntryDecoder {
  const char* name;
  const MetricDecodeMap metric_map;
  const StudyDecodeMap study_map;
};
typedef std::map<uint64_t, EntryDecoder> DecodeMap;
DecodeMap CreateDecodeMap();

}  // namespace builders
}  // namespace dwa

#endif  // __DWA_DECODE_H
""")

  def testDecodeImplementationOutputFromDwaTest(self) -> None:
    decode_impl_output = decode_template.IMPL._StampFileCode(
        self.relpath, self.dwa_test_data)
    self.assertEqual(
        decode_impl_output, """
// Generated from gen_builders.py.  DO NOT EDIT!
// source: dwa.xml

#include "./dwa_decode.h"
#include "./dwa_builders.h"

namespace dwa {
namespace builders {

std::map<uint64_t, EntryDecoder> CreateDecodeMap() {
  return {
    
    {
      UINT64_C(10084375266486398523),
      {
        DwaTestMetric::kEntryName,
        {
          
    {DwaTestMetric::kHasVideoNameHash, DwaTestMetric::kHasVideoName},

    {DwaTestMetric::kLatencyNameHash, DwaTestMetric::kLatencyName},

        },
        {
          
    {DwaTestMetric::kACTStudyNameHash, DwaTestMetric::kACTStudyName},

    {DwaTestMetric::kLegacyStudyNameHash, DwaTestMetric::kLegacyStudyName},

    {DwaTestMetric::kModeBStudyNameHash, DwaTestMetric::kModeBStudyName},

        }
      }
    },

  };
}

}  // namespace builders
}  // namespace dwa
""")

if __name__ == '__main__':
  unittest.main()
