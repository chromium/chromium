// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_ANIMATION_OBSERVER_HELPER_H_
#define UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_ANIMATION_OBSERVER_HELPER_H_

#include <algorithm>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/animation/ink_drop_animation_ended_reason.h"

namespace views {
namespace test {

// Context tracking helper that can be used with test implementations of
// ink drop animation observers.
template <typename ContextType>
class TestInkDropAnimationObserverHelper {
 public:
  TestInkDropAnimationObserverHelper()
      : last_animation_started_context_(),

        last_animation_ended_context_() {}

  virtual ~TestInkDropAnimationObserverHelper() = default;

  int last_animation_started_ordinal() const {
    return last_animation_started_ordinal_;
  }

  ContextType last_animation_started_context() const {
    return last_animation_started_context_;
  }

  int last_animation_ended_ordinal() const {
    return last_animation_ended_ordinal_;
  }

  ContextType last_animation_ended_context() const {
    return last_animation_ended_context_;
  }

  InkDropAnimationEndedReason last_animation_ended_reason() const {
    return last_animation_ended_reason_;
  }

  void OnAnimationStarted(ContextType context) {
    animation_started_contexts_.push_back(context);
    last_animation_started_context_ = context;
    last_animation_started_ordinal_ = GetNextOrdinal();
  }

  void OnAnimationEnded(ContextType context,
                        InkDropAnimationEndedReason reason) {
    animation_ended_contexts_.push_back(context);
    last_animation_ended_context_ = context;
    last_animation_ended_ordinal_ = GetNextOrdinal();
    last_animation_ended_reason_ = reason;
  }

  //
  // Collection of assertion predicates to be used with GTest test assertions.
  // i.e. EXPECT_TRUE/EXPECT_FALSE and the ASSERT_ counterparts.
  //
  // Example:
  //
  //   TestInkDropAnimationObserverHelper<int> observer;
  //   event_source.set_observer(observer);
  //   EXPECT_TRUE(observer.AnimationHasNotStarted());
  //

  // Passes *_TRUE assertions when an AnimationStarted() event has been
  // observed.
  testing::AssertionResult AnimationHasStarted() {
    if (last_animation_started_ordinal() > 0) {
      return testing::AssertionSuccess()
             << "Animations were started at ordinal="
             << last_animation_started_ordinal() << ".";
    }
    return testing::AssertionFailure() << "Animations have not started.";
  }

  // Passes *_TRUE assertions when an AnimationStarted() event has NOT been
  // observed.
  testing::AssertionResult AnimationHasNotStarted() {
    if (last_animation_started_ordinal() < 0)
      return testing::AssertionSuccess();
    return testing::AssertionFailure() << "Animations were started at ordinal="
                                       << last_animation_started_ordinal()
                                       << ".";
  }

  // Passes *_TRUE assertions when an AnimationEnded() event has been observed.
  testing::AssertionResult AnimationHasEnded() {
    if (last_animation_ended_ordinal() > 0) {
      return testing::AssertionSuccess() << "Animations were ended at ordinal="
                                         << last_animation_ended_ordinal()
                                         << ".";
    }
    return testing::AssertionFailure() << "Animations have not ended.";
  }

  // Passes *_TRUE assertions when an AnimationEnded() event has NOT been
  // observed.
  testing::AssertionResult AnimationHasNotEnded() {
    if (last_animation_ended_ordinal() < 0)
      return testing::AssertionSuccess();
    return testing::AssertionFailure() << "Animations were ended at ordinal="
                                       << last_animation_ended_ordinal() << ".";
  }

  // Passes *_TRUE assertions when |animation_started_context_| is the same as
  // |expected_contexts|.
  testing::AssertionResult AnimationStartedContextsMatch(
      const std::vector<ContextType>& expected_contexts) {
    return ContextsMatch(expected_contexts, animation_started_contexts_);
  }

  // Passes *_TRUE assertions when |animation_ended_context_| is the same as
  // |expected_contexts|.
  testing::AssertionResult AnimationEndedContextsMatch(
      const std::vector<ContextType>& expected_contexts) {
    return ContextsMatch(expected_contexts, animation_ended_contexts_);
  }

 private:
  // Helper function that checks if |actual_contexts| is the same as
  // |expected_contexts| returning appropriate AssertionResult.
  testing::AssertionResult ContextsMatch(
      const std::vector<ContextType>& expected_contexts,
      const std::vector<ContextType>& actual_contexts) {
    const bool match =
        expected_contexts.size() == actual_contexts.size() &&
        std::equal(expected_contexts.begin(), expected_contexts.end(),
                   actual_contexts.begin());
    testing::AssertionResult result =
        match ? (testing::AssertionSuccess() << "Expected == Actual: {")
              : (testing::AssertionFailure() << "Expected != Actual: {");
    for (auto eit = expected_contexts.begin(), ait = actual_contexts.begin();
         eit != expected_contexts.end() || ait != actual_contexts.end();) {
      if (eit != expected_contexts.begin())
        result << ", ";
      const bool eexists = eit != expected_contexts.end();
      const bool aexists = ait != actual_contexts.end();
      const bool item_match = eexists && aexists && *eit == *ait;
      result << (eexists ? ToString(*eit) : "<none>")
             << (item_match ? " == " : " != ")
             << (aexists ? ToString(*ait) : "<none>");
      if (eexists)
        eit++;
      if (aexists)
        ait++;
    }
    result << "}";
    return result;
  }

  // Returns the next event ordinal. The first returned ordinal will be 1.
  int GetNextOrdinal() const {
    return std::max(1, std::max(last_animation_started_ordinal_,
                                last_animation_ended_ordinal_) +
                           1);
  }

  // The ordinal time of the last AnimationStarted() call.
  int last_animation_started_ordinal_ = -1;

  // List of contexts for which animation is started.
  std::vector<ContextType> animation_started_contexts_;

  // The |context| passed to the last call to AnimationStarted().
  ContextType last_animation_started_context_;

  // The ordinal time of the last AnimationEnded() call.
  int last_animation_ended_ordinal_ = -1;

  // List of contexts for which animation is ended.
  std::vector<ContextType> animation_ended_contexts_;

  // The |context| passed to the last call to AnimationEnded().
  ContextType last_animation_ended_context_;

  InkDropAnimationEndedReason last_animation_ended_reason_ =
      InkDropAnimationEndedReason::SUCCESS;

  DISALLOW_COPY_AND_ASSIGN(TestInkDropAnimationObserverHelper);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_ANIMATION_OBSERVER_HELPER_H_
