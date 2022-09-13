// Copyright 2015 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "client/prune_crash_reports.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <algorithm>
#include <random>
#include <string>
#include <vector>

#include "base/numerics/safe_conversions.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/scoped_temp_dir.h"
#include "util/file/file_io.h"

namespace crashpad {
namespace test {
namespace {

class MockDatabase : public CrashReportDatabase {
 public:
  // CrashReportDatabase:
  MOCK_METHOD(Settings*, GetSettings, (), (override));
  MOCK_METHOD(OperationStatus,
              PrepareNewCrashReport,
              (std::unique_ptr<NewReport>*),
              (override));
  MOCK_METHOD(OperationStatus,
              LookUpCrashReport,
              (const UUID&, Report*),
              (override));
  MOCK_METHOD(OperationStatus,
              GetPendingReports,
              (std::vector<Report>*),
              (override));
  MOCK_METHOD(OperationStatus,
              GetCompletedReports,
              (std::vector<Report>*),
              (override));
  MOCK_METHOD(OperationStatus,
              GetReportForUploading,
              (const UUID&,
               std::unique_ptr<const UploadReport>*,
               bool report_metrics),
              (override));
  MOCK_METHOD(OperationStatus,
              RecordUploadAttempt,
              (UploadReport*, bool, const std::string&),
              (override));
  MOCK_METHOD(OperationStatus,
              SkipReportUpload,
              (const UUID&, Metrics::CrashSkippedReason),
              (override));
  MOCK_METHOD(OperationStatus, DeleteReport, (const UUID&), (override));
  MOCK_METHOD(OperationStatus, RequestUpload, (const UUID&), (override));
  MOCK_METHOD(base::FilePath, DatabasePath, (), (override));

  // Google Mock doesn't support mocking methods with non-copyable types such as
  // unique_ptr.
  OperationStatus FinishedWritingCrashReport(std::unique_ptr<NewReport> report,
                                             UUID* uuid) override {
    return kNoError;
  }
};

time_t NDaysAgo(int num_days) {
  return time(nullptr) - (num_days * 60 * 60 * 24);
}

TEST(PruneCrashReports, AgeCondition) {
  CrashReportDatabase::Report report_80_days;
  report_80_days.creation_time = NDaysAgo(80);

  CrashReportDatabase::Report report_10_days;
  report_10_days.creation_time = NDaysAgo(10);

  CrashReportDatabase::Report report_30_days;
  report_30_days.creation_time = NDaysAgo(30);

  AgePruneCondition condition(30);
  EXPECT_TRUE(condition.ShouldPruneReport(report_80_days));
  EXPECT_FALSE(condition.ShouldPruneReport(report_10_days));
  EXPECT_FALSE(condition.ShouldPruneReport(report_30_days));
}

TEST(PruneCrashReports, SizeCondition) {
  CrashReportDatabase::Report report_1k;
  report_1k.total_size = 1024u;
  CrashReportDatabase::Report report_3k;
  report_3k.total_size = 1024u * 3u;
  CrashReportDatabase::Report report_unset_size;

  {
    DatabaseSizePruneCondition condition(/*max_size_in_kb=*/1);
    // |report_1k| should not be pruned as the cumulated size is not past 1kB
    // yet.
    EXPECT_FALSE(condition.ShouldPruneReport(report_1k));
    // |report_3k| should be pruned as the cumulated size is now past 1kB.
    EXPECT_TRUE(condition.ShouldPruneReport(report_3k));
  }

  {
    DatabaseSizePruneCondition condition(/*max_size_in_kb=*/1);
    // |report_3k| should be pruned as the cumulated size is already past 1kB.
    EXPECT_TRUE(condition.ShouldPruneReport(report_3k));
  }

  {
    DatabaseSizePruneCondition condition(/*max_size_in_kb=*/6);
    // |report_3k| should not be pruned as the cumulated size is not past 6kB
    // yet.
    EXPECT_FALSE(condition.ShouldPruneReport(report_3k));
    // |report_3k| should not be pruned as the cumulated size is not past 6kB
    // yet.
    EXPECT_FALSE(condition.ShouldPruneReport(report_3k));
    // |report_1k| should be pruned as the cumulated size is now past 6kB.
    EXPECT_TRUE(condition.ShouldPruneReport(report_1k));
  }

  {
    DatabaseSizePruneCondition condition(/*max_size_in_kb=*/0);
    // |report_unset_size| should not be pruned as its size is 0, regardless of
    // how many times we try to prune it.
    EXPECT_FALSE(condition.ShouldPruneReport(report_unset_size));
    EXPECT_FALSE(condition.ShouldPruneReport(report_unset_size));
    EXPECT_FALSE(condition.ShouldPruneReport(report_unset_size));
    EXPECT_FALSE(condition.ShouldPruneReport(report_unset_size));
    EXPECT_FALSE(condition.ShouldPruneReport(report_unset_size));
    // |report_1k| should be pruned as the cumulated size is now past 0kB.
    EXPECT_TRUE(condition.ShouldPruneReport(report_1k));
  }

  {
    DatabaseSizePruneCondition condition(/*max_size_in_kb=*/6);
    // |report_3k| should not be pruned as the cumulated size is not past 6kB
    // yet.
    EXPECT_FALSE(condition.ShouldPruneReport(report_3k));
    // |report_3k| should not be pruned as the cumulated size is not past 6kB
    // yet.
    EXPECT_FALSE(condition.ShouldPruneReport(report_3k));
    // |report_1k| should be pruned as the cumulated size is now past 6kB.
    EXPECT_TRUE(condition.ShouldPruneReport(report_1k));

    // Reset |measured_size_in_kb_|, which stores the size of reports, to 0.
    condition.ResetPruneConditionState();

    // |report_3k| should not be pruned as the cumulated size is not past 6kB
    // yet.
    EXPECT_FALSE(condition.ShouldPruneReport(report_3k));
    // |report_3k| should not be pruned as the cumulated size is not past 6kB
    // yet.
    EXPECT_FALSE(condition.ShouldPruneReport(report_3k));
    // |report_1k| should be pruned as the cumulated size is now past 6kB.
    EXPECT_TRUE(condition.ShouldPruneReport(report_1k));
  }
}

class StaticCondition final : public PruneCondition {
 public:
  explicit StaticCondition(bool value) : value_(value), did_execute_(false) {}

