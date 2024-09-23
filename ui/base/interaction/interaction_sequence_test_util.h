// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_INTERACTION_SEQUENCE_TEST_UTIL_H_
#define UI_BASE_INTERACTION_INTERACTION_SEQUENCE_TEST_UTIL_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace ui::test {

template <typename M1,
          typename M2,
          typename M3,
          typename M4,
          typename M5,
          typename M6 = decltype(testing::_),
          typename M7 = decltype(testing::_)>
auto SequenceAbortedMatcher(M1 step_idx_matcher,
                            M2 element_matcher,
                            M3 id_matcher,
                            M4 step_type_matcher,
                            M5 aborted_reason_matcher,
                            M6 step_description_matcher = testing::_,
                            M7 subsequence_failures_matcher = testing::_) {
  return testing::AllOf(
      testing::Field(&InteractionSequence::AbortedData::step_index,
                     (step_idx_matcher)),
      testing::Field(&InteractionSequence::AbortedData::element,
                     (element_matcher)),
      testing::Field(&InteractionSequence::AbortedData::element_id,
                     (id_matcher)),
      testing::Field(&InteractionSequence::AbortedData::step_type,
                     (step_type_matcher)),
      testing::Field(&InteractionSequence::AbortedData::aborted_reason,
                     (aborted_reason_matcher)),
      testing::Field(&InteractionSequence::AbortedData::step_description,
                     (step_description_matcher)),
      testing::Field(&InteractionSequence::AbortedData::subsequence_failures,
                     (subsequence_failures_matcher)));

// When waiting for mock callbacks in a run loop, post the trigger this far in
// advance to allow any existing processing to finish. Needs to be long enough
// that it won't be overtaken on slower machines.
//
// Note that this is Probably Bad and is only being used because we're trying to
// coordinate between external signals and step callbacks without triggering the
// external signals from the step callbacks.
//
// In the future we might want to rewrite these tests to do it other ways.
#define _EXPECT_ASYNC_POST_HELPER(Block)                              \
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask( \
      FROM_HERE, base::BindLambdaForTesting([&]() { Block; }),        \
      base::Milliseconds(100))

#define _EXPECT_ASYNC_CALL_HELPER(Name, Call) \
  EXPECT_CALL(Name, Call).WillOnce([&]() {    \
    if (++__call_count >= __kExpectedCount) { \
      __run_loop.Quit();                      \
    }                                         \
  })

#define EXPECT_ASYNC_CALL_IN_SCOPE(Name, Call, Block)                     \
  {                                                                       \
    static constexpr int __kExpectedCount = 1;                            \
    int __call_count = 0;                                                 \
    base::RunLoop __run_loop(base::RunLoop::Type::kNestableTasksAllowed); \
    _EXPECT_ASYNC_CALL_HELPER(Name, Call);                                \
    _EXPECT_ASYNC_POST_HELPER(Block);                                     \
    __run_loop.Run();                                                     \
    EXPECT_CALL(Name, Run).Times(0);                                      \
  }

#define EXPECT_ASYNC_CALLS_IN_SCOPE_2(Name1, Call1, Name2, Call2, Block)  \
  {                                                                       \
    static constexpr int __kExpectedCount = 2;                            \
    int __call_count = 0;                                                 \
    base::RunLoop __run_loop(base::RunLoop::Type::kNestableTasksAllowed); \
    _EXPECT_ASYNC_CALL_HELPER(Name1, Call1);                              \
    _EXPECT_ASYNC_CALL_HELPER(Name2, Call2);                              \
    _EXPECT_ASYNC_POST_HELPER(Block);                                     \
    __run_loop.Run();                                                     \
    EXPECT_CALL(Name2, Run).Times(0);                                     \
    EXPECT_CALL(Name1, Run).Times(0);                                     \
  }

#define EXPECT_ASYNC_CALLS_IN_SCOPE_3(Name1, Call1, Name2, Call2, Name3,  \
                                      Call3, Block)                       \
  {                                                                       \
    static constexpr int __kExpectedCount = 3;                            \
    int __call_count = 0;                                                 \
    base::RunLoop __run_loop(base::RunLoop::Type::kNestableTasksAllowed); \
    _EXPECT_ASYNC_CALL_HELPER(Name1, Call1);                              \
    _EXPECT_ASYNC_CALL_HELPER(Name2, Call2);                              \
    _EXPECT_ASYNC_CALL_HELPER(Name3, Call3);                              \
    _EXPECT_ASYNC_POST_HELPER(Block);                                     \
    __run_loop.Run();                                                     \
    EXPECT_CALL(Name3, Run).Times(0);                                     \
    EXPECT_CALL(Name2, Run).Times(0);                                     \
    EXPECT_CALL(Name1, Run).Times(0);                                     \
  }
}

}  // namespace ui::test

#endif  // UI_BASE_INTERACTION_INTERACTION_SEQUENCE_TEST_UTIL_H_
