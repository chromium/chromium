// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interaction_sequence.h"

#include "base/callback_forward.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"

namespace ui {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier1);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier2);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier3);
const ElementContext kTestContext1(1);
const ElementContext kTestContext2(2);

}  // namespace

TEST(InteractionSequenceTest, ConstructAndDestructContext) {
  auto tracker =
      InteractionSequence::Builder()
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  tracker.reset();
}

TEST(InteractionSequenceTest, ConstructAndDestructWithWithInitialElement) {
  TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto tracker = InteractionSequence::Builder()
                     .AddStep(InteractionSequence::WithInitialElement(&element))
                     .Build();
  tracker.reset();
}

TEST(InteractionSequenceTest, StartAndDestruct) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  tracker->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, tracker.reset());
}

TEST(InteractionSequenceTest, StartFailsIfWithInitialElementNotVisible) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element(kTestIdentifier1, kTestContext1);
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(aborted, Run, tracker->Start());
}

TEST(InteractionSequenceTest,
     StartFailsIfWithInitialElementNotVisibleIdentifierOnly) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element(kTestIdentifier1, kTestContext1);
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustBeVisibleAtStart(true)
                       .SetMustRemainVisible(true)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(aborted, Run, tracker->Start());
}

TEST(InteractionSequenceTest, AbortIfWithInitialElementHiddenBeforeStart) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElementPtr element =
      std::make_unique<TestElement>(kTestIdentifier1, kTestContext1);
  element->Show();
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(element.get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element->identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  element.reset();
  EXPECT_CALL_IN_SCOPE(
      aborted,
      Run(nullptr, ElementIdentifier(), InteractionSequence::StepType::kShown,
          InteractionSequence::AbortedReason::
              kElementHiddenBeforeSequenceStart),
      tracker->Start());
}

TEST(InteractionSequenceTest,
     AbortIfWithInitialElementHiddenBeforeStartIdentifierOnly) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElementPtr element =
      std::make_unique<TestElement>(kTestIdentifier1, kTestContext1);
  element->Show();
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element->context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element->identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustBeVisibleAtStart(true)
                       .SetMustRemainVisible(true)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element->identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  element.reset();
  EXPECT_CALL_IN_SCOPE(aborted, Run, tracker->Start());
}

TEST(InteractionSequenceTest, HideWithInitialElementAborts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  tracker->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, element.Hide());
}

TEST(InteractionSequenceTest,
     HideWithInitialElementDoesNotAbortIfMustRemainVisibleIsFalse) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustBeVisibleAtStart(true)
                       .SetMustRemainVisible(false)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  tracker->Start();
  element.Hide();
  EXPECT_CALL_IN_SCOPE(aborted, Run, tracker.reset());
}

TEST(InteractionSequenceTest, TransitionOnActivated) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step);
  TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  tracker->Start();
  EXPECT_CALL(step, Run(&element, element.identifier(),
                        InteractionSequence::StepType::kActivated))
      .Times(1);
  EXPECT_CALL(completed, Run).Times(1);
  element.Activate();
}

TEST(InteractionSequenceTest, TransitionOnElementShown) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  tracker->Start();
  EXPECT_CALL(step, Run(&element2, element2.identifier(),
                        InteractionSequence::StepType::kShown))
      .Times(1);
  EXPECT_CALL(completed, Run).Times(1);
  element2.Show();
}

TEST(InteractionSequenceTest, TransitionFailsOnElementShownIfMustBeVisible) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step.Get())
                       .SetMustBeVisibleAtStart(true)
                       .Build())
          .Build();
  EXPECT_CALL(
      aborted,
      Run(nullptr, element2.identifier(), InteractionSequence::StepType::kShown,
          InteractionSequence::AbortedReason::kElementNotVisibleAtStartOfStep))
      .Times(1);
  tracker->Start();
}

TEST(InteractionSequenceTest, TransitionOnSameElementHidden) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustRemainVisible(false)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  tracker->Start();
  element2.Show();
  EXPECT_CALL(step, Run(nullptr, element2.identifier(),
                        InteractionSequence::StepType::kHidden))
      .Times(1);
  EXPECT_CALL(completed, Run).Times(1);
  element2.Hide();
}

