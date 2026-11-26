// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/rtc_base/logging.h"

#include <cstdint>
#include <ostream>

#include "base/test/mock_log.h"
#include "base/test/scoped_logging_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc_overrides/rtc_base/diagnostic_logging.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Return;

std::optional<std::string> g_last_log_message;

void DoLog(const std::string& message) {
  g_last_log_message = message;
}

class LoggingTest : public ::testing::Test {
 public:
  void SetUp() override {
    g_last_log_message.reset();
    InitDiagnosticLoggingDelegateFunction(&DoLog);
  }
};

TEST_F(LoggingTest, LogMessage) {
  RTC_LOG(LS_INFO) << "Hello World";
  EXPECT_EQ(g_last_log_message, "Hello World");
}

struct TestStructWithOstream {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
  friend std::ostream& operator<<(std::ostream& os, TestStructWithOstream) {
    os << "TestStructWithOstream";
    return os;
  }
#pragma clang diagnostic pop
};

TEST_F(LoggingTest, LogMessageUsesOstreamOperator) {
  TestStructWithOstream s;
  RTC_LOG(LS_INFO) << s;
  EXPECT_EQ(g_last_log_message, "TestStructWithOstream");
}

struct TestStruct {
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const TestStruct& s) {
    sink.Append("TestStruct");
  }
};

TEST_F(LoggingTest, LogMessageUsesAbslStringify) {
  TestStruct s;
  RTC_LOG(LS_INFO) << s;
  EXPECT_EQ(g_last_log_message, "TestStruct");
}

enum class TestEnum : uint8_t {
  kFoo = 1,
};

template <typename Sink>
void AbslStringify(Sink& sink, TestEnum e) {
  sink.Append("Foo");
}

TEST_F(LoggingTest, UsesAbslStringifyOnTrivialConvertibleEnums) {
  RTC_LOG(LS_INFO) << TestEnum::kFoo;
  EXPECT_EQ(g_last_log_message, "Foo");
}

int CountCall(int& calls) {
  return ++calls;
}

TEST_F(LoggingTest, EvaluatesOperandsWhenLogging) {
  int calls = 0;
  RTC_LOG(LS_INFO) << CountCall(calls);
  EXPECT_EQ(calls, 1);
}

TEST_F(LoggingTest, DoesNotEvaluateOperandsWhenNotLogging) {
  int calls = 0;
  RTC_LOG(LS_VERBOSE) << CountCall(calls);
  EXPECT_EQ(calls, 0);
  EXPECT_EQ(g_last_log_message, std::nullopt);
}

TEST_F(LoggingTest, EvaluatesAndLogsVerboseOperandsWhenVlogIsOn) {
  if (kVerboseVlogDisabled) {
    GTEST_SKIP() << "Verbose vlog is compiled out.";
  }
  logging::ScopedVmoduleSwitches vmodule;
  vmodule.InitWithSwitches("logging_test=1");
  base::test::MockLog log;
  EXPECT_CALL(log, Log(_, _, _, _, HasSubstr("verbose 1")))
      .WillOnce(Return(true));
  log.StartCapturingLogs();

  int calls = 0;
  RTC_LOG(LS_VERBOSE) << "verbose " << CountCall(calls);
  EXPECT_EQ(calls, 1);
  EXPECT_EQ(g_last_log_message, std::nullopt);
}

TEST_F(LoggingTest, RtcLogVEvaluatesOperandsUpToInfo) {
  LoggingSeverity severity = LS_INFO;
  int calls = 0;
  RTC_LOG_V(severity) << CountCall(calls);
  EXPECT_EQ(calls, 1);
}

TEST_F(LoggingTest, RtcLogVSkipsVerboseOperandsWhenVlogIsOff) {
  LoggingSeverity severity = LS_VERBOSE;
  int calls = 0;
  RTC_LOG_V(severity) << CountCall(calls);
  EXPECT_EQ(calls, 0);
}

TEST_F(LoggingTest, RtcLogIfEvaluatesOperandsOnlyWhenConditionHoldsAndLogging) {
  int calls = 0;
  RTC_LOG_IF(LS_INFO, false) << CountCall(calls);
  EXPECT_EQ(calls, 0);
  RTC_LOG_IF(LS_INFO, true) << CountCall(calls);
  EXPECT_EQ(calls, 1);
  RTC_LOG_IF(LS_VERBOSE, true) << CountCall(calls);
  EXPECT_EQ(calls, 1);
}

}  // namespace
}  // namespace webrtc
