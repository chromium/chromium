// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/traffic_annotation/auditor/traffic_annotation_auditor.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/traffic_annotation/auditor/traffic_annotation_exporter.h"
#include "tools/traffic_annotation/auditor/traffic_annotation_file_filter.h"
#include "tools/traffic_annotation/auditor/traffic_annotation_id_checker.h"

namespace {

#define TEST_HASH_CODE(X)                                  \
  EXPECT_EQ(TrafficAnnotationAuditor::ComputeHashValue(X), \
            net::DefineNetworkTrafficAnnotation(X, "").unique_id_hash_code)

const char* kIrrelevantFiles[] = {
    "tools/traffic_annotation/auditor/tests/git_list.txt",
    "tools/traffic_annotation/auditor/tests/irrelevant_file_content.cc",
    "tools/traffic_annotation/auditor/tests/irrelevant_file_content.mm",
    "tools/traffic_annotation/auditor/tests/irrelevant_file_name.txt"};

const char* kRelevantFiles[] = {
    "tools/traffic_annotation/auditor/tests/relevant_file_name_and_content.cc",
    "tools/traffic_annotation/auditor/tests/relevant_file_name_and_content.mm"};

const base::FilePath kTestsFolder =
    base::FilePath(FILE_PATH_LITERAL("tools"))
        .Append(FILE_PATH_LITERAL("traffic_annotation"))
        .Append(FILE_PATH_LITERAL("auditor"))
        .Append(FILE_PATH_LITERAL("tests"));

const base::FilePath kClangToolPath =
    base::FilePath(FILE_PATH_LITERAL("tools"))
        .Append(FILE_PATH_LITERAL("traffic_annotation/bin"));

const std::set<int> kDummyDeprecatedIDs = {100, 101, 102};
}  // namespace

class TrafficAnnotationAuditorTest : public ::testing::Test {
 public:
  void SetUp() override {
    if (!base::PathService::Get(base::DIR_SOURCE_ROOT, &source_path_)) {
      LOG(ERROR) << "Could not get current directory to find source path.";
      return;
    }

    tests_folder_ = source_path_.Append(kTestsFolder);

#if defined(OS_WIN)
    base::FilePath platform_name(FILE_PATH_LITERAL("win32"));
#elif defined(OS_LINUX)
    base::FilePath platform_name(FILE_PATH_LITERAL("linux64"));
#elif defined(OS_MACOSX)
    base::FilePath platform_name(FILE_PATH_LITERAL("mac"));
#else
    NOTREACHED() << "Unexpected platform.";
#endif

    base::FilePath clang_tool_path =
        source_path_.Append(kClangToolPath).Append(platform_name);
    std::vector<std::string> path_filters;

    // As build path is not available and not used in tests, the default (empty)
    // build path is passed to auditor.
    auditor_ = std::make_unique<TrafficAnnotationAuditor>(
        source_path_,
        source_path_.Append(FILE_PATH_LITERAL("out"))
            .Append(FILE_PATH_LITERAL("Default")),
        clang_tool_path, path_filters);

    id_checker_ = std::make_unique<TrafficAnnotationIDChecker>(
        TrafficAnnotationAuditor::GetReservedIDsSet(), kDummyDeprecatedIDs);
  }

  const base::FilePath source_path() const { return source_path_; }
  const base::FilePath tests_folder() const { return tests_folder_; }
  TrafficAnnotationAuditor& auditor() { return *auditor_; }
  TrafficAnnotationIDChecker& id_checker() { return *id_checker_; }
  std::vector<AuditorResult>* errors() { return &errors_; }

 protected:
  // Deserializes an annotation or a call instance from a sample file similar to
  // clang tool outputs.
  AuditorResult::Type Deserialize(const std::string& file_name,
                                  InstanceBase* instance);

  // Creates a complete annotation instance using sample files.
  AnnotationInstance CreateAnnotationInstanceSample();
  AnnotationInstance CreateAnnotationInstanceSample(
      AnnotationInstance::Type type,
      int unique_id);
  AnnotationInstance CreateAnnotationInstanceSample(
      AnnotationInstance::Type type,
      int unique_id,
      int second_id);

  void SetAnnotationForTesting(const AnnotationInstance& instance) {
    std::vector<AnnotationInstance> annotations;
    annotations.push_back(instance);
    auditor_->SetExtractedAnnotationsForTesting(annotations);
    auditor_->ClearErrorsForTesting();
  }

  void RunIDChecker(const AnnotationInstance& instance) {
    std::vector<AnnotationInstance> annotations;
    annotations.push_back(instance);
    errors_.clear();
    id_checker_->Load(annotations);
    id_checker_->CheckIDs(&errors_);
  }

 private:
  base::FilePath source_path_;
  base::FilePath tests_folder_;
  std::unique_ptr<TrafficAnnotationAuditor> auditor_;
  std::unique_ptr<TrafficAnnotationIDChecker> id_checker_;
  std::vector<AuditorResult> errors_;
};

