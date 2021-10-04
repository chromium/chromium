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

#include "maldoca/service/common/utils.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "maldoca/base/file.h"
#include "maldoca/base/logging.h"
#include "maldoca/base/parse_text_proto.h"
#include "maldoca/base/status.h"
#include "maldoca/base/testing/status_matchers.h"
#include "maldoca/base/testing/test_utils.h"

using ::maldoca::ParsedDocument;
using ::maldoca::testing::ServiceTestFilename;
using ::testing::ElementsAre;
using ::testing::Pair;

namespace maldoca {
namespace utils {
namespace {

#ifndef MALDOCA_CHROME
std::string GetTestContent(absl::string_view filename) {
  std::string content;
  auto status = maldoca::testing::GetTestContents(ServiceTestFilename(filename),
                                                  &content);
  MALDOCA_CHECK_OK(status) << status;
  return content;
}

// TODO(X): add more types in tests
TEST(InferDocType, CorrectInferredType) {
  auto fid = FileTypeIdentifierForDocType();
  MALDOCA_ASSERT_OK(fid);
  ::maldoca::FileTypeIdentifier *identifier = fid->get();
  auto doc = GetTestContent(
      "ffc835c9a950beda17fa79dd0acf28d1df3835232877b5fdd512b3df2ffb2431_xor_"
      "0x42_encoded.doc");
  EXPECT_EQ(DocType::DOC, InferDocType("ffc835c9a950beda17fa79dd0acf28d1df38352"
                                       "32877b5fdd512b3df2ffb2431.doc",
                                       doc, identifier));
  EXPECT_EQ(DocType::DOC, InferDocType("ffc835c9a950beda17fa79dd0acf28d1df38352"
                                       "32877b5fdd512b3df2ffb2431.fake",
                                       doc, identifier));
  doc = GetTestContent(
      "c98661bcd5bd2e5df06d3432890e7a2e8d6a3edcb5f89f6aaa2e5c79d4619f3d.docx");
  EXPECT_EQ(DocType::DOCX, InferDocType("c98661bcd5bd2e5df06d3432890e7a2e8d6a3e"
                                        "dcb5f89f6aaa2e5c79d4619f3d.docx",
                                        doc, identifier));
  EXPECT_EQ(DocType::DOCX, InferDocTypeByContent(doc, identifier));
  doc = GetTestContent(
      "f66b8ee9bea7ec406c6a88ccfb54c447afc3e4c44ae08c071b97beb74b66e2eb.xls");
  EXPECT_EQ(DocType::XLS, InferDocType("test.anyname", doc, identifier));
  EXPECT_EQ(DocType::XLA, InferDocType("test.xla", doc, identifier));

  doc = GetTestContent(
      "ba5c251f78a1d57b72901f4ff80824d6ad0aa4bf1931c593a36254db4ab41021_xor_"
      "0x42_encoded.ppt");
  EXPECT_EQ(DocType::PPT, InferDocType("test.anyname", doc, identifier));
  doc = GetTestContent("image_and_text.pdf");
  EXPECT_EQ(DocType::PDF, InferDocType("test.anyname", doc, identifier));
}

TEST(SortDependencies, CorrectOrder) {
  auto result = SortDependencies({{}, {3, 4}, {}, {0}, {0, 2}, {}});
  EXPECT_OK(result);
  EXPECT_THAT(result.value(), ElementsAre(Pair(0, 0), Pair(2, 0), Pair(5, 1),
                                          Pair(3, 0), Pair(4, 0), Pair(1, 0)));
  // cycle detection
  result = SortDependencies({{1}, {3, 4}, {}, {0}, {0, 2}, {}});
  EXPECT_FALSE(result.ok());
}
#endif  // MALDOCA_CHROME

TEST(DocumentResponseHasVbaScript, ProcessDocumentResponseHasVba) {
  ParsedDocument parsed_doc = ParseTextOrDie<ParsedDocument>(R"pb(
    office {
      parser_output {
        script_features {
          scripts { vba_code {} }
          scripts { excel4_macros {} }
        }
      }
    }
  )pb");
  EXPECT_EQ(true, ParsedDocumentHasVbaScript(parsed_doc));
}

TEST(DocumentResponseHasVbaScript, ProcessDocumentResponseNoOffice) {
  ParsedDocument parsed_doc = ParseTextOrDie<ParsedDocument>(R"pb(
  )pb");
  EXPECT_EQ(false, ParsedDocumentHasVbaScript(parsed_doc));
}

TEST(DocumentResponseHasVbaScript, ProcessDocumentResponseNoVba) {
  ParsedDocument parsed_doc = ParseTextOrDie<ParsedDocument>(R"pb(
    office {
      parser_output { script_features { scripts { excel4_macros {} } } }
    }
  )pb");
  EXPECT_EQ(false, ParsedDocumentHasVbaScript(parsed_doc));
}
}  // namespace
}  // namespace utils
}  // namespace maldoca

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
#ifdef MALDOCA_CHROME
  // mini_chromium needs InitLogging
  maldoca::InitLogging();
#endif
  return RUN_ALL_TESTS();
}
