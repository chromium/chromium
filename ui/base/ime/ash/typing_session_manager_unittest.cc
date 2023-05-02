// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/typing_session_manager.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

class TypingSessionManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    base::Time now = base::Time::Now();
    test_clock_.SetNow(now);
  }

 public:
  TypingSessionManagerTest() : typing_session_manager_(&test_clock_) {}

  base::SimpleTestClock test_clock_;
  TypingSessionManager typing_session_manager_;
};

TEST_F(TypingSessionManagerTest, RecordMetricsForSimpleTypingSession) {
  base::HistogramTester histogram_tester;

  histogram_tester.ExpectTotalCount("InputMethod.SessionDuration", 0);
  histogram_tester.ExpectTotalCount("InputMethod.CharactersPerSession", 0);

  typing_session_manager_.CommitCharacters(15);
  test_clock_.Advance(base::Seconds(1));
  typing_session_manager_.CommitCharacters(20);
  test_clock_.Advance(base::Seconds(2));

  typing_session_manager_.EndAndRecordSession();

  histogram_tester.ExpectTotalCount("InputMethod.SessionDuration", 1);
  histogram_tester.ExpectUniqueSample("InputMethod.SessionDuration", 1000, 1);
  histogram_tester.ExpectTotalCount("InputMethod.CharactersPerSession", 1);
  histogram_tester.ExpectUniqueSample("InputMethod.CharactersPerSession", 35,
                                      1);

  typing_session_manager_.CommitCharacters(25);
  test_clock_.Advance(base::Milliseconds(500));
  test_clock_.Advance(base::Seconds(1));
  typing_session_manager_.CommitCharacters(30);
  test_clock_.Advance(base::Seconds(2));

  typing_session_manager_.EndAndRecordSession();

  histogram_tester.ExpectTotalCount("InputMethod.SessionDuration", 2);
  histogram_tester.ExpectBucketCount("InputMethod.SessionDuration", 1500, 1);
  histogram_tester.ExpectTotalCount("InputMethod.CharactersPerSession", 2);
  histogram_tester.ExpectBucketCount("InputMethod.CharactersPerSession", 55, 1);
}

TEST_F(TypingSessionManagerTest,
       RecordMetricsForAutomaticallyEndedTypingSession) {
  base::HistogramTester histogram_tester;

  histogram_tester.ExpectTotalCount("InputMethod.SessionDuration", 0);
  histogram_tester.ExpectTotalCount("InputMethod.CharactersPerSession", 0);

  typing_session_manager_.CommitCharacters(15);
  test_clock_.Advance(base::Seconds(1));
  typing_session_manager_.CommitCharacters(20);
  test_clock_.Advance(base::Seconds(4));

  typing_session_manager_.CommitCharacters(25);

  histogram_tester.ExpectTotalCount("InputMethod.SessionDuration", 1);
  histogram_tester.ExpectUniqueSample("InputMethod.SessionDuration", 1000, 1);
  histogram_tester.ExpectTotalCount("InputMethod.CharactersPerSession", 1);
  histogram_tester.ExpectUniqueSample("InputMethod.CharactersPerSession", 35,
                                      1);

  test_clock_.Advance(base::Milliseconds(500));
  test_clock_.Advance(base::Seconds(1));
  typing_session_manager_.CommitCharacters(30);
  test_clock_.Advance(base::Seconds(4));

  typing_session_manager_.Heartbeat();

  histogram_tester.ExpectTotalCount("InputMethod.SessionDuration", 2);
  histogram_tester.ExpectBucketCount("InputMethod.SessionDuration", 1500, 1);
  histogram_tester.ExpectTotalCount("InputMethod.CharactersPerSession", 2);
  histogram_tester.ExpectBucketCount("InputMethod.CharactersPerSession", 55, 1);
}