TEST(InteractionSequenceTest, TransitionOnOtherElementHidden) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  tracker->Start();
  EXPECT_CALL(step, Run(nullptr, element2.identifier(),
                        InteractionSequence::StepType::kHidden))
      .Times(1);
  EXPECT_CALL(completed, Run).Times(1);
  element2.Hide();
}

TEST(InteractionSequenceTest, TransitionOnOtherElementAlreadyHidden) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  EXPECT_CALL(step, Run(testing::_, element2.identifier(),
                        InteractionSequence::StepType::kHidden))
      .Times(1);
  EXPECT_CALL(completed, Run).Times(1);
  tracker->Start();
}

TEST(InteractionSequenceTest, FailOnOtherElementAlreadyHiddenIfMustBeVisible) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetMustBeVisibleAtStart(true)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  EXPECT_CALL(
      aborted,
      Run(nullptr, element2.identifier(),
          InteractionSequence::StepType::kHidden,
          InteractionSequence::AbortedReason::kElementNotVisibleAtStartOfStep))
      .Times(1);
  tracker->Start();
}

TEST(InteractionSequenceTest, NoWithInitialElementTransitionsOnActivation) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step);
  TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetMustBeVisibleAtStart(false)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  tracker->Start();
  EXPECT_CALL(step, Run(&element, element.identifier(),
                        InteractionSequence::StepType::kActivated))
      .Times(1);
  EXPECT_CALL(completed, Run).Times(1);
  element.Activate();
}

TEST(InteractionSequenceTest, NoWithInitialElementTransitionsOnShown) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step);
  TestElement element(kTestIdentifier1, kTestContext1);
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  tracker->Start();
  EXPECT_CALL(step, Run(&element, element.identifier(),
                        InteractionSequence::StepType::kShown))
      .Times(1);
  EXPECT_CALL(completed, Run).Times(1);
  element.Show();
}

TEST(InteractionSequenceTest, StepEndCallbackCalled) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step_end);
  TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step_start.Get())
                       .SetEndCallback(step_end.Get())
                       .Build())
          .Build();
  tracker->Start();
  EXPECT_CALLS_IN_SCOPE_3(step_start,
                          Run(&element, element.identifier(),
                              InteractionSequence::StepType::kActivated),
                          step_end,
                          Run(&element, element.identifier(),
                              InteractionSequence::StepType::kActivated),
                          completed, Run, element.Activate());
}

TEST(InteractionSequenceTest, StepEndCallbackCalledForInitialStep) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2);
  TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(
              &element, step_start.Get(), step_end.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2.Get())
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(step_start, Run, tracker->Start());
  EXPECT_CALLS_IN_SCOPE_3(step_end,
                          Run(&element, element.identifier(),
                              InteractionSequence::StepType::kShown),
                          step2,
                          Run(&element, element.identifier(),
                              InteractionSequence::StepType::kActivated),
                          completed, Run, element.Activate());
}

TEST(InteractionSequenceTest, MultipleStepsComplete) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step1_start.Get())
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2_start.Get())
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();

  tracker->Start();

  EXPECT_CALL_IN_SCOPE(step1_start, Run, element2.Show());

  EXPECT_CALLS_IN_SCOPE_2(step1_end, Run, step2_start, Run,
                          element2.Activate());

  EXPECT_CALLS_IN_SCOPE_2(step2_end, Run, completed, Run, element3.Show());
}

TEST(InteractionSequenceTest, MultipleStepsWithImmediateTransition) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();
  element3.Show();
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step1_start.Get())
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2_start.Get())
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();

  tracker->Start();

  EXPECT_CALL_IN_SCOPE(step1_start, Run, element2.Show());

  // Since element3 is already visible, we skip straight to the end.
  {
    testing::InSequence in_sequence;
    EXPECT_CALL(step1_end, Run).Times(1);
    EXPECT_CALL(step2_start, Run).Times(1);
    EXPECT_CALL(step2_end, Run).Times(1);
    EXPECT_CALL(completed, Run).Times(1);
  }
  element2.Activate();
}

