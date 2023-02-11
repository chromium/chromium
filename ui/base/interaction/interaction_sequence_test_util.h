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
}

}  // namespace ui::test

#endif  // UI_BASE_INTERACTION_INTERACTION_SEQUENCE_TEST_UTIL_H_
