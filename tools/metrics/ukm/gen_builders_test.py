import unittest

from ukm_model import UKM_XML_TYPE
from ukm_model import _EVENT_TYPE
from ukm_model import _METRIC_TYPE
from codegen import EventInfo
from codegen import MetricInfo
from builders_template import HEADER as BUILDERS_HEADER_TEMPLATE
from builders_template import IMPL as BUILDERS_IMPL_TEMPLATE
from decode_template import HEADER as DECODE_HEADER_TEMPLATE
from decode_template import IMPL as DECODE_IMPL_TEMPLATE


class GenBuildersTest(unittest.TestCase):

  def testGenerateCode(self):
    relpath = '.'
    data = UKM_XML_TYPE.Parse(open('../../tools/metrics/ukm/ukm.xml').read())
    event = data[_EVENT_TYPE.tag][0]
    metric = event[_METRIC_TYPE.tag][0]
    self.assertIsNotNone(event)
    self.assertIsNotNone(metric)
    eventInfo = EventInfo(event)
    metricInfo = MetricInfo(metric)

    # Not using codegen.Template.WriteFile to avoid non-deterministic test
    # behaviour after writing to disk.
    builders_header_output = BUILDERS_HEADER_TEMPLATE._StampFileCode(
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
  explicit {name}(base::UkmSourceId source_id);
  ~{name}() override;

  static const char kEntryName[];
  static constexpr uint64_t kEntryNameHash = UINT64_C({hash});""".format(
            name=eventInfo.name, hash=eventInfo.hash), builders_header_output)

    self.assertIn(
        """
  static const char k{metricName}Name[];
  static constexpr uint64_t k{metricName}NameHash = UINT64_C({metricHash});
  {eventName}& Set{metricName}(int64_t value);
""".format(eventName=eventInfo.name,
           metricName=metricInfo.name,
           metricHash=metricInfo.hash), builders_header_output)

    builders_impl_output = BUILDERS_IMPL_TEMPLATE._StampFileCode(relpath, data)
    self.assertIsNotNone(builders_impl_output)
    self.assertIn("// Generated from gen_builders.py.  DO NOT EDIT!",
                  builders_impl_output)
    self.assertIn("namespace builders", builders_impl_output)
    self.assertIn(
        """
const char {name}::kEntryName[] = "{rawName}";

{name}::{name}(ukm::SourceId source_id) :
  ::ukm::internal::UkmEntryBuilderBase(source_id, kEntryNameHash) {{
}}

{name}::{name}(base::UkmSourceId source_id) :
  ::ukm::internal::UkmEntryBuilderBase(source_id, kEntryNameHash) {{
}}""".format(name=eventInfo.name, rawName=eventInfo.raw_name),
        builders_impl_output)

    self.assertIn(
        """
const char {eventName}::k{metricName}Name[] = "{metricRawName}";

{eventName}& {eventName}::Set{metricName}(int64_t value) {{
  SetMetricInternal(k{metricName}NameHash, value);
  return *this;
}}
""".format(eventName=eventInfo.name,
           metricName=metricInfo.name,
           metricRawName=metricInfo.raw_name), builders_impl_output)

    decode_header_output = DECODE_HEADER_TEMPLATE._StampFileCode(relpath, data)
    self.assertIsNotNone(decode_header_output)
    self.assertIn("// Generated from gen_builders.py.  DO NOT EDIT!",
                  decode_header_output)
    self.assertIn("namespace builders", decode_header_output)
    self.assertIn(
        """typedef std::map<uint64_t, const char*> MetricDecodeMap;
struct EntryDecoder {
  const char* name;
  const MetricDecodeMap metric_map;
};
typedef std::map<uint64_t, EntryDecoder> DecodeMap;
DecodeMap CreateDecodeMap();""", decode_header_output)

    decode_impl_output = DECODE_IMPL_TEMPLATE._StampFileCode(relpath, data)
    self.assertIsNotNone(decode_impl_output)
    self.assertIn("// Generated from gen_builders.py.  DO NOT EDIT!",
                  decode_impl_output)
    self.assertIn("namespace builders", decode_impl_output)
    self.assertIn(
        """
    {{{eventName}::k{metricName}NameHash, {eventName}::k{metricName}Name}},"""
        .format(eventName=eventInfo.name,
                metricName=metricInfo.name), decode_impl_output)


if __name__ == '__main__':
  unittest.main()