  StaticCondition(const StaticCondition&) = delete;
  StaticCondition& operator=(const StaticCondition&) = delete;

  ~StaticCondition() {}

  bool ShouldPruneReport(const CrashReportDatabase::Report& report) override {
    did_execute_ = true;
    return value_;
  }

  void ResetPruneConditionState() override {}

  bool did_execute() const { return did_execute_; }

 private:
  const bool value_;
  bool did_execute_;
};

TEST(PruneCrashReports, BinaryCondition) {
  static constexpr struct {
    const char* name;
    BinaryPruneCondition::Operator op;
    bool lhs_value;
    bool rhs_value;
    bool cond_result;
    bool lhs_executed;
    bool rhs_executed;
  } kTests[] = {
      // clang-format off
    {"false && false",
     BinaryPruneCondition::AND, false, false,
     false, true, false},
    {"false && true",
     BinaryPruneCondition::AND, false, true,
     false, true, false},
    {"true && false",
     BinaryPruneCondition::AND, true, false,
     false, true, true},
    {"true && true",
     BinaryPruneCondition::AND, true, true,
     true, true, true},
    {"false || false",
     BinaryPruneCondition::OR, false, false,
     false, true, true},
    {"false || true",
     BinaryPruneCondition::OR, false, true,
     true, true, true},
    {"true || false",
     BinaryPruneCondition::OR, true, false,
     true, true, false},
    {"true || true",
     BinaryPruneCondition::OR, true, true,
     true, true, false},
      // clang-format on
  };
  for (const auto& test : kTests) {
    SCOPED_TRACE(test.name);
    auto lhs = new StaticCondition(test.lhs_value);
    auto rhs = new StaticCondition(test.rhs_value);
    BinaryPruneCondition condition(test.op, lhs, rhs);
    CrashReportDatabase::Report report;
    EXPECT_EQ(condition.ShouldPruneReport(report), test.cond_result);
    EXPECT_EQ(lhs->did_execute(), test.lhs_executed);
    EXPECT_EQ(rhs->did_execute(), test.rhs_executed);
  }
}

MATCHER_P(TestUUID, data_1, "") {
  return arg.data_1 == data_1;
}

TEST(PruneCrashReports, PruneOrder) {
  using ::testing::_;
  using ::testing::DoAll;
  using ::testing::Return;
  using ::testing::SetArgPointee;

  const size_t kNumReports = 10;
  std::vector<CrashReportDatabase::Report> reports;
  for (size_t i = 0; i < kNumReports; ++i) {
    CrashReportDatabase::Report temp;
    temp.uuid.data_1 = static_cast<uint32_t>(i);
    temp.creation_time = NDaysAgo(static_cast<int>(i) * 10);
    reports.push_back(temp);
  }
  std::mt19937 urng(std::random_device{}());
  std::shuffle(reports.begin(), reports.end(), urng);
  std::vector<CrashReportDatabase::Report> pending_reports(reports.begin(),
                                                           reports.begin() + 5);
  std::vector<CrashReportDatabase::Report> completed_reports(
      reports.begin() + 5, reports.end());

  MockDatabase db;
  EXPECT_CALL(db, GetPendingReports(_))
      .WillOnce(DoAll(SetArgPointee<0>(pending_reports),
                      Return(CrashReportDatabase::kNoError)));
  EXPECT_CALL(db, GetCompletedReports(_))
      .WillOnce(DoAll(SetArgPointee<0>(completed_reports),
                      Return(CrashReportDatabase::kNoError)));
  for (size_t i = 0; i < reports.size(); ++i) {
    EXPECT_CALL(db, DeleteReport(TestUUID(i)))
        .WillOnce(Return(CrashReportDatabase::kNoError));
  }

  StaticCondition delete_all(true);
  EXPECT_EQ(PruneCrashReportDatabase(&db, &delete_all), kNumReports);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