TEST(InteractionSequenceTest, CancelMidSequenceWhenViewHidden) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step1_start.Get())
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       // Specify that this element must remain visible:
                       .SetMustRemainVisible(true)
                       .SetStartCallback(step2_start.Get())
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();

  tracker->Start();

  EXPECT_CALL_IN_SCOPE(step1_start, Run, element2.Show());

  EXPECT_CALLS_IN_SCOPE_2(step1_end, Run, step2_start, Run,
                          element2.Activate());

  EXPECT_CALLS_IN_SCOPE_2(
      step2_end, Run, aborted,
      Run(testing::_, element2.identifier(),
          InteractionSequence::StepType::kActivated,
          InteractionSequence::AbortedReason::kElementHiddenDuringStep),
      element2.Hide());
}

TEST(InteractionSequenceTest, DontCancelIfViewDoesNotNeedToRemainVisible) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step1_start.Get())
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       // Specify that this element need not remain visible:
                       .SetMustRemainVisible(false)
                       .SetStartCallback(step2_start.Get())
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();

  tracker->Start();

  EXPECT_CALL_IN_SCOPE(step1_start, Run, element2.Show());

  EXPECT_CALLS_IN_SCOPE_2(step1_end, Run, step2_start, Run,
                          element2.Activate());

  element2.Hide();

  EXPECT_CALLS_IN_SCOPE_2(step2_end, Run, completed, Run, element3.Show());
}

TEST(InteractionSequenceTest,
     MultipleSequencesInDifferentContextsOneCompletes) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier1, kTestContext2);
  element1.Show();
  element2.Show();

  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();

  auto tracker2 =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted2.Get())
          .SetCompletedCallback(completed2.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element2))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2.Get())
                       .Build())
          .Build();

  tracker->Start();
  tracker2->Start();

  EXPECT_CALLS_IN_SCOPE_2(step,
                          Run(&element1, element1.identifier(),
                              InteractionSequence::StepType::kActivated),
                          completed, Run, element1.Activate());

  EXPECT_CALL_IN_SCOPE(aborted2, Run, element2.Hide());
}

TEST(InteractionSequenceTest,
     MultipleSequencesInDifferentContextsBothComplete) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier1, kTestContext2);
  element1.Show();
  element2.Show();

  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();

  auto tracker2 =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted2.Get())
          .SetCompletedCallback(completed2.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element2))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2.Get())
                       .Build())
          .Build();

  tracker->Start();
  tracker2->Start();

  EXPECT_CALLS_IN_SCOPE_2(step,
                          Run(&element1, element1.identifier(),
                              InteractionSequence::StepType::kActivated),
                          completed, Run, element1.Activate());

  EXPECT_CALLS_IN_SCOPE_2(step2,
                          Run(&element2, element2.identifier(),
                              InteractionSequence::StepType::kActivated),
                          completed2, Run, element2.Activate());
}

// These tests verify that events sent during callbacks (as might be used by an
// interactive UI test powered by an InteractionSequence) do not break the
// sequence.

TEST(InteractionSequenceTest, ShowDuringCallback) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_start);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](TrackedElement*, ElementIdentifier,
                      InteractionSequence::StepType) { element2.Show(); };
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(
                           base::BindLambdaForTesting(std::move(callback)))
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step2_start.Get())
                       .Build())
          .Build();

  tracker->Start();
  EXPECT_CALLS_IN_SCOPE_3(step1_end, Run, step2_start, Run, completed, Run,
                          element1.Activate());
}

TEST(InteractionSequenceTest, HideDuringCallback) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_start);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();

  auto callback = [&](TrackedElement*, ElementIdentifier,
                      InteractionSequence::StepType) { element2.Hide(); };
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(
                           base::BindLambdaForTesting(std::move(callback)))
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetStartCallback(step2_start.Get())
                       .Build())
          .Build();

  tracker->Start();
  EXPECT_CALLS_IN_SCOPE_3(step1_end, Run, step2_start, Run, completed, Run,
                          element1.Activate());
}

TEST(InteractionSequenceTest, ActivateDuringCallbackDifferentView) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_start);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();

  auto callback = [&](TrackedElement*, ElementIdentifier,
                      InteractionSequence::StepType) { element2.Activate(); };
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(
                           base::BindLambdaForTesting(std::move(callback)))
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2_start.Get())
                       .Build())
          .Build();

  tracker->Start();
  EXPECT_CALLS_IN_SCOPE_3(step1_end, Run, step2_start, Run, completed, Run,
                          element1.Activate());
}