TEST_F(TypingSessionManagerTest, DoNotRecordInvalidTypingSessionManuallyEnded) {
  base::HistogramTester histogram_tester;

  histogram_tester.ExpectTotalCount("InputMethod.SessionDuration", 0);
  histogram_tester.ExpectTotalCount("InputMethod.CharactersPerSession", 0);

  typing_session_manager_.CommitCharacters(1);
  test_clock_.Advance(base::Seconds(1));
  typing_session_manager_.CommitCharacters(2);
  test_clock_.Advance(base::Seconds(2));

  typing_session_manager_.EndAndRecordSession();

  histogram_tester.ExpectTotalCount("InputMethod.SessionDuration", 0);
  histogram_tester.ExpectTotalCount("InputMethod.CharactersPerSession", 0);

  typing_session_manager_.CommitCharacters(3);
  test_clock_.Advance(base::Milliseconds(500));
  test_clock_.Advance(base::Seconds(1));
  typing_session_manager_.CommitCharacters(4);
  test_clock_.Advance(base::Seconds(2));

  typing_session_manager_.EndAndRecordSession();

  histogram_tester.ExpectTotalCount("InputMethod.SessionDuration", 0);
  histogram_tester.ExpectTotalCount("InputMethod.CharactersPerSession", 0);
}

TEST_F(TypingSessionManagerTest,
       DoNotRecordInvalidTypingSessionAutomaticallyEnded) {
  base::HistogramTester histogram_tester;

  histogram_tester.ExpectTotalCount("InputMethod.SessionDuration", 0);
  histogram_tester.ExpectTotalCount("InputMethod.CharactersPerSession", 0);

  typing_session_manager_.CommitCharacters(1);
  test_clock_.Advance(base::Seconds(1));
  typing_session_manager_.CommitCharacters(2);
  test_clock_.Advance(base::Seconds(4));

  typing_session_manager_.CommitCharacters(3);

  histogram_tester.ExpectTotalCount("InputMethod.SessionDuration", 0);
  histogram_tester.ExpectTotalCount("InputMethod.CharactersPerSession", 0);

  test_clock_.Advance(base::Milliseconds(500));
  test_clock_.Advance(base::Seconds(1));
  typing_session_manager_.CommitCharacters(4);
  test_clock_.Advance(base::Seconds(4));

  typing_session_manager_.Heartbeat();

  histogram_tester.ExpectTotalCount("InputMethod.SessionDuration", 0);
  histogram_tester.ExpectTotalCount("InputMethod.CharactersPerSession", 0);
}

TEST_F(TypingSessionManagerTest, DoNotRecordTooShortTypingSession) {
  base::HistogramTester histogram_tester;

  histogram_tester.ExpectTotalCount("InputMethod.SessionDuration", 0);
  histogram_tester.ExpectTotalCount("InputMethod.CharactersPerSession", 0);

  typing_session_manager_.CommitCharacters(15);
  test_clock_.Advance(base::Milliseconds(500));
  typing_session_manager_.CommitCharacters(20);
  test_clock_.Advance(base::Seconds(2));

  typing_session_manager_.EndAndRecordSession();

  histogram_tester.ExpectTotalCount("InputMethod.SessionDuration", 0);
  histogram_tester.ExpectTotalCount("InputMethod.CharactersPerSession", 0);

  typing_session_manager_.CommitCharacters(25);
  test_clock_.Advance(base::Milliseconds(500));
  test_clock_.Advance(base::Seconds(1));
  typing_session_manager_.CommitCharacters(30);
  test_clock_.Advance(base::Seconds(2));

  typing_session_manager_.EndAndRecordSession();

  histogram_tester.ExpectTotalCount("InputMethod.SessionDuration", 1);
  histogram_tester.ExpectBucketCount("InputMethod.SessionDuration", 1500, 1);
  histogram_tester.ExpectTotalCount("InputMethod.CharactersPerSession", 1);
  histogram_tester.ExpectBucketCount("InputMethod.CharactersPerSession", 55, 1);
}

}  // namespace
}  // namespace ash
