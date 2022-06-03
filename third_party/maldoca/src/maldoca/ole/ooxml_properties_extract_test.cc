// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "maldoca/ole/ooxml_properties_extract.h"

#include <limits>
#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "libxml/parser.h"
#include "libxml/tree.h"
#include "libxml/xpath.h"
#include "maldoca/base/testing/status_matchers.h"
#include "maldoca/ole/oss_utils.h"

namespace maldoca {
using utils::XmlDocDeleter;

namespace {
// Parses the `xml_content` string and tries to extract a value of type `type`
// and place it inside the `value` object. `should_parse` indicates if the
// extraction should be successful or not.
void CheckXmlParsing(absl::string_view xml_content, ole::VariantType type,
                     ole::VtValue* value, bool should_parse = true) {
  *value = ole::VtValue();
  std::unique_ptr<xmlDoc, utils::XmlDocDeleter> xmldoc(
      utils::XmlParseMemory(xml_content.data(), xml_content.size()));
  ASSERT_TRUE(xmldoc);
  auto xmlnode = xmlDocGetRootElement(xmldoc.get());

  auto status_or_value = ExtractVtValueFromXML(type, xmlnode);

  ASSERT_EQ(status_or_value.ok(), should_parse);
  if (should_parse) {
    *value = status_or_value.value();
  }
}

std::string WrapNTag(absl::string_view content) {
  return absl::StrCat("<n>", content, "</n>");
}

// Tests if integer properties are properly parsed.
TEST(OOXMLPropertiesTypeTest, IntegerPropertiesAreParsed) {
  ole::VtValue value;

  // Min / Max values for both I4 and I8 are properly parsed.
  for (int i :
       {std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
    std::string content = std::to_string(i);
    CheckXmlParsing(WrapNTag(content), ole::VariantType::kVtI4, &value);
    EXPECT_TRUE(value.has_int_());
    EXPECT_EQ(value.int_(), i);
  }
  for (int i : {std::numeric_limits<int64_t>::min(),
                std::numeric_limits<int64_t>::max()}) {
    CheckXmlParsing(WrapNTag(std::to_string(i)), ole::VariantType::kVtI8,
                    &value);
    EXPECT_TRUE(value.has_int_());
    EXPECT_EQ(value.int_(), i);
  }

  // Invalid numbers are refused.
  CheckXmlParsing("<n>1a</n>", ole::VariantType::kVtI8, &value, false);
  CheckXmlParsing("<n>--1</n>", ole::VariantType::kVtI8, &value, false);
  CheckXmlParsing("<n>one</n>", ole::VariantType::kVtI8, &value, false);
  CheckXmlParsing("<n>1a</n>", ole::VariantType::kVtI4, &value, false);
  CheckXmlParsing("<n>--1</n>", ole::VariantType::kVtI4, &value, false);
  CheckXmlParsing("<n>one</n>", ole::VariantType::kVtI4, &value, false);
  CheckXmlParsing("<n></n>", ole::VariantType::kVtI4, &value, false);
}

// Tests if unsigned properties are properly parsed.
TEST(OOXMLPropertiesTypeTest, UnsignedPropertiesAreParsed) {
  ole::VtValue value;

  // Max values for UI4 and UI8 are properly parsed.
  std::string content = std::to_string(std::numeric_limits<uint32_t>::max());
  CheckXmlParsing(WrapNTag(content), ole::VariantType::kVtUI4, &value);
  EXPECT_TRUE(value.has_uint());
  EXPECT_EQ(value.uint(), std::numeric_limits<unsigned int>::max());

  content = std::to_string(std::numeric_limits<uint64_t>::max());
  CheckXmlParsing(WrapNTag(content), ole::VariantType::kVtUI8, &value);
  EXPECT_TRUE(value.has_uint());
  EXPECT_EQ(value.uint(), std::numeric_limits<uint64_t>::max());

  // Invalid numbers are refused.
  CheckXmlParsing("<n>-1</n>", ole::VariantType::kVtUI8, &value, false);
  CheckXmlParsing("<n>one</n>", ole::VariantType::kVtUI8, &value, false);
  CheckXmlParsing("<n>1a</n>", ole::VariantType::kVtUI8, &value, false);
  CheckXmlParsing("<n>-1</n>", ole::VariantType::kVtUI4, &value, false);
  CheckXmlParsing("<n>one</n>", ole::VariantType::kVtUI4, &value, false);
  CheckXmlParsing("<n>1a</n>", ole::VariantType::kVtUI4, &value, false);
  CheckXmlParsing("<n></n>", ole::VariantType::kVtUI4, &value, false);
}

// Tests if boolean values are properly parsed.
TEST(OOXMLPropertiesTypeTest, BooleanAreParsed) {
  ole::VtValue value;
  const std::vector<std::pair<bool, std::string>> data = {
      {true, "True"}, {true, "true"}, {false, "False"}, {false, "fAlSe"}};
  for (const auto& d : data) {
    CheckXmlParsing(WrapNTag(d.second), ole::VariantType::kVtBool, &value);
    ASSERT_TRUE(value.has_boolean());
    EXPECT_EQ(value.boolean(), d.first);
  }
  CheckXmlParsing("<n>invalid</n>", ole::VariantType::kVtBool, &value, false);
  CheckXmlParsing("<n></n>", ole::VariantType::kVtBool, &value, false);
}

// Tests if string values are properly parsed.
TEST(OOXMLPropertiesTypeTest, StringsAreParsed) {
  ole::VtValue value;
  CheckXmlParsing("<n>Hello World!</n>", ole::VariantType::kVtLpstr, &value);
  ASSERT_TRUE(value.has_str());
  EXPECT_EQ(value.str(), "Hello World!");
}

// Tests if ole::VariantType::kVtVariant types are properly parsed.
TEST(OOXMLPropertiesTypeTest, OLE_VariantTypeAreParsed) {
  ole::VtValue value;
  std::string message = "<vt:variant><vt:lpstr>Title</vt:lpstr></vt:variant>";
  // Valid message. Type should be detected as lpstr.
  CheckXmlParsing(message, ole::VariantType::kVtVariant, &value);
  ASSERT_TRUE(value.has_str());
  EXPECT_EQ(value.str(), "Title");

  message = "<vt:variant><i8>12345</i8></vt:variant>";
  // Valid message. Type should be detected as int64.
  CheckXmlParsing(message, ole::VariantType::kVtVariant, &value);
  ASSERT_TRUE(value.has_int_());
  EXPECT_EQ(value.int_(), 12345);

  // Checking invalid messages.
  message = "<vt:variant><invalid>Title</invalid></vt:variant>";
  CheckXmlParsing(message, ole::VariantType::kVtVariant, &value, false);

  message =
      "<vt:variant>"
      "<vt:lpstr>Title</vt:lpstr>"
      "<vt:lpstr>Title</vt:lpstr>"
      "</vt:variant>";
  CheckXmlParsing(message, ole::VariantType::kVtVariant, &value, false);

  message = "<vt:variant></vt:variant>";
  CheckXmlParsing(message, ole::VariantType::kVtVariant, &value, false);

  message = "<vt:variant><i4>123abc</i4></vt:variant>";
  CheckXmlParsing(message, ole::VariantType::kVtVariant, &value, false);
}

// Tests if vector values are properly parsed.
TEST(OOXMLPropertiesTypeTest, VectorTypeAreParsed) {
  ole::VtValue value;

  // Vector of variant types.
  std::string message =
      "<n>"
      "<vt:vector size=\"2\" baseType=\"variant\">"
      "<vt:variant><vt:lpstr>Title</vt:lpstr></vt:variant>"
      "<vt:variant><vt:i4>1</vt:i4></vt:variant></vt:vector>"
      "</n>";
  CheckXmlParsing(message, ole::VariantType::kVtVectorVtVariant, &value);
  ASSERT_TRUE(value.has_vector());
  auto vector = value.vector();
  // Check the vector has the required fields.
  ASSERT_EQ(vector.value_size(), 2);
  ASSERT_TRUE(vector.value(0).has_str());
  EXPECT_EQ(vector.value(0).str(), "Title");
  ASSERT_TRUE(vector.value(1).has_int_());
  EXPECT_EQ(vector.value(1).int_(), 1);

  // Vector of strings.
  message =
      "<n>"
      "<vt:vector size=\"2\" baseType=\"lpstr\">"
      "<vt:lpstr>Title</vt:lpstr>"
      "<vt:lpstr>Title2</vt:lpstr></vt:vector>"
      "</n>";
  CheckXmlParsing(message, ole::VariantType::kVtVectorVtLpstr, &value);
  ASSERT_TRUE(value.has_vector());
  vector = value.vector();
  ASSERT_EQ(vector.value_size(), 2);
  ASSERT_TRUE(vector.value(0).has_str() && vector.value(1).has_str());
  EXPECT_EQ(vector.value(0).str(), "Title");
  EXPECT_EQ(vector.value(1).str(), "Title2");

  // Empty vector.
  CheckXmlParsing("<n><vector size=\"0\" baseType=\"variant\"></vector></n>",
                  ole::VariantType::kVtVectorVtVariant, &value);
  EXPECT_TRUE(value.has_vector());
  EXPECT_EQ(value.vector().value_size(), 0);

  // Checking invalid vectors.
  message =
      "<n>"
      "<vt:vector size=\"200\" baseType=\"variant\">"
      "<vt:variant><vt:lpstr>Title</vt:lpstr></vt:variant>"
      "<vt:variant><vt:i4>1</vt:i4></vt:variant></vt:vector>"
      "</n>";
  CheckXmlParsing(message, ole::VariantType::kVtVectorVtVariant, &value);
  // Size is wrong, but the vector should still have 2 elements.
  ASSERT_TRUE(value.has_vector());
  EXPECT_EQ(value.vector().value_size(), 2);

  message =
      "<n>"
      "<vt:vector size=\"abc\" baseType=\"variant\">"
      "<vt:variant><vt:lpstr>Title</vt:lpstr></vt:variant>"
      "</vt:vector>"
      "</n>";
  CheckXmlParsing(message, ole::VariantType::kVtVectorVtVariant, &value);
  // Size is wrong, but the vector should still have 2 elements.
  ASSERT_TRUE(value.has_vector());
  EXPECT_EQ(value.vector().value_size(), 1);

  message =
      "<n>"
      "<vt:vector size=\"200\" baseType=\"variant\">"
      "<vt:variant><vt:i4>Title</vt:i4></vt:variant>"
      "<vt:variant><vt:i4>1</vt:i4></vt:variant></vt:vector>"
      "</n>";
  // Type is wrong, the vector should not parse.
  CheckXmlParsing(message, ole::VariantType::kVtVectorVtVariant, &value, false);

  message =
      "<n>"
      "<vt:vector size=\"200\" baseType=\"variant\">"
      "<vt:lpstr>Title</vt:lpstr>"
      "<vt:variant><vt:i4>1</vt:i4></vt:variant></vt:vector>"
      "</n>";
  // Base vector entry type is not variant, the vector should not parse.
  CheckXmlParsing(message, ole::VariantType::kVtVectorVtVariant, &value, false);
}

// Tests if Blob values are correctly parsed.
TEST(OOXMLPropertiesTypeTest, BlobAreParsed) {
  ole::VtValue value;
  CheckXmlParsing("<n>Hello World!</n>", ole::VariantType::kVtBlob, &value);
  ASSERT_TRUE(value.has_blob());
  ASSERT_TRUE(value.has_blob_hash());
  EXPECT_EQ(value.blob(), "Hello World!");
}

// Tests if SummaryInformation entries are correctly parsed.
TEST(OOXMLPropertiesTest, SummaryInformationIsParsed) {
  std::string message =
      "<Properties>"
      "<Application>text</Application>"
      "<Template>text</Template>"
      "<TotalTime>10</TotalTime>"
      "<Words>10</Words>"
      "<Characters>10</Characters>"
      "<DocSecurity>10</DocSecurity>"
      "<RevisionNumber>text</RevisionNumber>"
      "<dc:title>text</dc:title>"
      "<dc:subject>text</dc:subject>"
      "<dc:description>text</dc:description>"
      "<dc:creator>text</dc:creator>"
      "<dc:description>text</dc:description>"
      "<cp:lastModifiedBy>text</cp:lastModifiedBy>"
      "<cp:revision>text</cp:revision>"
      "<cp:keywords>text</cp:keywords>"
      "<dcterms:created>2020-08-14T10:42:01.2233411Z</dcterms:created>"
      "<dcterms:modified>2020-08-14T10:42:01.2233411Z</dcterms:modified>"
      "<cp:lastPrinted>2020-08-14T10:42:01.2233411Z</cp:lastPrinted>"
      "</Properties>";
  ooxml::OOXMLFile proto;
  proto.mutable_summary_information()->add_property_set()->add_dictionary();
  proto.mutable_document_summary_information()
      ->add_property_set()
      ->add_dictionary();
  std::unique_ptr<xmlDoc, utils::XmlDocDeleter> doc(
      utils::XmlParseMemory(message.c_str(), message.size()));
  ASSERT_TRUE(doc);
  xmlNodePtr node = xmlDocGetRootElement(doc.get())->children;
  ASSERT_TRUE(node != nullptr);

  for (int counter = 0; node; node = node->next) {
    auto status = ExtractOOXMLPropertyToProto(node, &proto, &counter);
    EXPECT_TRUE(status.ok());
  }
}

// Tests if DocumentSummaryInformation entries are correctly parsed.
TEST(OOXMLPropertiesTest, DocumentSummaryInformationIsParsed) {
  std::string message =
      "<Properties>"
      "<Lines>10</Lines>"
      "<Paragraphs>10</Paragraphs>"
      "<Slides>10</Slides>"
      "<Notes>text</Notes>"
      "<HiddenSlides>10</HiddenSlides>"
      "<MMClips>10</MMClips>"
      "<Company>text</Company>"
      "<Manager>text</Manager>"
      "<Language>text</Language>"
      "<Category>text</Category>"
      "<ScaleCrop>True</ScaleCrop>"
      "<CharactersWithSpaces>10</CharactersWithSpaces>"
      "<SharedDoc>False</SharedDoc>"
      "<HyperlinksChanged>False</HyperlinksChanged>"
      "<AppVersion>text</AppVersion>"
      "<PresentationFormat>text</PresentationFormat>"
      "<HeadingPairs>"
      "<vt:vector size=\"200\" baseType=\"variant\">"
      "<vt:variant><vt:lpstr>Title</vt:lpstr></vt:variant>"
      "<vt:variant><vt:i4>1</vt:i4></vt:variant></vt:vector>"
      "</HeadingPairs>"
      "<TitlesOfParts>"
      "<vector size=\"1\" baseType=\"lpstr\">"
      "<lpstr>text</lpstr>"
      "</vector>"
      "</TitlesOfParts>"
      "</Properties>";
  ooxml::OOXMLFile proto;
  proto.mutable_summary_information()->add_property_set()->add_dictionary();
  proto.mutable_document_summary_information()
      ->add_property_set()
      ->add_dictionary();
  std::unique_ptr<xmlDoc, utils::XmlDocDeleter> doc(
      utils::XmlParseMemory(message.c_str(), message.size()));
  ASSERT_TRUE(doc.get() != nullptr);
  xmlNodePtr node = xmlDocGetRootElement(doc.get())->children;
  ASSERT_TRUE(node != nullptr);

  for (int counter = 0; node != nullptr; node = node->next) {
    auto status = ExtractOOXMLPropertyToProto(node, &proto, &counter);
    EXPECT_TRUE(status.ok());
  }
}

}  // namespace
}  // namespace maldoca

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
#ifdef MALDOCA_CHROME
  // mini_chromium needs InitLogging
  maldoca::InitLogging();
#endif
  return RUN_ALL_TESTS();
}