TEST(InteractionSequenceTest, ActivateDuringCallbackSameView) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_start);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](TrackedElement*, ElementIdentifier,
                      InteractionSequence::StepType) { element2.Activate(); };
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(
                           base::BindLambdaForTesting(std::move(callback)))
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2_start.Get())
                       .Build())
          .Build();

  tracker->Start();
  EXPECT_CALLS_IN_SCOPE_3(step1_end, Run, step2_start, Run, completed, Run,
                          element2.Show());
}

TEST(InteractionSequenceTest, HideAfterActivateDoesntAbort) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();
  element3.Show();

  auto callback = [&](TrackedElement*, ElementIdentifier,
                      InteractionSequence::StepType) {
    element2.Activate();
    element2.Hide();
  };
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(
                           base::BindLambdaForTesting(std::move(callback)))
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();

  tracker->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Show());
}

TEST(InteractionSequenceTest, HideDuringStepStartedCallbackAborts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](TrackedElement*, ElementIdentifier,
                      InteractionSequence::StepType) { element2.Hide(); };
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(
                           base::BindLambdaForTesting(std::move(callback)))
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();

  tracker->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, element2.Show());
}

TEST(InteractionSequenceTest, HideDuringStepEndedCallbackAborts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](TrackedElement*, ElementIdentifier,
                      InteractionSequence::StepType) { element2.Hide(); };
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(
              &element1, InteractionSequence::StepCallback(),
              base::BindLambdaForTesting(std::move(callback))))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustBeVisibleAtStart(true)
                       .SetStartCallback(step2_start.Get())
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(aborted, Run, tracker->Start());
}

TEST(InteractionSequenceTest, ElementHiddenDuringFinalStepStart) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](TrackedElement*, ElementIdentifier,
                      InteractionSequence::StepType) { element2.Hide(); };
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(callback))
                       .SetEndCallback(step_end.Get())
                       .Build())
          .Build();

  tracker->Start();
  EXPECT_CALLS_IN_SCOPE_2(step_end,
                          Run(nullptr, element2.identifier(),
                              InteractionSequence::StepType::kShown),
                          completed, Run, element2.Show());
}

TEST(InteractionSequenceTest, ElementHiddenDuringFinalStepEnd) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](TrackedElement*, ElementIdentifier,
                      InteractionSequence::StepType) { element2.Hide(); };
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustRemainVisible(false)
                       .SetEndCallback(base::BindLambdaForTesting(callback))
                       .Build())
          .Build();

  tracker->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Show());
}

