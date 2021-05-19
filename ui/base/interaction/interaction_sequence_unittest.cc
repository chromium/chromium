// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interaction_sequence.h"

#include "base/callback_forward.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"

namespace ui {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier1);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier2);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier3);
const ElementContext kTestContext1(1);
const ElementContext kTestContext2(2);

#define DECLARE_STEP_CALLBACK(Name)                           \
  base::MockCallback<InteractionSequence::StepCallback> Name; \
  EXPECT_CALL(Name, Run).Times(0)

#define DECLARE_COMPLETED_CALLBACK(Name)                           \
  base::MockCallback<InteractionSequence::CompletedCallback> Name; \
  EXPECT_CALL(Name, Run).Times(0)

#define DECLARE_ABORTED_CALLBACK(Name)                           \
  base::MockCallback<InteractionSequence::AbortedCallback> Name; \
  EXPECT_CALL(Name, Run).Times(0)

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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
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
  EXPECT_CALL_IN_SCOPE(aborted, Run, tracker->Start());
}

TEST(InteractionSequenceTest,
     AbortIfWithInitialElementHiddenBeforeStartIdentifierOnly) {
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step);
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
  EXPECT_CALL(aborted, Run(nullptr, element2.identifier(),
                           InteractionSequence::StepType::kShown))
      .Times(1);
  tracker->Start();
}

TEST(InteractionSequenceTest, TransitionOnSameElementHidden) {
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step);
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
  EXPECT_CALL(aborted, Run(nullptr, element2.identifier(),
                           InteractionSequence::StepType::kHidden))
      .Times(1);
  tracker->Start();
}

TEST(InteractionSequenceTest, NoWithInitialElementTransitionsOnActivation) {
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step_start);
  DECLARE_STEP_CALLBACK(step_end);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step_start);
  DECLARE_STEP_CALLBACK(step_end);
  DECLARE_STEP_CALLBACK(step2);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step1_start);
  DECLARE_STEP_CALLBACK(step1_end);
  DECLARE_STEP_CALLBACK(step2_start);
  DECLARE_STEP_CALLBACK(step2_end);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step1_start);
  DECLARE_STEP_CALLBACK(step1_end);
  DECLARE_STEP_CALLBACK(step2_start);
  DECLARE_STEP_CALLBACK(step2_end);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step1_start);
  DECLARE_STEP_CALLBACK(step1_end);
  DECLARE_STEP_CALLBACK(step2_start);
  DECLARE_STEP_CALLBACK(step2_end);
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

  EXPECT_CALLS_IN_SCOPE_2(step2_end, Run, aborted,
                          Run(testing::_, element2.identifier(),
                              InteractionSequence::StepType::kActivated),
                          element2.Hide());
}

TEST(InteractionSequenceTest, DontCancelIfViewDoesNotNeedToRemainVisible) {
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step1_start);
  DECLARE_STEP_CALLBACK(step1_end);
  DECLARE_STEP_CALLBACK(step2_start);
  DECLARE_STEP_CALLBACK(step2_end);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step);
  DECLARE_ABORTED_CALLBACK(aborted2);
  DECLARE_COMPLETED_CALLBACK(completed2);
  DECLARE_STEP_CALLBACK(step2);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step);
  DECLARE_ABORTED_CALLBACK(aborted2);
  DECLARE_COMPLETED_CALLBACK(completed2);
  DECLARE_STEP_CALLBACK(step2);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step1_end);
  DECLARE_STEP_CALLBACK(step2_start);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step1_end);
  DECLARE_STEP_CALLBACK(step2_start);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step1_end);
  DECLARE_STEP_CALLBACK(step2_start);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step1_end);
  DECLARE_STEP_CALLBACK(step2_start);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step2_start);
  DECLARE_STEP_CALLBACK(step2_end);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step_end);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
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
  EXPECT_CALL_IN_SCOPE(aborted,
                       Run(nullptr, element2.identifier(),
                           InteractionSequence::StepType::kShown),
                       tracker.reset());
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringInitialStepStartCallback) {
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step1_end);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step2_start);
  DECLARE_STEP_CALLBACK(step2_end);
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
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step1_start);
  DECLARE_STEP_CALLBACK(step1_end);
  DECLARE_STEP_CALLBACK(step2_start);
  DECLARE_STEP_CALLBACK(step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> tracker;
  auto callback = [&](TrackedElement*, ElementIdentifier,
                      InteractionSequence::StepType) { tracker.reset(); };
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step1_end);
  DECLARE_STEP_CALLBACK(step2_start);
  DECLARE_STEP_CALLBACK(step2_end);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step1_start);
  DECLARE_STEP_CALLBACK(step2_start);
  DECLARE_STEP_CALLBACK(step2_end);
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
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step1_start);
  DECLARE_STEP_CALLBACK(step1_end);
  DECLARE_STEP_CALLBACK(step2_start);
  DECLARE_STEP_CALLBACK(step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> tracker;
  auto callback = [&](TrackedElement*, ElementIdentifier,
                      InteractionSequence::StepType) { tracker.reset(); };
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_COMPLETED_CALLBACK(completed);
  DECLARE_STEP_CALLBACK(step1_start);
  DECLARE_STEP_CALLBACK(step1_end);
  DECLARE_STEP_CALLBACK(step2_start);
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
  DECLARE_ABORTED_CALLBACK(aborted);
  DECLARE_STEP_CALLBACK(step1_start);
  DECLARE_STEP_CALLBACK(step1_end);
  DECLARE_STEP_CALLBACK(step2_start);
  DECLARE_STEP_CALLBACK(step2_end);
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

}  // namespace ui