AuditorResult::Type TrafficAnnotationAuditorTest::Deserialize(
    const std::string& file_name,
    InstanceBase* instance) {
  std::string file_content;
  EXPECT_TRUE(base::ReadFileToString(
      tests_folder_.Append(FILE_PATH_LITERAL("extractor_outputs"))
          .AppendASCII(file_name),
      &file_content))
      << file_name;
  base::RemoveChars(file_content, "\r", &file_content);
  std::vector<std::string> lines = base::SplitString(
      file_content, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  return instance->Deserialize(lines, 0, static_cast<int>(lines.size())).type();
}

AnnotationInstance
TrafficAnnotationAuditorTest::CreateAnnotationInstanceSample() {
  AnnotationInstance instance;
  EXPECT_EQ(Deserialize("good_complete_annotation.txt", &instance),
            AuditorResult::Type::RESULT_OK);
  return instance;
}

AnnotationInstance TrafficAnnotationAuditorTest::CreateAnnotationInstanceSample(
    AnnotationInstance::Type type,
    int unique_id) {
  return CreateAnnotationInstanceSample(type, unique_id, 0);
}

AnnotationInstance TrafficAnnotationAuditorTest::CreateAnnotationInstanceSample(
    AnnotationInstance::Type type,
    int unique_id,
    int second_id) {
  AnnotationInstance instance = CreateAnnotationInstanceSample();
  instance.type = type;
  instance.unique_id_hash_code = unique_id;
  instance.proto.set_unique_id(base::StringPrintf("S%i", unique_id));
  if (second_id) {
    instance.second_id = base::StringPrintf("S%i", second_id);
    instance.second_id_hash_code = second_id;
  } else {
    instance.second_id.clear();
    instance.second_id_hash_code = 0;
  }
  return instance;
}

// Tests if the two hash computation functions have the same result.
TEST_F(TrafficAnnotationAuditorTest, HashFunctionCheck) {
  TEST_HASH_CODE("test");
  TEST_HASH_CODE("unique_id");
  TEST_HASH_CODE("123_id");
  TEST_HASH_CODE("ID123");
  TEST_HASH_CODE(
      "a_unique_looooooooooooooooooooooooooooooooooooooooooooooooooooooong_id");
}

// Tests if TrafficAnnotationFileFilter::GetFilesFromGit function returns
// correct files given a mock git list file. It also inherently checks
// TrafficAnnotationFileFilter::IsFileRelevant.
TEST_F(TrafficAnnotationAuditorTest, GetFilesFromGit) {
  TrafficAnnotationFileFilter filter;
  filter.SetGitFileForTesting(
      tests_folder().Append(FILE_PATH_LITERAL("git_list.txt")));
  filter.GetFilesFromGit(source_path());

  const std::vector<std::string> git_files = filter.git_files();

  EXPECT_EQ(git_files.size(), base::size(kRelevantFiles));
  for (const char* filepath : kRelevantFiles) {
    EXPECT_TRUE(base::Contains(git_files, filepath));
  }

  for (const char* filepath : kIrrelevantFiles) {
    EXPECT_FALSE(base::Contains(git_files, filepath));
  }
}

// Tests if TrafficAnnotationFileFilter::GetRelevantFiles gives the correct list
// of files, given a mock git list file.
TEST_F(TrafficAnnotationAuditorTest, RelevantFilesReceived) {
  TrafficAnnotationFileFilter filter;
  filter.SetGitFileForTesting(
      tests_folder().Append(FILE_PATH_LITERAL("git_list.txt")));
  filter.GetFilesFromGit(source_path());

  unsigned int git_files_count = filter.git_files().size();

  std::vector<std::string> ignore_list;
  std::vector<std::string> file_paths;

  // Check if all files are returned with no ignore list and directory.
  filter.GetRelevantFiles(base::FilePath(), ignore_list, "", &file_paths);
  EXPECT_EQ(file_paths.size(), git_files_count);

  // Check if a file is ignored if it is added to ignore list.
  ignore_list.push_back(file_paths[0]);
  file_paths.clear();
  filter.GetRelevantFiles(base::FilePath(), ignore_list, "", &file_paths);
  EXPECT_EQ(file_paths.size(), git_files_count - 1);
  EXPECT_FALSE(base::Contains(file_paths, ignore_list[0]));

  // Check if files are filtered based on given directory.
  ignore_list.clear();
  file_paths.clear();
  filter.GetRelevantFiles(base::FilePath(), ignore_list,
                          "tools/traffic_annotation", &file_paths);
  EXPECT_EQ(file_paths.size(), git_files_count);
  file_paths.clear();
  filter.GetRelevantFiles(base::FilePath(), ignore_list, "content",
                          &file_paths);
  EXPECT_EQ(file_paths.size(), 0u);
}

// Tests if TrafficAnnotationFileFilter::IsSafeListed works as expected.
// Inherently checks if TrafficAnnotationFileFilter::LoadSafeList works and
// AuditorException rules are correctly deserialized.
TEST_F(TrafficAnnotationAuditorTest, IsSafeListed) {
  for (unsigned int i = 0;
       i < static_cast<unsigned int>(
               AuditorException::ExceptionType::EXCEPTION_TYPE_LAST);
       i++) {
    AuditorException::ExceptionType type =
        static_cast<AuditorException::ExceptionType>(i);
    // Anything in /tools directory is safelisted for all types.
    EXPECT_TRUE(auditor().IsSafeListed("tools/something.cc", type));
    EXPECT_TRUE(auditor().IsSafeListed("tools/somewhere/something.mm", type));

    // Anything in a general folder is not safelisted for any type
    EXPECT_FALSE(auditor().IsSafeListed("something.cc", type));
    EXPECT_FALSE(auditor().IsSafeListed("content/something.mm", type));
  }

  // Files defining missing annotation functions in net/ are exceptions of
  // 'missing' type.
  EXPECT_TRUE(auditor().IsSafeListed("net/url_request/url_fetcher.cc",
                                     AuditorException::ExceptionType::MISSING));
  EXPECT_TRUE(auditor().IsSafeListed("net/url_request/url_request_context.cc",
                                     AuditorException::ExceptionType::MISSING));

  // Files having the word test in their full path can have annotation for
  // tests.
  EXPECT_FALSE(
      auditor().IsSafeListed("net/url_request/url_fetcher.cc",
                             AuditorException::ExceptionType::TEST_ANNOTATION));
  EXPECT_TRUE(
      auditor().IsSafeListed("chrome/browser/test_something.cc",
                             AuditorException::ExceptionType::TEST_ANNOTATION));
  EXPECT_TRUE(
      auditor().IsSafeListed("test/send_something.cc",
                             AuditorException::ExceptionType::TEST_ANNOTATION));
}

// Tests if annotation instances are correctly deserialized.
TEST_F(TrafficAnnotationAuditorTest, AnnotationDeserialization) {
  struct AnnotationSample {
    std::string file_name;
    AuditorResult::Type result_type;
    AnnotationInstance::Type type;
  };

  AnnotationSample test_cases[] = {
      {"good_complete_annotation.txt", AuditorResult::Type::RESULT_OK,
       AnnotationInstance::Type::ANNOTATION_COMPLETE},
      {"good_branched_completing_annotation.txt",
       AuditorResult::Type::RESULT_OK,
       AnnotationInstance::Type::ANNOTATION_BRANCHED_COMPLETING},
      {"good_completing_annotation.txt", AuditorResult::Type::RESULT_OK,
       AnnotationInstance::Type::ANNOTATION_COMPLETING},
      {"good_partial_annotation.txt", AuditorResult::Type::RESULT_OK,
       AnnotationInstance::Type::ANNOTATION_PARTIAL},
      {"good_test_annotation.txt", AuditorResult::Type::ERROR_TEST_ANNOTATION},
      {"missing_annotation.txt", AuditorResult::Type::ERROR_MISSING_TAG_USED},
      {"fatal_annotation1.txt", AuditorResult::Type::ERROR_FATAL},
      {"fatal_annotation2.txt", AuditorResult::Type::ERROR_FATAL},
      {"fatal_annotation3.txt", AuditorResult::Type::ERROR_FATAL},
      {"bad_syntax_annotation1.txt", AuditorResult::Type::ERROR_SYNTAX},
      {"bad_syntax_annotation2.txt", AuditorResult::Type::ERROR_SYNTAX},
      {"bad_syntax_annotation3.txt", AuditorResult::Type::ERROR_SYNTAX},
      {"bad_syntax_annotation4.txt", AuditorResult::Type::ERROR_SYNTAX},
  };

  for (const auto& test_case : test_cases) {
    // Check if deserialization result is as expected.
    AnnotationInstance annotation;
    AuditorResult::Type result_type =
        Deserialize(test_case.file_name, &annotation);
    EXPECT_EQ(result_type, test_case.result_type) << test_case.file_name;

    if (result_type == AuditorResult::Type::RESULT_OK)
      EXPECT_EQ(annotation.type, test_case.type);

    // Content checks for one complete sample.
    if (test_case.file_name != "good_complete_annotation.txt")
      continue;

    EXPECT_EQ(annotation.proto.unique_id(),
              "supervised_user_refresh_token_fetcher");
    EXPECT_EQ(annotation.proto.source().file(),
              "chrome/browser/supervised_user/legacy/"
              "supervised_user_refresh_token_fetcher.cc");
    EXPECT_EQ(annotation.proto.source().line(), 166);
    EXPECT_EQ(annotation.proto.semantics().sender(), "Supervised Users");
    EXPECT_EQ(annotation.proto.policy().cookies_allowed(), 1);
  }
}

// Tests if call instances are correctly deserialized.
TEST_F(TrafficAnnotationAuditorTest, CallDeserialization) {
  struct CallSample {
    std::string file_name;
    AuditorResult::Type result_type;
  };

  CallSample test_cases[] = {
      {"good_call.txt", AuditorResult::Type::RESULT_OK},
      {"bad_call.txt", AuditorResult::Type::ERROR_FATAL},
  };

  for (const auto& test_case : test_cases) {
    // Check if deserialization result is as expected.
    CallInstance call;
    AuditorResult::Type result_type = Deserialize(test_case.file_name, &call);
    EXPECT_EQ(result_type, test_case.result_type);

    // Content checks for one complete sample.
    if (test_case.file_name != "good_call.txt")
      continue;

    EXPECT_EQ(call.file_path, "headless/public/util/http_url_fetcher.cc");
    EXPECT_EQ(call.line_number, 100u);
    EXPECT_EQ(call.function_name, "net::URLRequestContext::CreateRequest");
    EXPECT_EQ(call.is_annotated, true);
  }
}

// Tests if call instances are correctly deserialized.
TEST_F(TrafficAnnotationAuditorTest, AssignmentDeserialization) {
  struct Assignmentample {
    std::string file_name;
    AuditorResult::Type result_type;
  };

  Assignmentample test_cases[] = {
      {"good_assignment.txt", AuditorResult::Type::RESULT_OK},
      {"bad_assignment.txt", AuditorResult::Type::ERROR_FATAL},
  };

  for (const auto& test_case : test_cases) {
    // Check if deserialization result is as expected.
    AssignmentInstance assignment;
    AuditorResult::Type result_type =
        Deserialize(test_case.file_name, &assignment);
    SCOPED_TRACE(test_case.file_name);
    EXPECT_EQ(result_type, test_case.result_type);
  }
}

// Tests if TrafficAnnotationAuditor::GetReservedIDsMap has all known ids and
// they have correct text.
TEST_F(TrafficAnnotationAuditorTest, GetReservedIDsCoverage) {
  int expected_ids[] = {
      TRAFFIC_ANNOTATION_FOR_TESTS.unique_id_hash_code,
      PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS.unique_id_hash_code,
      MISSING_TRAFFIC_ANNOTATION.unique_id_hash_code};

  std::map<int, std::string> reserved_words =
      TrafficAnnotationAuditor::GetReservedIDsMap();

  for (int id : expected_ids) {
    EXPECT_TRUE(base::Contains(reserved_words, id));
    EXPECT_EQ(id, TrafficAnnotationAuditor::ComputeHashValue(
                      reserved_words.find(id)->second));
  }
}

// Tests if use of reserved ids are detected.
TEST_F(TrafficAnnotationAuditorTest, CheckReservedIDsUsageDetection) {
  for (const auto& reserved_id : auditor().GetReservedIDsSet()) {
    RunIDChecker(CreateAnnotationInstanceSample(
        AnnotationInstance::Type::ANNOTATION_COMPLETE, reserved_id));
    EXPECT_EQ(1u, errors()->size());
    EXPECT_EQ(AuditorResult::Type::ERROR_RESERVED_ID_HASH_CODE,
              (*errors())[0].type());

    RunIDChecker(CreateAnnotationInstanceSample(
        AnnotationInstance::Type::ANNOTATION_PARTIAL, 1, reserved_id));
    EXPECT_EQ(1u, errors()->size());
    EXPECT_EQ(AuditorResult::Type::ERROR_RESERVED_ID_HASH_CODE,
              (*errors())[0].type());
  }
}

// Tests if use of deprecated ids are detected.
TEST_F(TrafficAnnotationAuditorTest, CheckDeprecatedIDsUsageDetection) {
  for (const auto& deprecated_id : kDummyDeprecatedIDs) {
    RunIDChecker(CreateAnnotationInstanceSample(
        AnnotationInstance::Type::ANNOTATION_COMPLETE, deprecated_id));
    EXPECT_EQ(1u, errors()->size());
    EXPECT_EQ(AuditorResult::Type::ERROR_DEPRECATED_ID_HASH_CODE,
              (*errors())[0].type());

    RunIDChecker(CreateAnnotationInstanceSample(
        AnnotationInstance::Type::ANNOTATION_PARTIAL, 1, deprecated_id));
    EXPECT_EQ(1u, errors()->size());
    EXPECT_EQ(AuditorResult::Type::ERROR_DEPRECATED_ID_HASH_CODE,
              (*errors())[0].type());
  }
}

// Tests if use of repeated ids are detected.
TEST_F(TrafficAnnotationAuditorTest, CheckRepeatedIDsDetection) {
  std::vector<AnnotationInstance> annotations;

  // Check if several different hash codes result in no error.
  for (int i = 0; i < 10; i++) {
    annotations.push_back(CreateAnnotationInstanceSample(
        AnnotationInstance::Type::ANNOTATION_COMPLETE, i));
  }
  id_checker().Load(annotations);
  errors()->clear();
  id_checker().CheckIDs(errors());
  EXPECT_EQ(0u, errors()->size());

  // Check if repeating the same hash codes results in errors.
  annotations.clear();
  for (int i = 0; i < 10; i++) {
    annotations.push_back(CreateAnnotationInstanceSample(
        AnnotationInstance::Type::ANNOTATION_COMPLETE, i));
    annotations.push_back(annotations.back());
  }
  id_checker().Load(annotations);
  errors()->clear();
  id_checker().CheckIDs(errors());
  EXPECT_EQ(10u, errors()->size());
  for (const auto& error : *errors()) {
    EXPECT_EQ(error.type(), AuditorResult::Type::ERROR_REPEATED_ID);
  }
}

// Tests if having the same unique id and second id is detected.
TEST_F(TrafficAnnotationAuditorTest, CheckSimilarUniqueAndSecondIDsDetection) {
  const int last_type =
      static_cast<int>(AnnotationInstance::Type::ANNOTATION_INSTANCE_TYPE_LAST);
  for (int type = 0; type <= last_type; type++) {
    AnnotationInstance instance = CreateAnnotationInstanceSample(
        static_cast<AnnotationInstance::Type>(type), 1, 1);
    RunIDChecker(instance);
    EXPECT_EQ(instance.NeedsTwoIDs() ? 1u : 0u, errors()->size()) << type;
  }
}

// Tests Unique id and Second id collision cases.
TEST_F(TrafficAnnotationAuditorTest, CheckDuplicateIDsDetection) {
  const int last_type =
      static_cast<int>(AnnotationInstance::Type::ANNOTATION_INSTANCE_TYPE_LAST);

  for (int type1 = 0; type1 < last_type; type1++) {
    for (int type2 = type1; type2 <= last_type; type2++) {
      for (int id1 = 1; id1 < 5; id1++) {
        for (int id2 = 1; id2 < 5; id2++) {
          for (int id3 = 1; id3 < 5; id3++) {
            for (int id4 = 1; id4 < 5; id4++) {
              SCOPED_TRACE(
                  base::StringPrintf("Testing (%i, %i, %i, %i, %i, %i).", type1,
                                     type2, id1, id2, id3, id4));

              AnnotationInstance instance1 = CreateAnnotationInstanceSample(
                  static_cast<AnnotationInstance::Type>(type1), id1, id2);
              AnnotationInstance instance2 = CreateAnnotationInstanceSample(
                  static_cast<AnnotationInstance::Type>(type2), id3, id4);
              std::vector<AnnotationInstance> annotations;
              annotations.push_back(instance1);
              annotations.push_back(instance2);
              id_checker().Load(annotations);
              errors()->clear();
              id_checker().CheckIDs(errors());

              std::set<int> unique_ids;
              bool first_needs_two = instance1.NeedsTwoIDs();
              bool second_needs_two = instance2.NeedsTwoIDs();
              unique_ids.insert(id1);
              if (first_needs_two)
                unique_ids.insert(id2);
              unique_ids.insert(id3);
              if (second_needs_two)
                unique_ids.insert(id4);

              if (first_needs_two && second_needs_two) {
                // If both need two ids, either the 4 ids should be different,
                // or the second ids should be equal and both annotations should
                // be of types partial or branched completing.
                if (unique_ids.size() == 4) {
                  EXPECT_TRUE(errors()->empty());
                } else if (unique_ids.size() == 3) {
                  bool acceptable =
                      (id2 == id4) &&
                      (type1 ==
                           static_cast<int>(
                               AnnotationInstance::Type::ANNOTATION_PARTIAL) ||
                       type1 == static_cast<int>(
                                    AnnotationInstance::Type::
                                        ANNOTATION_BRANCHED_COMPLETING)) &&
                      (type2 ==
                           static_cast<int>(
                               AnnotationInstance::Type::ANNOTATION_PARTIAL) ||
                       type2 == static_cast<int>(
                                    AnnotationInstance::Type::
                                        ANNOTATION_BRANCHED_COMPLETING));

                  EXPECT_EQ(acceptable, errors()->empty());
                } else {
                  EXPECT_FALSE(errors()->empty());
                }
              } else if (first_needs_two && !second_needs_two) {
                // If just the first one needs two ids, then either the 3 ids
                // should be different or the first annotation would be partial
                // and the second completing, with one common id.
                if (unique_ids.size() == 3) {
                  EXPECT_TRUE(errors()->empty());
                } else if (unique_ids.size() == 2) {
                  bool acceptable =
                      (id2 == id3) &&
                      (type1 ==
                           static_cast<int>(
                               AnnotationInstance::Type::ANNOTATION_PARTIAL) &&
                       type2 == static_cast<int>(AnnotationInstance::Type::
                                                     ANNOTATION_COMPLETING));
                  EXPECT_EQ(errors()->empty(), acceptable);
                } else {
                  EXPECT_FALSE(errors()->empty());
                }
              } else if (!first_needs_two && second_needs_two) {
                // Can only be valid if all 3 are different.
                EXPECT_EQ(unique_ids.size() == 3, errors()->empty());
              } else {
                // If none requires two ids, it can only be valid if ids are
                // different.
                EXPECT_EQ(unique_ids.size() == 2, errors()->empty());
                }
              }
          }
        }
      }
    }
  }
}

// Tests if IDs format is correctly checked.
TEST_F(TrafficAnnotationAuditorTest, CheckIDsFormat) {
  std::map<std::string, bool> test_cases = {
      {"ID1", true},   {"id2", true},   {"Id_3", true},
      {"ID?4", false}, {"ID:5", false}, {"ID>>6", false},
  };

  AnnotationInstance instance = CreateAnnotationInstanceSample();
  for (const auto& test_case : test_cases) {
    // Set type to complete to require just unique id.
    instance.type = AnnotationInstance::Type::ANNOTATION_COMPLETE;
    instance.proto.set_unique_id(test_case.first);
    instance.unique_id_hash_code = 1;
    RunIDChecker(instance);
    EXPECT_EQ(test_case.second ? 0u : 1u, errors()->size()) << test_case.first;

    // Set type to partial to require both ids.
    instance.type = AnnotationInstance::Type::ANNOTATION_PARTIAL;
    instance.proto.set_unique_id("Something_Good");
    instance.second_id = test_case.first;
    instance.unique_id_hash_code = 1;
    instance.second_id_hash_code = 2;
    RunIDChecker(instance);
    EXPECT_EQ(test_case.second ? 0u : 1u, errors()->size()) << test_case.first;
  }

  // Test all cases together.
  std::vector<AnnotationInstance> annotations;
  instance.type = AnnotationInstance::Type::ANNOTATION_COMPLETE;
  instance.unique_id_hash_code = 1;

  unsigned int false_samples_count = 0;
  for (const auto& test_case : test_cases) {
    instance.proto.set_unique_id(test_case.first);
    instance.unique_id_hash_code++;
    annotations.push_back(instance);
    if (!test_case.second)
      false_samples_count++;
  }
  id_checker().Load(annotations);
  errors()->clear();
  id_checker().CheckIDs(errors());
  EXPECT_EQ(false_samples_count, errors()->size());
}

// Tests if TrafficAnnotationAuditor::CheckAllRequiredFunctionsAreAnnotated
// results are as expected. It also inherently checks
// TrafficAnnotationAuditor::CheckIfCallCanBeUnannotated.
TEST_F(TrafficAnnotationAuditorTest, CheckAllRequiredFunctionsAreAnnotated) {
  std::string file_paths[] = {"net/url_request/url_fetcher.cc",
                              "net/url_request/url_request_context.cc",
                              "net/url_request/other_file.cc",
                              "somewhere_else.cc", "something_unittest.cc"};
  std::vector<CallInstance> calls(1);
  CallInstance& call = calls[0];

  for (const std::string& file_path : file_paths) {
    for (int annotated = 0; annotated < 2; annotated++) {
      for (int dependent = 0; dependent < 2; dependent++) {
        SCOPED_TRACE(base::StringPrintf(
            "Testing (%s, %i, %i).", file_path.c_str(), annotated, dependent));
        call.file_path = file_path;
        call.function_name = "net::URLFetcher::Create";
        call.is_annotated = annotated;
        auditor().SetGnFileForTesting(tests_folder().Append(
            dependent ? FILE_PATH_LITERAL("gn_list_positive.txt")
                      : FILE_PATH_LITERAL("gn_list_negative.txt")));

        auditor().ClearErrorsForTesting();
        auditor().SetExtractedCallsForTesting(calls);
        auditor().ClearCheckedDependenciesForTesting();
        auditor().CheckAllRequiredFunctionsAreAnnotated();
        // Error should be issued if all the following is met:
        //   1- Function is not annotated.
        //   2- chrome::chrome depends on it.
        //   3- The filepath is not safelisted.
        bool is_unittest = file_path.find("unittest") != std::string::npos;
        bool is_safelist =
            file_path == "net/url_request/url_fetcher.cc" ||
            file_path == "net/url_request/url_request_context.cc" ||
            is_unittest;
        EXPECT_EQ(auditor().errors().size() == 1,
                  !annotated && dependent && !is_safelist)
            << base::StringPrintf(
                   "Annotated:%i, Depending:%i, IsUnitTest:%i, "
                   "IsSafeListed:%i",
                   annotated, dependent, is_unittest, is_safelist);
      }
    }
  }
}

// Tests if TrafficAnnotationAuditor::CheckAnnotationsContents works as
// expected for COMPLETE annotations. It also inherently checks
// TrafficAnnotationAuditor::IsAnnotationComplete and
// TrafficAnnotationAuditor::IsAnnotationConsistent.
TEST_F(TrafficAnnotationAuditorTest, CheckCompleteAnnotations) {
  AnnotationInstance instance = CreateAnnotationInstanceSample();
  std::vector<AnnotationInstance> annotations;
  unsigned int expected_errors_count = 0;

  for (int test_no = 0;; test_no++) {
    AnnotationInstance test_case = instance;
    bool expect_error = true;
    std::string test_description;
    test_case.unique_id_hash_code = test_no;
    switch (test_no) {
      case 0:
        test_description = "All fields OK.";
        expect_error = false;
        break;
      case 1:
        test_description = "Missing semantics::sender.";
        test_case.proto.mutable_semantics()->clear_sender();
        break;
      case 2:
        test_description = "Missing semantics::description.";
        test_case.proto.mutable_semantics()->clear_description();
        break;
      case 3:
        test_description = "Missing semantics::trigger.";
        test_case.proto.mutable_semantics()->clear_trigger();
        break;
      case 4:
        test_description = "Missing semantics::data.";
        test_case.proto.mutable_semantics()->clear_data();
        break;
      case 5:
        test_description = "Missing semantics::destination.";
        test_case.proto.mutable_semantics()->clear_destination();
        break;
      case 6:
        test_description = "Missing policy::cookies_allowed.";
        test_case.proto.mutable_policy()->set_cookies_allowed(
            traffic_annotation::
                NetworkTrafficAnnotation_TrafficPolicy_CookiesAllowed_UNSPECIFIED);
        break;
      case 7:
        test_description =
            "policy::cookies_allowed = NO with existing policy::cookies_store.";
        test_case.proto.mutable_policy()->set_cookies_allowed(
            traffic_annotation::
                NetworkTrafficAnnotation_TrafficPolicy_CookiesAllowed_NO);
        test_case.proto.mutable_policy()->set_cookies_store("somewhere");
        break;
      case 8:
        test_description =
            "policy::cookies_allowed = NO and no policy::cookies_store.";
        test_case.proto.mutable_policy()->set_cookies_allowed(
            traffic_annotation::
                NetworkTrafficAnnotation_TrafficPolicy_CookiesAllowed_NO);
        test_case.proto.mutable_policy()->clear_cookies_store();
        expect_error = false;
        break;
      case 9:
        test_description =
            "policy::cookies_allowed = YES and policy::cookies_store exists.";
        test_case.proto.mutable_policy()->set_cookies_allowed(
            traffic_annotation::
                NetworkTrafficAnnotation_TrafficPolicy_CookiesAllowed_YES);
        test_case.proto.mutable_policy()->set_cookies_store("somewhere");
        expect_error = false;
        break;
      case 10:
        test_description =
            "policy::cookies_allowed = YES and no policy::cookies_store.";
        test_case.proto.mutable_policy()->set_cookies_allowed(
            traffic_annotation::
                NetworkTrafficAnnotation_TrafficPolicy_CookiesAllowed_YES);
        test_case.proto.mutable_policy()->clear_cookies_store();
        break;
      case 11:
        test_description = "Missing policy::settings.";
        test_case.proto.mutable_policy()->clear_setting();
        break;
      case 12:
        test_description =
            "Missing policy::chrome_policy and "
            "policy::policy_exception_justification.";
        test_case.proto.mutable_policy()->clear_chrome_policy();
        test_case.proto.mutable_policy()
            ->clear_policy_exception_justification();
        break;
      case 13:
        test_description =
            "Missing policy::chrome_policy and existing "
            "policy::policy_exception_justification.";
        test_case.proto.mutable_policy()->clear_chrome_policy();
        test_case.proto.mutable_policy()->set_policy_exception_justification(
            "Because!");
        expect_error = false;
        break;
      case 14:
        test_description =
            "Existing policy::chrome_policy and no "
            "policy::policy_exception_justification.";
        test_case.proto.mutable_policy()->add_chrome_policy();
        test_case.proto.mutable_policy()
            ->clear_policy_exception_justification();
        expect_error = false;
        break;
      case 15:
        test_description =
            "Existing policy::chrome_policy and existing"
            "policy::policy_exception_justification.";
        test_case.proto.mutable_policy()->add_chrome_policy();
        test_case.proto.mutable_policy()->set_policy_exception_justification(
            "Because!");
        break;

      default:
        // Trigger stop.
        test_no = -1;
        break;
    }
    if (test_no < 0)
      break;
    SCOPED_TRACE(base::StringPrintf("Testing: %s", test_description.c_str()));
    SetAnnotationForTesting(test_case);
    auditor().CheckAnnotationsContents();

    EXPECT_EQ(auditor().errors().size(), expect_error ? 1u : 0u);
    annotations.push_back(test_case);
    if (expect_error)
      expected_errors_count++;
  }

  // Check All.
  auditor().SetExtractedAnnotationsForTesting(annotations);
  auditor().ClearErrorsForTesting();
  auditor().CheckAnnotationsContents();
  EXPECT_EQ(auditor().errors().size(), expected_errors_count);
}

// Tests if AnnotationInstance::IsCompletableWith works as expected.
TEST_F(TrafficAnnotationAuditorTest, IsCompletableWith) {
  const int last_type =
      static_cast<int>(AnnotationInstance::Type::ANNOTATION_INSTANCE_TYPE_LAST);
  for (int type1 = 0; type1 < last_type; type1++) {
    for (int type2 = 0; type2 <= last_type; type2++) {
      // Iterate all combination of common/specified ids.
      for (int ids = 0; ids < 256; ids++) {
        AnnotationInstance instance1 = CreateAnnotationInstanceSample(
            static_cast<AnnotationInstance::Type>(type1), ids % 4,
            (ids >> 2) % 4);
        AnnotationInstance instance2 = CreateAnnotationInstanceSample(
            static_cast<AnnotationInstance::Type>(type2), (ids >> 4) % 4,
            (ids >> 6));

        bool expectation = false;
        // It's compatible only if the first one is partial and has second_id,
        // and the second one is either completing with matching unique id, or
        // branched completing with matching second id.
        if (instance1.type == AnnotationInstance::Type::ANNOTATION_PARTIAL &&
            !instance1.second_id.empty()) {
          expectation |=
              (instance2.type ==
                   AnnotationInstance::Type::ANNOTATION_COMPLETING &&
               instance1.second_id_hash_code == instance2.unique_id_hash_code);
          expectation |=
              (instance2.type ==
                   AnnotationInstance::Type::ANNOTATION_BRANCHED_COMPLETING &&
               instance1.second_id_hash_code == instance2.second_id_hash_code);
        }
        EXPECT_EQ(instance1.IsCompletableWith(instance2), expectation);
      }
    }
  }
}

// Tests if AnnotationInstance::CreateCompleteAnnotation works as
// expected.
TEST_F(TrafficAnnotationAuditorTest, CreateCompleteAnnotation) {
  AnnotationInstance instance = CreateAnnotationInstanceSample();
  AnnotationInstance other = instance;

  instance.proto.clear_semantics();
  other.proto.clear_policy();

  AnnotationInstance combination;

  // Partial and Completing.
  instance.type = AnnotationInstance::Type::ANNOTATION_PARTIAL;
  other.type = AnnotationInstance::Type::ANNOTATION_COMPLETING;
  instance.second_id_hash_code = 1;
  instance.second_id = "SomeID";
  other.unique_id_hash_code = 1;
  EXPECT_EQ(instance.CreateCompleteAnnotation(other, &combination).type(),
            AuditorResult::Type::RESULT_OK);
  EXPECT_EQ(combination.unique_id_hash_code, instance.unique_id_hash_code);

  // Partial and Branched Completing.
  other.type = AnnotationInstance::Type::ANNOTATION_BRANCHED_COMPLETING;
  instance.second_id_hash_code = 1;
  other.second_id_hash_code = 1;
  other.second_id = "SomeID";
  EXPECT_EQ(instance.CreateCompleteAnnotation(other, &combination).type(),
            AuditorResult::Type::RESULT_OK);
  EXPECT_EQ(combination.unique_id_hash_code, other.unique_id_hash_code);

  // Inconsistent field.
  other = instance;
  other.type = AnnotationInstance::Type::ANNOTATION_BRANCHED_COMPLETING;
  instance.second_id_hash_code = 1;
  other.second_id_hash_code = 1;
  other.second_id = "SomeID";
  instance.proto.mutable_semantics()->set_destination(
      traffic_annotation::
          NetworkTrafficAnnotation_TrafficSemantics_Destination_WEBSITE);
  other.proto.mutable_semantics()->set_destination(
      traffic_annotation::
          NetworkTrafficAnnotation_TrafficSemantics_Destination_LOCAL);
  EXPECT_NE(instance.CreateCompleteAnnotation(other, &combination).type(),
            AuditorResult::Type::RESULT_OK);
}

// Tests if Annotations.xml has proper content.
TEST_F(TrafficAnnotationAuditorTest, AnnotationsXML) {
  TrafficAnnotationExporter exporter(source_path());

  EXPECT_TRUE(exporter.LoadAnnotationsXML());
  exporter.CheckArchivedAnnotations(errors());
  EXPECT_TRUE(errors()->empty());
}

// Tests if 'annotations.xml' is read and has at least one item.
TEST_F(TrafficAnnotationAuditorTest, AnnotationsXMLLines) {
  TrafficAnnotationExporter exporter(source_path());
  EXPECT_LE(1u, exporter.GetXMLItemsCountForTesting());
}

// Tests if 'annotations.xml' changes are correctly reported.
TEST_F(TrafficAnnotationAuditorTest, AnnotationsXMLDifferences) {
  TrafficAnnotationExporter exporter(source_path());

  std::string xml1;
  std::string xml2;
  std::string xml3;

  EXPECT_TRUE(base::ReadFileToString(
      base::MakeAbsoluteFilePath(
          tests_folder().Append(FILE_PATH_LITERAL("annotations_sample1.xml"))),
      &xml1));
  EXPECT_TRUE(base::ReadFileToString(
      base::MakeAbsoluteFilePath(
          tests_folder().Append(FILE_PATH_LITERAL("annotations_sample2.xml"))),
      &xml2));
  EXPECT_TRUE(base::ReadFileToString(
      base::MakeAbsoluteFilePath(
          tests_folder().Append(FILE_PATH_LITERAL("annotations_sample3.xml"))),
      &xml3));

  std::string diff12 = exporter.GetXMLDifferencesForTesting(xml1, xml2);
  std::string diff13 = exporter.GetXMLDifferencesForTesting(xml1, xml3);
  std::string diff23 = exporter.GetXMLDifferencesForTesting(xml2, xml3);

  std::string expected_diff12;
  std::string expected_diff13;
  std::string expected_diff23;

  EXPECT_TRUE(base::ReadFileToString(
      base::MakeAbsoluteFilePath(
          tests_folder().Append(FILE_PATH_LITERAL("annotations_diff12.txt"))),
      &expected_diff12));
  EXPECT_TRUE(base::ReadFileToString(
      base::MakeAbsoluteFilePath(
          tests_folder().Append(FILE_PATH_LITERAL("annotations_diff13.txt"))),
      &expected_diff13));
  EXPECT_TRUE(base::ReadFileToString(
      base::MakeAbsoluteFilePath(
          tests_folder().Append(FILE_PATH_LITERAL("annotations_diff23.txt"))),
      &expected_diff23));

  EXPECT_EQ(diff12, expected_diff12);
  EXPECT_EQ(diff13, expected_diff13);
  EXPECT_EQ(diff23, expected_diff23);
}