TEST(InteractionSequenceTest, ElementHiddenDuringStepEndDuringAbort) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](TrackedElement*, ElementIdentifier,
                      InteractionSequence::StepType) { element2.Hide(); };
  auto tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetEndCallback(base::BindLambdaForTesting(callback))
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();

  tracker->Start();
  element2.Show();

  // First parameter will be null because during the delete the step end
  // callback will hide the element, which happens before the abort callback is
  // called.
  EXPECT_CALL_IN_SCOPE(
      aborted,
      Run(nullptr, element2.identifier(), InteractionSequence::StepType::kShown,
          InteractionSequence::AbortedReason::kSequenceDestroyed),
      tracker.reset());
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringInitialStepStartCallback) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> tracker;
  auto callback = [&](TrackedElement*, ElementIdentifier,
                      InteractionSequence::StepType) { tracker.reset(); };
  tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(
              &element1, base::BindLambdaForTesting(callback), step1_end.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();

  EXPECT_CALLS_IN_SCOPE_2(step1_end, Run, aborted, Run, tracker->Start());
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringInitialStepEndCallback) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> tracker;
  auto callback = [&](TrackedElement*, ElementIdentifier,
                      InteractionSequence::StepType) { tracker.reset(); };
  tracker = InteractionSequence::Builder()
                .SetAbortedCallback(aborted.Get())
                .SetCompletedCallback(completed.Get())
                .AddStep(InteractionSequence::WithInitialElement(
                    &element1, InteractionSequence::StepCallback(),
                    base::BindLambdaForTesting(callback)))
                .AddStep(InteractionSequence::StepBuilder()
                             .SetElementID(element1.identifier())
                             .SetType(InteractionSequence::StepType::kActivated)
                             .SetStartCallback(step2_start.Get())
                             .SetEndCallback(step2_end.Get())
                             .Build())
                .Build();

  tracker->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, element1.Activate());
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringInitialStepAbort) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> tracker;
  auto callback = [&](TrackedElement*, ElementIdentifier,
                      InteractionSequence::StepType,
                      InteractionSequence::AbortedReason) { tracker.reset(); };
  tracker = InteractionSequence::Builder()
                .SetAbortedCallback(base::BindLambdaForTesting(callback))
                .SetCompletedCallback(completed.Get())
                .AddStep(InteractionSequence::WithInitialElement(
                    &element1, step1_start.Get(), step1_end.Get()))
                .AddStep(InteractionSequence::StepBuilder()
                             .SetElementID(element1.identifier())
                             .SetType(InteractionSequence::StepType::kActivated)
                             .SetStartCallback(step2_start.Get())
                             .SetEndCallback(step2_end.Get())
                             .Build())
                .Build();

  EXPECT_CALL_IN_SCOPE(step1_start, Run, tracker->Start());
  EXPECT_CALL_IN_SCOPE(step1_end, Run, element1.Hide());
  EXPECT_FALSE(tracker);
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringMidSequenceStepStart) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> tracker;
  auto callback = [&](TrackedElement*, ElementIdentifier,
                      InteractionSequence::StepType) { tracker.reset(); };
  tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(base::BindLambdaForTesting(callback))
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2_start.Get())
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .Build();

  tracker->Start();
  EXPECT_CALLS_IN_SCOPE_2(step1_end, Run, aborted, Run, element2.Show());
  EXPECT_FALSE(tracker);
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringMidSequenceStepEnd) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> tracker;
  auto callback = [&](TrackedElement*, ElementIdentifier,
                      InteractionSequence::StepType) { tracker.reset(); };
  tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step1_start.Get())
                       .SetEndCallback(base::BindLambdaForTesting(callback))
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2_start.Get())
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .Build();

  tracker->Start();
  EXPECT_CALL_IN_SCOPE(step1_start, Run, element2.Show());
  EXPECT_CALL_IN_SCOPE(aborted, Run, element2.Activate());
  EXPECT_FALSE(tracker);
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringMidSequenceAbort) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> tracker;
  auto callback = [&](TrackedElement*, ElementIdentifier,
                      InteractionSequence::StepType,
                      InteractionSequence::AbortedReason) { tracker.reset(); };
  tracker = InteractionSequence::Builder()
                .SetAbortedCallback(base::BindLambdaForTesting(callback))
                .SetCompletedCallback(completed.Get())
                .AddStep(InteractionSequence::WithInitialElement(&element1))
                .AddStep(InteractionSequence::StepBuilder()
                             .SetElementID(element2.identifier())
                             .SetType(InteractionSequence::StepType::kShown)
                             .SetStartCallback(step1_start.Get())
                             .SetEndCallback(step1_end.Get())
                             .Build())
                .AddStep(InteractionSequence::StepBuilder()
                             .SetElementID(element2.identifier())
                             .SetType(InteractionSequence::StepType::kActivated)
                             .SetStartCallback(step2_start.Get())
                             .SetEndCallback(step2_end.Get())
                             .Build())
                .Build();

  tracker->Start();
  EXPECT_CALL_IN_SCOPE(step1_start, Run, element2.Show());
  EXPECT_CALL_IN_SCOPE(step1_end, Run, element2.Hide());
  EXPECT_FALSE(tracker);
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringFinalStepEnd) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_start);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> tracker;
  auto callback = [&](TrackedElement*, ElementIdentifier,
                      InteractionSequence::StepType) { tracker.reset(); };
  tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step1_start.Get())
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2_start.Get())
                       .SetEndCallback(base::BindLambdaForTesting(callback))
                       .Build())
          .Build();

  tracker->Start();
  EXPECT_CALL_IN_SCOPE(step1_start, Run, element2.Show());
  EXPECT_CALLS_IN_SCOPE_3(step1_end, Run, step2_start, Run, completed, Run,
                          element2.Activate());
  EXPECT_FALSE(tracker);
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringCompleted) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> tracker;
  auto callback = [&]() { tracker.reset(); };
  tracker = InteractionSequence::Builder()
                .SetAbortedCallback(aborted.Get())
                .SetCompletedCallback(base::BindLambdaForTesting(callback))
                .AddStep(InteractionSequence::WithInitialElement(&element1))
                .AddStep(InteractionSequence::StepBuilder()
                             .SetElementID(element2.identifier())
                             .SetType(InteractionSequence::StepType::kShown)
                             .SetStartCallback(step1_start.Get())
                             .SetEndCallback(step1_end.Get())
                             .Build())
                .AddStep(InteractionSequence::StepBuilder()
                             .SetElementID(element2.identifier())
                             .SetType(InteractionSequence::StepType::kActivated)
                             .SetStartCallback(step2_start.Get())
                             .SetEndCallback(step2_end.Get())
                             .Build())
                .Build();

  tracker->Start();
  EXPECT_CALL_IN_SCOPE(step1_start, Run, element2.Show());
  EXPECT_CALLS_IN_SCOPE_3(step1_end, Run, step2_start, Run, step2_end, Run,
                          element2.Activate());
  EXPECT_FALSE(tracker);
}

