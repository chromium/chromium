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

#include "maldoca/ole/ole_to_proto.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "maldoca/base/file.h"
#include "maldoca/base/testing/test_utils.h"
#include "maldoca/base/status.h"
#include "maldoca/base/testing/protocol-buffer-matchers.h"
#include "maldoca/base/testing/status_matchers.h"

namespace {
using ::maldoca::ole::OleFile;
using ::maldoca::ole::OleToProtoSettings;
using ::maldoca::testing::EqualsProto;
using ::maldoca::testing::proto::IgnoringRepeatedFieldOrdering;
using ::testing::Eq;
using ::testing::IsTrue;
using ::testing::SizeIs;
using ::testing::StrEq;

std::string TestFilename(absl::string_view filename) {
  return maldoca::testing::OleTestFilename(filename, "ole/");
}

std::string GetTestContent(absl::string_view filename) {
  std::string content;
  auto status =
      maldoca::testing::GetTestContents(TestFilename(filename), &content);
  MALDOCA_EXPECT_OK(status) << status;
  return content;
}

maldoca::StatusOr<OleFile> GetOleStatusOrProto(
    absl::string_view filename, bool include_vba, bool include_hashes,
    bool include_dirs, bool include_summary, bool include_olenative_metadata,
    bool include_olenative_content, bool include_strings,
    bool include_excel4_macros) {
  std::string content = GetTestContent(filename);

  OleToProtoSettings settings;
  settings.set_include_directory_structure(include_dirs);
  settings.set_include_olenative_content(include_olenative_content);
  settings.set_include_olenative_metadata(include_olenative_metadata);
  settings.set_include_stream_hashes(include_hashes);
  settings.set_include_summary_information(include_summary);
  settings.set_include_unknown_strings(include_strings);
  settings.set_include_vba_code(include_vba);
  settings.set_include_excel4_macros(include_excel4_macros);
  return maldoca::GetOleFileProto(content, settings);
}

OleFile GetOleProto(absl::string_view filename, bool include_vba,
                    bool include_hashes, bool include_dirs,
                    bool include_summary, bool include_olenative_metadata,
                    bool include_olenative_content, bool include_strings,
                    bool include_excel4_macros) {
  auto status_or_proto = GetOleStatusOrProto(
      filename, include_vba, include_hashes, include_dirs, include_summary,
      include_olenative_metadata, include_olenative_content, include_strings,
      include_excel4_macros);
  MALDOCA_EXPECT_OK(status_or_proto);
  return status_or_proto.value();
}

void CheckOleProto(absl::string_view test_name, absl::string_view test_suffix,
                   const OleFile &ole_proto) {
  OleFile expected_ole_proto;
  auto status = maldoca::file::GetTextProto(
      TestFilename(absl::StrCat(test_name, "_", test_suffix, ".textproto")),
      &expected_ole_proto);
  MALDOCA_EXPECT_OK(status) << status;

  EXPECT_THAT(ole_proto,
              IgnoringRepeatedFieldOrdering(EqualsProto(expected_ole_proto)))
      << ", test_name: " << test_name
      << "\n ole_proto: " << ole_proto.DebugString()
      << "\n expected proto: " << expected_ole_proto.DebugString();
}

class OleToProtoTest
    : public ::testing::TestWithParam<
          ::testing::tuple<const char *, const char *, bool, bool, bool, bool,
                           bool, bool, bool, bool>> {
 public:
  OleToProtoTest()
      : file_name_(::testing::get<0>(GetParam())),
        test_name_(::testing::get<1>(GetParam())),
        include_vba_(::testing::get<2>(GetParam())),
        include_hashes_(::testing::get<3>(GetParam())),
        include_dirs_(::testing::get<4>(GetParam())),
        include_summary_(::testing::get<5>(GetParam())),
        include_olenative_metadata_(::testing::get<6>(GetParam())),
        include_olenative_content_(::testing::get<7>(GetParam())),
        include_strings_(::testing::get<8>(GetParam())),
        include_excel4_macros_(::testing::get<9>(GetParam())) {}
  const char *file_name_;
  const char *test_name_;
  bool include_vba_;
  bool include_hashes_;
  bool include_dirs_;
  bool include_summary_;
  bool include_olenative_metadata_;
  bool include_olenative_content_;
  bool include_strings_;
  bool include_excel4_macros_;
};

TEST_P(OleToProtoTest, Samples) {
  CheckOleProto(
      file_name_, test_name_,
      GetOleProto(file_name_, include_vba_, include_hashes_, include_dirs_,
                  include_summary_, include_olenative_metadata_,
                  include_olenative_content_, include_strings_,
                  include_excel4_macros_));
}

// TODO(b/186442398): Create test data for Excel files that use addin functions
// and OLE links (embedded files).
INSTANTIATE_TEST_SUITE_P(
    OleToProtoTestSamples, OleToProtoTest,
    ::testing::Values(
        std::make_tuple("ffc835c9a950beda17fa79dd0acf28d1df3835232877b5fdd512b3"
                        "df2ffb2431_xor_0x42_encoded",
                        "all", true, true, true, true, true, true, false, true),
        std::make_tuple("ffc835c9a950beda17fa79dd0acf28d1df3835232877b5fdd512b3"
                        "df2ffb2431_xor_0x42_encoded",
                        "with_strings", true, true, true, true, true, true,
                        true, true),
        std::make_tuple("ffc835c9a950beda17fa79dd0acf28d1df3835232877b5fdd512b3"
                        "df2ffb2431_xor_0x42_encoded",
                        "nodirs", true, true, /*include_dirs*/ false, true,
                        true, true, false, true),
        std::make_tuple("ffc835c9a950beda17fa79dd0acf28d1df3835232877b5fdd512b3"
                        "df2ffb2431_xor_0x42_encoded",
                        "novba",
                        /*include_vba*/ false, true, true, true, true, true,
                        false, true),
        std::make_tuple("ffc835c9a950beda17fa79dd0acf28d1df3835232877b5fdd512b3"
                        "df2ffb2431_xor_0x42_encoded",
                        "nosummary", true, true, true,
                        /*include_summary*/ false, true, true, false, true),
        std::make_tuple("ffc835c9a950beda17fa79dd0acf28d1df3835232877b5fdd512b3"
                        "df2ffb2431_xor_0x42_encoded",
                        "nohashes", true, /*include_hashes*/ false, true, true,
                        true, true, false, true)
#ifndef MALDOCA_CHROME
            ,
        std::make_tuple("name.xls", "excel4", false, false,
                        /*include_dirs*/ true, false, false, false, false,
                        /*include_excel4_macros*/ true),
        std::make_tuple("name.xls", "noexcel4", true, true, true, true, true,
                        true, true,
                        /*include_excel4_macros*/ false),
        std::make_tuple("external_refs.xls", "excel4", true, true, true, true,
                        true, true, true,
                        /*include_excel4_macros*/ true),
        std::make_tuple("extraction_errors_base.xls", "excel4", true, true,
                        true, true, true, true, true,
                        /*include_excel4_macros*/ true),
        std::make_tuple("extraction_errors_corrupted_boundsheet.xls", "excel4",
                        true, true, true, true, true, true, true,
                        /*include_excel4_macros*/ true),
        std::make_tuple("extraction_errors_corrupted_externalbook.xls",
                        "excel4", true, true, true, true, true, true, true,
                        /*include_excel4_macros*/ true),
        std::make_tuple("extraction_errors_corrupted_externalname.xls",
                        "excel4", true, true, true, true, true, true, true,
                        /*include_excel4_macros*/ true),
        std::make_tuple("extraction_errors_corrupted_externalname_formula.xls",
                        "excel4", true, true, true, true, true, true, true,
                        /*include_excel4_macros*/ true),
        std::make_tuple("extraction_errors_corrupted_externsheet.xls", "excel4",
                        true, true, true, true, true, true, true,
                        /*include_excel4_macros*/ true),
        std::make_tuple("extraction_errors_corrupted_formula.xls", "excel4",
                        true, true, true, true, true, true, true,
                        /*include_excel4_macros*/ true),
        std::make_tuple("extraction_errors_corrupted_formularecord.xls",
                        "excel4", true, true, true, true, true, true, true,
                        /*include_excel4_macros*/ true),
        std::make_tuple("extraction_errors_corrupted_lbl.xls", "excel4", true,
                        true, true, true, true, true, true,
                        /*include_excel4_macros*/ true),
        std::make_tuple("extraction_errors_corrupted_lbl_formula.xls", "excel4",
                        true, true, true, true, true, true, true,
                        /*include_excel4_macros*/ true),
        std::make_tuple("encrypted_workbook_12345.xls", "excel4", true, true,
                        true, true, true, true, true,
                        /*include_excel4_macros*/ true),
        std::make_tuple("encrypted_no_formulas_12345.xls", "excel4", false,
                        false, /*include_dirs*/ true, false, false, false,
                        false,
                        /*include_excel4_macros*/ true),
        std::make_tuple("missing_formula.xls", "excel4", false, false, false,
                        false, false, false, false,
                        /*include_excel4_macros*/ true)
#endif  // MALDOCA_CHROME
            ));

TEST(OleToProtoTest, OlePidHlinks) {
  OleFile ole_file = GetOleProto(
      "f3897d9509bd8f6bbee6e39568fb694aa05f3dc83ccf5eedcfabda21b48332ee", true,
      true, true, true, false, false, false, false);
  EXPECT_THAT(ole_file.has_reserved_properties(), IsTrue());
  ASSERT_THAT(ole_file.reserved_properties().pid_hlinks(), SizeIs(1));

  ASSERT_THAT(ole_file.reserved_properties().pid_hlinks(0).has_hlink2(),
              IsTrue());
  EXPECT_THAT(ole_file.reserved_properties().pid_hlinks(0).hlink2(), StrEq(""));
}

TEST(OleToProtoTest, OleHwpSummaryInfo) {
  OleFile ole_file = GetOleProto(
      "7050af905f1696b2b8cdb4c6e6805a618addf5acfbd4edc3fc807a663016ab26_xor_"
      "0x42_encoded",
      true, true, true, true, false, false, false, false);

  static constexpr int kNumberOfProperties = 13;

  // This test is supposed to check if HWP summary information parsing is
  // enabled, so checking only one property should be more than enough
  static constexpr int kPropertyIndex = 7;
  static constexpr const char *kPropertyValue =
      "9, 0, 0, 562 WIN32LEWindows_Unknown_Version";

  EXPECT_THAT(ole_file.has_summary_information(), IsTrue());
  EXPECT_THAT(ole_file.summary_information().property_set(0).property_size(),
              Eq(kNumberOfProperties));
  EXPECT_THAT(ole_file.summary_information()
                  .property_set(0)
                  .property(kPropertyIndex)
                  .value()
                  .str(),
              StrEq(kPropertyValue));
}

TEST(OleToProtoTest, OleNativeEmbedded) {
  OleFile ole_file = GetOleProto(
      "f674740dfdf4fd4ded529c339160c8255cdd971c4a00180c9e3fc3f3e7b53799_xor_"
      "0x42_encoded",
      false, false, false, false, true, true, false, false);

  EXPECT_THAT(ole_file.has_olenative_embedded(), IsTrue());

  // check olenative embedded package metadata is parsed correctly
  EXPECT_THAT(ole_file.olenative_embedded().has_native_size(), IsTrue());
  EXPECT_THAT(ole_file.olenative_embedded().has_type(), IsTrue());
  EXPECT_THAT(ole_file.olenative_embedded().has_file_name(), IsTrue());
  EXPECT_THAT(ole_file.olenative_embedded().has_file_path(), IsTrue());
  EXPECT_THAT(ole_file.olenative_embedded().has_reserved_unknown(), IsTrue());
  EXPECT_THAT(ole_file.olenative_embedded().has_temp_path(), IsTrue());
  EXPECT_THAT(ole_file.olenative_embedded().native_size(), Eq(398850));
  EXPECT_THAT(ole_file.olenative_embedded().type(), Eq(2));
  EXPECT_THAT(ole_file.olenative_embedded().file_name(), StrEq("11.zip"));
  EXPECT_THAT(ole_file.olenative_embedded().file_path(),
              StrEq("C:\\1\\11.zip"));
  EXPECT_THAT(ole_file.olenative_embedded().temp_path(),
              StrEq("C:\\Users\\ADMINI~1\\AppData\\Local\\Temp\\11.zip"));
  EXPECT_THAT(
      ole_file.olenative_embedded().file_hash(),
      StrEq(
          "17816a20524f7131306759b4f0d663a3e854bd9f5df9ea86d37e1395384fad20"));

  // check olenative embedded packager file content was parsed correctly
  EXPECT_THAT(ole_file.olenative_embedded().has_file_content(), IsTrue());
  EXPECT_THAT(ole_file.olenative_embedded().file_content().length(),
              Eq(398641));
  EXPECT_TRUE(absl::StartsWith(ole_file.olenative_embedded().file_content(),
                               "PK\x03\x04\x14"))
      << ole_file.olenative_embedded().file_content();
}

TEST(OleToProtoTest, Regression148270725) {
  // b/148270725
  // this used to crash test case
  OleFile ole_proto = GetOleProto("testcase-5487200440418304", true, true, true,
                                  true, true, true, true, false);
}

TEST(OleToProtoTest, Regression148270343) {
  // b/148270343
  // this used to crash test case (with msan enabled)
  OleFile ole_proto = GetOleProto("testcase-6493403182268416", true, true, true,
                                  true, true, true, true, false);
}

TEST(OleToProtoTest, StatusPayload) {
  auto status_or = GetOleStatusOrProto("bogus_ole", true, true, true, true,
                                       true, true, true, true);
  EXPECT_FALSE(status_or.ok());
  auto error = status_or.status().GetPayload(::maldoca::kMaldocaStatusType);
  ASSERT_TRUE(error.has_value());
  ASSERT_EQ("EMPTY_FAT_HEADER", std::string(error.value()));
  LOG(INFO) << "StatusPayload_status" << status_or.status();
}
}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
#ifdef MALDOCA_CHROME
  // mini_chromium needs InitLogging
  maldoca::InitLogging();
#endif
  return RUN_ALL_TESTS();
}