TEST(InteractionSequenceTest,
     RunSynchronouslyForTesting_SequenceAbortsDuringStart) {
  base::test::TaskEnvironment task_environment;
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner({});
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);

  std::unique_ptr<InteractionSequence> tracker;
  tracker = InteractionSequence::Builder()
                .SetAbortedCallback(aborted.Get())
                .SetCompletedCallback(completed.Get())
                .SetContext(kTestContext1)
                .AddStep(InteractionSequence::StepBuilder()
                             .SetElementID(element1.identifier())
                             .SetType(InteractionSequence::StepType::kShown)
                             .SetMustBeVisibleAtStart(true)
                             .Build())
                .Build();
  EXPECT_CALL_IN_SCOPE(aborted, Run, tracker->RunSynchronouslyForTesting());
}

TEST(InteractionSequenceTest,
     RunSynchronouslyForTesting_SequenceCompletesDuringStart) {
  base::test::TaskEnvironment task_environment;
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner({});
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> tracker;
  tracker = InteractionSequence::Builder()
                .SetAbortedCallback(aborted.Get())
                .SetCompletedCallback(completed.Get())
                .SetContext(kTestContext1)
                .AddStep(InteractionSequence::StepBuilder()
                             .SetElementID(element1.identifier())
                             .SetType(InteractionSequence::StepType::kShown)
                             .SetMustBeVisibleAtStart(true)
                             .Build())
                .Build();
  EXPECT_CALL_IN_SCOPE(completed, Run, tracker->RunSynchronouslyForTesting());
}

TEST(InteractionSequenceTest,
     RunSynchronouslyForTesting_SequenceAbortsDuringStep) {
  base::test::SingleThreadTaskEnvironment task_environment;
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> tracker;
  tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(
              InteractionSequence::StepBuilder()
                  .SetElementID(element1.identifier())
                  .SetType(InteractionSequence::StepType::kShown)
                  .SetMustRemainVisible(true)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](TrackedElement* element, ElementIdentifier element_id,
                          InteractionSequence::StepType step_type) {
                        task_environment.GetMainThreadTaskRunner()->PostTask(
                            FROM_HERE, base::BindLambdaForTesting(
                                           [&]() { element1.Hide(); }));
                      }))
                  .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(aborted, Run, tracker->RunSynchronouslyForTesting());
}

TEST(InteractionSequenceTest,
     RunSynchronouslyForTesting_SequenceCompletesDuringStep) {
  base::test::SingleThreadTaskEnvironment task_environment;

  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> tracker;
  tracker =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(
              InteractionSequence::StepBuilder()
                  .SetElementID(element1.identifier())
                  .SetType(InteractionSequence::StepType::kShown)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](TrackedElement* element, ElementIdentifier element_id,
                          InteractionSequence::StepType step_type) {
                        task_environment.GetMainThreadTaskRunner()->PostTask(
                            FROM_HERE, base::BindLambdaForTesting(
                                           [&]() { element2.Show(); }));
                      }))
                  .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(completed, Run, tracker->RunSynchronouslyForTesting());
}

}  // namespace ui
