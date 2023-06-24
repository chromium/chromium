// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interaction_sequence.h"

#include <sstream>

#include "base/debug/stack_trace.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
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
#include "ui/base/interaction/interaction_sequence_test_util.h"

namespace ui {

namespace {

constexpr char kElementName1[] = "Element1";
constexpr char kElementName2[] = "Element2";
constexpr char kStepDescription[] = "Step description.";
constexpr char kStepDescription2[] = "Step description 2.";
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier1);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier2);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier3);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCustomEventType1);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCustomEventType2);
const ElementContext kTestContext1(1);
const ElementContext kTestContext2(2);
const ElementContext kTestContext3(3);

}  // namespace

TEST(InteractionSequenceTest, ConstructAndDestructContext) {
  auto sequence =
      InteractionSequence::Builder()
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence.reset();
}

TEST(InteractionSequenceTest, ConstructAndDestructWithWithInitialElement) {
  test::TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto sequence =
      InteractionSequence::Builder()
          .AddStep(InteractionSequence::WithInitialElement(&element))
          .Build();
  sequence.reset();
}

TEST(InteractionSequenceTest, StartAndDestruct) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence.reset());
}

TEST(InteractionSequenceTest, StartFailsIfWithInitialElementNotVisible) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     StartFailsIfWithInitialElementNotVisibleIdentifierOnly) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  auto sequence =
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
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->Start());
}

TEST(InteractionSequenceTest, AbortIfWithInitialElementHiddenBeforeStart) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElementPtr element =
      std::make_unique<test::TestElement>(kTestIdentifier1, kTestContext1);
  element->Show();
  auto sequence =
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
      Run(test::SequenceAbortedMatcher(1, nullptr, kTestIdentifier1,
                                       InteractionSequence::StepType::kShown,
                                       InteractionSequence::AbortedReason::
                                           kElementHiddenBeforeSequenceStart)),
      sequence->Start());
}

TEST(InteractionSequenceTest,
     AbortIfWithInitialElementHiddenBeforeStartIdentifierOnly) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElementPtr element =
      std::make_unique<test::TestElement>(kTestIdentifier1, kTestContext1);
  element->Show();
  auto sequence =
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
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->Start());
}

TEST(InteractionSequenceTest, HideWithInitialElementAborts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, element.Hide());
}

TEST(InteractionSequenceTest,
     HideWithInitialElementDoesNotAbortIfMustRemainVisibleIsFalse) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto sequence =
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
                       .SetMustBeVisibleAtStart(false)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->Start();
  element.Hide();
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence.reset());
}

// This tests a case where the element is hidden on the first step and there is
// an explicit step transition.
TEST(InteractionSequenceTest, TransitionOnElementHiddenFirstStep) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetTransitionOnlyOnEvent(true))
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element.Hide());
}

// Now that we're fairly confident that sequences can complete, try all the
// different ways to construct and add steps.
TEST(InteractionSequenceTest, TestStepBuilderConstructAndAdd) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCustomEvent);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, start1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, end1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, start2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, end2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, start3);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, end3);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, start4);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, end4);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  InteractionSequence::StepBuilder step1;
  step1.SetElementID(element1.identifier());
  step1.SetStartCallback(start1.Get());
  step1.SetEndCallback(end1.Get());
  InteractionSequence::StepBuilder step2;
  step2.SetElementID(element1.identifier());
  step2.SetType(InteractionSequence::StepType::kActivated);
  step2.SetStartCallback(start2.Get());
  step2.SetEndCallback(end2.Get());
  InteractionSequence::StepBuilder step3;
  step3.SetElementID(element2.identifier());
  step3.SetStartCallback(start3.Get());
  step3.SetEndCallback(end3.Get());
  InteractionSequence::StepBuilder step4;
  step4.SetElementID(element2.identifier());
  step4.SetType(InteractionSequence::StepType::kCustomEvent, kCustomEvent);

  // Test move and assign for step builder.
  InteractionSequence::StepBuilder step1_move_constructed(std::move(step1));
  InteractionSequence::StepBuilder step2_move_assigned;
  step2_move_assigned = std::move(step2);
  InteractionSequence::StepBuilder step4_move_constructed_then_modified(
      std::move(step4));
  step4_move_constructed_then_modified.SetStartCallback(start4.Get());
  step4_move_constructed_then_modified.SetEndCallback(end4.Get());

  InteractionSequence::Builder builder;
  builder.SetAbortedCallback(aborted.Get())
      .SetCompletedCallback(completed.Get())
      .SetContext(element1.context())
      .AddStep(step1_move_constructed)
      .AddStep(step2_move_assigned)
      .AddStep(std::move(step3))
      .AddStep(std::move(step4_move_constructed_then_modified))
      .AddStep(std::move(InteractionSequence::StepBuilder()
                             .SetElementID(element2.identifier())
                             .SetType(InteractionSequence::StepType::kHidden)));

  // Test move and assign for builder.
  InteractionSequence::Builder builder2(std::move(builder));
  InteractionSequence::Builder builder3;
  builder3 = std::move(builder2);
  auto sequence = builder3.Build();

  EXPECT_CALL_IN_SCOPE(start1, Run, sequence->Start());
  EXPECT_CALLS_IN_SCOPE_2(end1, Run, start2, Run, element1.Activate());
  EXPECT_CALLS_IN_SCOPE_2(end2, Run, start3, Run, element2.Show());
  EXPECT_CALLS_IN_SCOPE_2(end3, Run, start4, Run,
                          element2.SendCustomEvent(kCustomEvent));
  EXPECT_CALLS_IN_SCOPE_2(end4, Run, completed, Run, element2.Hide());
}

TEST(InteractionSequenceTest, TransitionOnActivated) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto sequence =
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
  sequence->Start();
  EXPECT_CALL(step, Run(sequence.get(), &element)).Times(1);
  EXPECT_CALL(completed, Run).Times(1);
  element.Activate();
}

TEST(InteractionSeuenceTest, TransitionOnCustomEventSameId) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_2(step, Run(sequence.get(), &element), completed, Run,
                          element.SendCustomEvent(kCustomEventType1));
}

TEST(InteractionSeuenceTest, TransitionOnCustomEventDifferentId) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element.Show();
  element2.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  sequence->Start();
  // Non-matching ID should not trigger the step, even if the event type
  // matches.
  element.SendCustomEvent(kCustomEventType1);
  EXPECT_CALLS_IN_SCOPE_2(step, Run(sequence.get(), &element2), completed, Run,
                          element2.SendCustomEvent(kCustomEventType1));
}

TEST(InteractionSeuenceTest, TransitionOnCustomEventAnyElement) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element.Show();
  element2.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .SetStartCallback(step.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .SetStartCallback(step2.Get())
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(step, Run(sequence.get(), &element),
                       element.SendCustomEvent(kCustomEventType1));
  EXPECT_CALLS_IN_SCOPE_2(step2, Run(sequence.get(), &element2), completed, Run,
                          element2.SendCustomEvent(kCustomEventType1));
}

TEST(InteractionSeuenceTest, TransitionOnCustomEventMultipleEvents) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element.Show();
  element2.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .SetStartCallback(step.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType2)
                       .SetStartCallback(step2.Get())
                       .Build())
          .Build();
  sequence->Start();

  // This is the wrong event type so won't cause a transition.
  element.SendCustomEvent(kCustomEventType2);

  EXPECT_CALL_IN_SCOPE(step, Run(sequence.get(), &element),
                       element.SendCustomEvent(kCustomEventType1));

  // This is the wrong event type so won't cause a transition.
  element2.SendCustomEvent(kCustomEventType1);

  EXPECT_CALLS_IN_SCOPE_2(step2, Run(sequence.get(), &element2), completed, Run,
                          element2.SendCustomEvent(kCustomEventType2));
}

TEST(InteractionSeuenceTest, TransitionOnCustomEventFailsIfMustBeVisible) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .SetMustBeVisibleAtStart(true)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(
      aborted,
      Run(test::SequenceAbortedMatcher(
          2, nullptr, element2.identifier(),
          InteractionSequence::StepType::kCustomEvent,
          InteractionSequence::AbortedReason::kElementNotVisibleAtStartOfStep)),
      sequence->Start());
}

TEST(InteractionSequenceTest, TransitionOnElementShown) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  auto sequence =
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
  sequence->Start();
  EXPECT_CALL(step, Run(sequence.get(), &element2)).Times(1);
  EXPECT_CALL(completed, Run).Times(1);
  element2.Show();
}

TEST(InteractionSequenceTest, TransitionFailsOnElementShownIfMustBeVisible) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetDescription(kStepDescription)
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step.Get())
                       .SetMustBeVisibleAtStart(true)
                       .Build())
          .Build();
  EXPECT_CALL(
      aborted,
      Run(test::SequenceAbortedMatcher(
          2, nullptr, element2.identifier(),
          InteractionSequence::StepType::kShown,
          InteractionSequence::AbortedReason::kElementNotVisibleAtStartOfStep,
          std::string(kStepDescription))))
      .Times(1);
  sequence->Start();
}

TEST(InteractionSequenceTest, TransitionOnSameElementHidden) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  auto sequence =
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
  sequence->Start();
  element2.Show();
  EXPECT_CALL(step, Run(sequence.get(), nullptr)).Times(1);
  EXPECT_CALL(completed, Run).Times(1);
  element2.Hide();
}

TEST(InteractionSequenceTest, TransitionOnOtherElementHidden) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto sequence =
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
  sequence->Start();
  EXPECT_CALL(step, Run(sequence.get(), nullptr)).Times(1);
  EXPECT_CALL(completed, Run).Times(1);
  element2.Hide();
}

TEST(InteractionSequenceTest, TransitionOnOtherElementAlreadyHidden) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  auto sequence =
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
  EXPECT_CALL(step, Run(sequence.get(), testing::_)).Times(1);
  EXPECT_CALL(completed, Run).Times(1);
  sequence->Start();
}

TEST(InteractionSequenceTest, FailOnOtherElementAlreadyHiddenIfMustBeVisible) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  auto sequence =
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
      Run(test::SequenceAbortedMatcher(
          2, nullptr, element2.identifier(),
          InteractionSequence::StepType::kHidden,
          InteractionSequence::AbortedReason::kElementNotVisibleAtStartOfStep)))
      .Times(1);
  sequence->Start();
}

TEST(InteractionSequenceTest, FailIfFirstElementBecomesHiddenBeforeActivation) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, element1.Hide());
}

TEST(InteractionSequenceTest,
     FailIfSecondElementBecomesHiddenBeforeActivation) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, element2.Hide());
}

TEST(InteractionSequenceTest,
     FailIfFirstElementBecomesHiddenBeforeCustomEvent) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, element1.Hide());
}

TEST(InteractionSequenceTest, NoInitialElementTransitionsOnActivation) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto sequence =
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
  sequence->Start();
  EXPECT_CALL(step, Run(sequence.get(), &element)).Times(1);
  EXPECT_CALL(completed, Run).Times(1);
  element.Activate();
}

TEST(InteractionSequenceTest, NoInitialElementTransitionsOnCustomEvent) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_2(step, Run(sequence.get(), &element), completed, Run,
                          element.SendCustomEvent(kCustomEventType1));
}

TEST(InteractionSequenceTest, NoInitialElementTransitionsOnShown) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  auto sequence =
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
  sequence->Start();
  EXPECT_CALL(step, Run(sequence.get(), &element)).Times(1);
  EXPECT_CALL(completed, Run).Times(1);
  element.Show();
}

TEST(InteractionSequenceTest, StepEndCallbackCalled) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step_end);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto sequence =
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
  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_3(step_start, Run(sequence.get(), &element), step_end,
                          Run(&element), completed, Run, element.Activate());
}

TEST(InteractionSequenceTest, StepEndCallbackCalledForInitialStep) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto sequence =
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
  EXPECT_CALL_IN_SCOPE(step_start, Run, sequence->Start());
  EXPECT_CALLS_IN_SCOPE_3(step_end, Run(&element), step2,
                          Run(sequence.get(), &element), completed, Run,
                          element.Activate());
}

TEST(InteractionSequenceTest, MultipleStepsComplete) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();
  auto sequence =
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

  sequence->Start();

  EXPECT_CALL_IN_SCOPE(step1_start, Run, element2.Show());

  EXPECT_CALLS_IN_SCOPE_2(step1_end, Run, step2_start, Run,
                          element2.Activate());

  EXPECT_CALLS_IN_SCOPE_2(step2_end, Run, completed, Run, element3.Show());
}

TEST(InteractionSequenceTest, MultipleStepsWithImmediateTransition) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();
  element3.Show();
  auto sequence =
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

  sequence->Start();

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
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();
  auto sequence =
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
                       .SetDescription(kStepDescription)
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

  sequence->Start();

  EXPECT_CALL_IN_SCOPE(step1_start, Run, element2.Show());

  EXPECT_CALLS_IN_SCOPE_2(step1_end, Run, step2_start, Run,
                          element2.Activate());

  EXPECT_CALLS_IN_SCOPE_2(
      step2_end, Run, aborted,
      Run(test::SequenceAbortedMatcher(
          3, testing::_, element2.identifier(),
          InteractionSequence::StepType::kActivated,
          InteractionSequence::AbortedReason::kElementHiddenDuringStep,
          std::string(kStepDescription))),
      element2.Hide());
}

TEST(InteractionSequenceTest, DontCancelIfViewDoesNotNeedToRemainVisible) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();
  auto sequence =
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

  sequence->Start();

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
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier1, kTestContext2);
  element1.Show();
  element2.Show();

  auto sequence =
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

  auto sequence2 =
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

  sequence->Start();
  sequence2->Start();

  EXPECT_CALLS_IN_SCOPE_2(step, Run(sequence.get(), &element1), completed, Run,
                          element1.Activate());

  EXPECT_CALL_IN_SCOPE(aborted2, Run, element2.Hide());
}

TEST(InteractionSequenceTest,
     MultipleSequencesInDifferentContextsBothComplete) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier1, kTestContext2);
  element1.Show();
  element2.Show();

  auto sequence =
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

  auto sequence2 =
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

  sequence->Start();
  sequence2->Start();

  EXPECT_CALLS_IN_SCOPE_2(step, Run(sequence.get(), &element1), completed, Run,
                          element1.Activate());

  EXPECT_CALLS_IN_SCOPE_2(step2, Run(sequence2.get(), &element2), completed2,
                          Run, element2.Activate());
}

// These tests verify that events sent during callbacks (as might be used by an
// interactive UI test powered by an InteractionSequence) do not break the
// sequence.

TEST(InteractionSequenceTest, ShowDuringCallback) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](InteractionSequence*, TrackedElement*) {
    element2.Show();
  };
  auto sequence =
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

  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_3(step1_end, Run, step2_start, Run, completed, Run,
                          element1.Activate());
}

TEST(InteractionSequenceTest, HideDuringCallback) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();

  auto callback = [&](InteractionSequence*, TrackedElement*) {
    element2.Hide();
  };
  auto sequence =
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

  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_3(step1_end, Run, step2_start, Run, completed, Run,
                          element1.Activate());
}

TEST(InteractionSequenceTest, ActivateDuringCallbackDifferentView) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();

  auto callback = [&](InteractionSequence*, TrackedElement*) {
    element2.Activate();
  };
  auto sequence =
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

  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_3(step1_end, Run, step2_start, Run, completed, Run,
                          element1.Activate());
}

TEST(InteractionSequenceTest, ActivateDuringCallbackSameView) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](InteractionSequence*, TrackedElement*) {
    element2.Activate();
  };
  auto sequence =
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

  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_3(step1_end, Run, step2_start, Run, completed, Run,
                          element2.Show());
}

TEST(InteractionSequenceTest, CustomEventDuringCallbackDifferentView) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();

  auto callback = [&](InteractionSequence*, TrackedElement*) {
    element2.SendCustomEvent(kCustomEventType1);
  };
  auto sequence =
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
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .SetStartCallback(step2_start.Get())
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_3(step1_end, Run, step2_start, Run, completed, Run,
                          element1.Activate());
}

TEST(InteractionSequenceTest, CustomEventDuringCallbackSameView) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](InteractionSequence*, TrackedElement*) {
    element2.SendCustomEvent(kCustomEventType1);
  };
  auto sequence =
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
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .SetStartCallback(step2_start.Get())
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_3(step1_end, Run, step2_start, Run, completed, Run,
                          element2.Show());
}

TEST(InteractionSequenceTest, ElementHiddenDuringElementShownCallback) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);

  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto hide_element = [&](InteractionSequence*, TrackedElement*) {
    element2.Hide();
  };

  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(
                           base::BindLambdaForTesting(std::move(hide_element)))
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Show());
}

TEST(InteractionSequenceTest, HideAfterActivateDoesntAbort) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();
  element3.Show();

  auto callback = [&](InteractionSequence*, TrackedElement*) {
    element2.Activate();
    element2.Hide();
  };
  auto sequence =
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

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Show());
}

TEST(InteractionSequenceTest, HideAfterCustomEventDoesntAbort) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();
  element3.Show();

  auto callback = [&](InteractionSequence*, TrackedElement*) {
    element2.SendCustomEvent(kCustomEventType1);
    element2.Hide();
  };
  auto sequence =
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
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Show());
}

TEST(InteractionSequenceTest, HideUnnamedElementAfterCustomEventDoesntAbort) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();
  element3.Show();

  auto callback = [&](InteractionSequence*, TrackedElement*) {
    element2.SendCustomEvent(kCustomEventType1);
    element2.Hide();
  };
  auto sequence =
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
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Show());
}

TEST(InteractionSequenceTest, HideDuringStepStartedCallbackAborts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](InteractionSequence*, TrackedElement*) {
    element2.Hide();
  };
  auto sequence =
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

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, element2.Show());
}

TEST(InteractionSequenceTest, HideDuringStepEndedCallbackAborts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](TrackedElement*) { element2.Hide(); };
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(
              &element1, InteractionSequence::StepStartCallback(),
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

  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     HideDuringStepStartedCallbackBeforeCustomEventAborts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](InteractionSequence*, TrackedElement*) {
    element2.Hide();
  };
  auto sequence =
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
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, element2.Show());
}

TEST(InteractionSequenceTest,
     HideDuringStepEndedCallbackBeforeCustomEventAborts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](TrackedElement*) { element2.Hide(); };
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(
              &element1, InteractionSequence::StepStartCallback(),
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
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->Start());
}

TEST(InteractionSequenceTest, ElementHiddenDuringFinalStepStart) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step_end);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](InteractionSequence*, TrackedElement*) {
    element2.Hide();
  };
  auto sequence =
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

  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_2(step_end, Run(nullptr), completed, Run,
                          element2.Show());
}

TEST(InteractionSequenceTest, ElementHiddenDuringFinalStepEnd) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](TrackedElement*) { element2.Hide(); };
  auto sequence =
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

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Show());
}

TEST(InteractionSequenceTest, ElementHiddenDuringStepEndDuringAbort) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](TrackedElement*) { element2.Hide(); };
  auto sequence =
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
                       .SetDescription(kStepDescription)
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();

  sequence->Start();
  element2.Show();

  // First parameter will be null because during the delete the step end
  // callback will hide the element, which happens before the abort callback is
  // called.
  EXPECT_CALL_IN_SCOPE(
      aborted,
      Run(test::SequenceAbortedMatcher(
          3, nullptr, element2.identifier(),
          InteractionSequence::StepType::kActivated,
          InteractionSequence::AbortedReason::kSequenceDestroyed,
          std::string(kStepDescription))),
      sequence.reset());
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringInitialStepStartCallback) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  auto callback = [&](InteractionSequence*, TrackedElement*) {
    sequence.reset();
  };
  sequence =
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

  EXPECT_CALLS_IN_SCOPE_2(step1_end, Run, aborted, Run, sequence->Start());
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringInitialStepEndCallback) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  auto callback = [&](TrackedElement*) { sequence.reset(); };
  sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(
              &element1, InteractionSequence::StepStartCallback(),
              base::BindLambdaForTesting(callback)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2_start.Get())
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, element1.Activate());
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringInitialStepAbort) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  auto callback = [&](const InteractionSequence::AbortedData&) {
    sequence.reset();
  };
  sequence =
      InteractionSequence::Builder()
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

  EXPECT_CALL_IN_SCOPE(step1_start, Run, sequence->Start());
  EXPECT_CALL_IN_SCOPE(step1_end, Run, element1.Hide());
  EXPECT_FALSE(sequence);
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringMidSequenceStepStart) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  auto callback = [&](InteractionSequence*, TrackedElement*) {
    sequence.reset();
  };
  sequence =
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

  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_2(step1_end, Run, aborted, Run, element2.Show());
  EXPECT_FALSE(sequence);
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringMidSequenceStepEnd) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  auto callback = [&](TrackedElement*) { sequence.reset(); };
  sequence =
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

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(step1_start, Run, element2.Show());
  EXPECT_CALL_IN_SCOPE(aborted, Run, element2.Activate());
  EXPECT_FALSE(sequence);
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringMidSequenceAbort) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  auto callback = [&](const InteractionSequence::AbortedData&) {
    sequence.reset();
  };
  sequence =
      InteractionSequence::Builder()
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

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(step1_start, Run, element2.Show());
  EXPECT_CALL_IN_SCOPE(step1_end, Run, element2.Hide());
  EXPECT_FALSE(sequence);
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringFinalStepEnd) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  auto callback = [&](TrackedElement*) { sequence.reset(); };
  sequence =
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

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(step1_start, Run, element2.Show());
  EXPECT_CALLS_IN_SCOPE_3(step1_end, Run, step2_start, Run, completed, Run,
                          element2.Activate());
  EXPECT_FALSE(sequence);
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringCompleted) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  auto callback = [&]() { sequence.reset(); };
  sequence =
      InteractionSequence::Builder()
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

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(step1_start, Run, element2.Show());
  EXPECT_CALLS_IN_SCOPE_3(step1_end, Run, step2_start, Run, step2_end, Run,
                          element2.Activate());
  EXPECT_FALSE(sequence);
}

TEST(InteractionSequenceTest, SimulateTestTimeout) {
  base::test::TaskEnvironment task_environment;
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner({});

  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  auto delete_sequence =
      base::BindLambdaForTesting([&]() { sequence.reset(); });

  sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetStartCallback(base::BindOnce(
                           [](base::OnceClosure cb) {
                             base::SingleThreadTaskRunner::GetCurrentDefault()
                                 ->PostTask(FROM_HERE, std::move(cb));
                           },
                           std::move(delete_sequence))))
          .AddStep(
              InteractionSequence::StepBuilder().SetElementID(kTestIdentifier2))
          .Build();

  // Should indicate step that was queued and waiting, not the step that
  // succeeded.
  EXPECT_CALL_IN_SCOPE(
      aborted,
      Run(test::SequenceAbortedMatcher(
          2, nullptr, kTestIdentifier2, InteractionSequence::StepType::kShown,
          InteractionSequence::AbortedReason::kSequenceDestroyed)),
      sequence->RunSynchronouslyForTesting());
}

// Transition during step callback tests for show and hide events.
// These are tricky to get right, so all of the variations must be tested.

TEST(InteractionSequenceTest, HideDuringStepTransitionSameElement) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&]() { element1.Hide(); })))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetTransitionOnlyOnEvent(true))
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     HideDuringStepTransitionSameElementVisibilityBlinks) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetStartCallback(base::BindLambdaForTesting([&]() {
                         element1.Hide();
                         element1.Show();
                       })))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetTransitionOnlyOnEvent(true))
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest, HideDuringStepTransitionDifferentElementSameID) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier1, kTestContext1);
  element1.Show();
  element2.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&]() { element2.Hide(); })))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetTransitionOnlyOnEvent(true))
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(
    InteractionSequenceTest,
    HideDuringStepTransitionDifferentElementSameIDSameElementVisibilityBlinks) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier1, kTestContext1);
  element1.Show();
  element2.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetStartCallback(base::BindLambdaForTesting([&]() {
                         element2.Hide();
                         element2.Show();
                       })))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetTransitionOnlyOnEvent(true))
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest, HideDuringStepTransitionDifferentID) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&]() { element2.Hide(); })))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier2)
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetTransitionOnlyOnEvent(true))
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     HideDuringStepTransitionDifferentIDVisibilityBlinks) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetStartCallback(base::BindLambdaForTesting([&]() {
                         element2.Hide();
                         element2.Show();
                       })))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier2)
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetTransitionOnlyOnEvent(true))
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     ShowDuringStepTransitionSameElementTransitionOnlyOnEvent) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetStartCallback(base::BindLambdaForTesting([&]() {
                         element1.Hide();
                         element1.Show();
                       })))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetTransitionOnlyOnEvent(true))
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     ShowDuringStepTransitionSameElementDoesNotNeedToRemainVisible) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting([&]() {
                         element1.Hide();
                         element1.Show();
                         element1.Hide();
                       })))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustRemainVisible(false))
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     ShowDuringStepTransitionSameIDTransitionOnlyOnEvent) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier1, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&]() { element2.Show(); })))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetTransitionOnlyOnEvent(true))
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     ShowDuringStepTransitionSameIDDoesNotNeedToRemainVisible) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier1, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting([&]() {
                         element2.Show();
                         element2.Hide();
                       })))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustRemainVisible(false))
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     ShowDuringStepTransitionDifferentIDTransitionOnlyOnEvent) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&]() { element2.Show(); })))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier2)
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetTransitionOnlyOnEvent(true))
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     ShowDuringStepTransitionDifferentIDDoesNotNeedToRemainVisible) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting([&]() {
                         element2.Show();
                         element2.Hide();
                       })))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier2)
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustRemainVisible(false))
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     ShowDuringStepTransitionDoesNotAbortAfterTrigger) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetStartCallback(base::BindLambdaForTesting([&]() {
                         element2.Show();
                         element1.Hide();
                       })))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier2)
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustRemainVisible(false))
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

// Bait-and-switch tests - verify that when an element must start visible and
// there are multiple such elements, it's okay if any of them receive the
// following event.

TEST(InteractionSequenceTest, BaitAndSwitchActivation) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetMustBeVisibleAtStart(true)
                       .Build())
          .Build();

  sequence->Start();
  element3.Show();
  element2.Hide();
  EXPECT_CALL_IN_SCOPE(completed, Run, element3.Activate());
}

TEST(InteractionSequenceTest, BaitAndSwitchActivationFails) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetMustBeVisibleAtStart(true)
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, {
    // By hiding before showing the other element, there are no visible
    // elements.
    element2.Hide();
    element3.Show();
    element3.Activate();
  });
}

TEST(InteractionSequenceTest, BaitAndSwitchActivationDuringStepTransition) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();

  auto step_start =
      base::BindLambdaForTesting([&](InteractionSequence*, TrackedElement*) {
        element3.Show();
        element2.Hide();
        element3.Activate();
      });

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(
              &element1, std::move(step_start)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetMustBeVisibleAtStart(true)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     BaitAndSwitchActivationDuringStepTransitionEventuallyConsistent) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();

  auto step_start =
      base::BindLambdaForTesting([&](InteractionSequence*, TrackedElement*) {
        // By hiding before showing the other element, there are no visible
        // elements. However, because the system becomes consistent before the
        // end of the callback and the event is triggered, we are allowed to
        // proceed. This is technically incorrect behavior but adds a bit of
        // forgiveness into the system if visibility of elements is updated in
        // arbitrary order.
        element2.Hide();
        element3.Show();
        element3.Activate();
      });

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(
              &element1, std::move(step_start)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetMustBeVisibleAtStart(true)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest, BaitAndSwitchCustomEvent) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .SetMustBeVisibleAtStart(true)
                       .Build())
          .Build();

  sequence->Start();
  element3.Show();
  element2.Hide();
  EXPECT_CALL_IN_SCOPE(completed, Run,
                       element3.SendCustomEvent(kCustomEventType1));
}

TEST(InteractionSequenceTest, BaitAndSwitchCustomEventFails) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .SetMustBeVisibleAtStart(true)
                       .Build())
          .Build();

  sequence->Start();
  // By hiding before showing the other element, there are no visible elements.
  EXPECT_CALL_IN_SCOPE(aborted, Run, {
    element2.Hide();
    element3.Show();
    element3.SendCustomEvent(kCustomEventType1);
  });
}

TEST(InteractionSequenceTest, BaitAndSwitchCustomEventDuringStepTransition) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();

  auto step_start =
      base::BindLambdaForTesting([&](InteractionSequence*, TrackedElement*) {
        element3.Show();
        element2.Hide();
        element3.SendCustomEvent(kCustomEventType1);
      });

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(
              &element1, std::move(step_start)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .SetMustBeVisibleAtStart(true)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     BaitAndSwitchCustomEventDuringStepTransitionEventuallyConsistent) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();

  auto step_start =
      base::BindLambdaForTesting([&](InteractionSequence*, TrackedElement*) {
        // By hiding before showing the other element, there are no visible
        // elements. However, because the system becomes consistent before the
        // end of the callback and the event is triggered, we are allowed to
        // proceed. This is technically incorrect behavior but adds a bit of
        // forgiveness into the system if visibility of elements is updated in
        // arbitrary order.
        element2.Hide();
        element3.Show();
        element3.SendCustomEvent(kCustomEventType1);
      });

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(
              &element1, std::move(step_start)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .SetMustBeVisibleAtStart(true)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

// Test step default values:

TEST(InteractionSequenceTest, MustBeVisibleAtStart_DefaultsToTrueForActivated) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  auto sequence =
      InteractionSequence::Builder()
          .SetContext(element1.context())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_3(
      step1_end, Run, step2_end, Run, aborted,
      Run(test::SequenceAbortedMatcher(
          3, nullptr, element3.identifier(),
          InteractionSequence::StepType::kActivated,
          InteractionSequence::AbortedReason::kElementNotVisibleAtStartOfStep)),
      element1.Show());
}

TEST(InteractionSequenceTest,
     MustBeVisibleAtStart_DefaultsToTrueForCustomEventIfElementIdSet) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  auto sequence =
      InteractionSequence::Builder()
          .SetContext(element1.context())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_3(
      step1_end, Run, step2_end, Run, aborted,
      Run(test::SequenceAbortedMatcher(
          3, nullptr, element3.identifier(),
          InteractionSequence::StepType::kCustomEvent,
          InteractionSequence::AbortedReason::kElementNotVisibleAtStartOfStep)),
      element1.Show());
}

TEST(InteractionSequenceTest,
     MustBeVisibleAtStart_DefaultsToFalseForCustomEventWithUnnamedElement) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  auto sequence =
      InteractionSequence::Builder()
          .SetContext(element1.context())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(step1_end, Run, element1.Show());
  element3.Show();
  EXPECT_CALLS_IN_SCOPE_2(step2_end, Run, completed, Run,
                          element3.SendCustomEvent(kCustomEventType1));
}

TEST(InteractionSequenceTest,
     MustRemainVisible_DefaultsBasedOnCurrentAndNextStep_Activation) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetContext(element1.context())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          // Shown followed by hidden defaults to must_remain_visible = false.
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .Build())
          // Activated step defaults to false.
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetMustBeVisibleAtStart(false)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          // Shown followed by activated defaults to true.
          // (We will fail the sequence on this step.)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();

  sequence->Start();
  // Trigger step 2.
  element1.Hide();
  element2.Show();
  // Trigger step 3.
  element2.Activate();
  // Trigger step 4.
  element3.Show();

  // Fail step four.
  EXPECT_CALL_IN_SCOPE(
      aborted,
      Run(test::SequenceAbortedMatcher(
          4, &element3, element3.identifier(),
          InteractionSequence::StepType::kShown,
          InteractionSequence::AbortedReason::kElementHiddenDuringStep)),
      element3.Hide());
}

TEST(InteractionSequenceTest,
     MustRemainVisible_DefaultsBasedOnCurrentAndNextStep_CustomEvents) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetContext(element1.context())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          // Shown followed by hidden defaults to must_remain_visible = false.
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .Build())
          // Activated step defaults to false.
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetMustBeVisibleAtStart(false)
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .Build())
          // Shown followed by activated defaults to true.
          // (We will fail the sequence on this step.)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType2)
                       .Build())
          .Build();

  sequence->Start();
  // Trigger step 2.
  element1.Hide();
  element2.Show();
  // Trigger step 3.
  element2.SendCustomEvent(kCustomEventType1);
  // Trigger step 4.
  element3.Show();

  // Fail step four.
  EXPECT_CALL_IN_SCOPE(
      aborted,
      Run(test::SequenceAbortedMatcher(
          4, &element3, element3.identifier(),
          InteractionSequence::StepType::kShown,
          InteractionSequence::AbortedReason::kElementHiddenDuringStep)),
      element3.Hide());
}

TEST(InteractionSequenceTest,
     MustRemainVisible_DefaultsBasedOnCurrentAndNextStep_Reshow) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetContext(element1.context())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          // Shown followed by reshow defaults to must_remain_visible = false.
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetTransitionOnlyOnEvent(true)
                       .Build())
          // Shown followed by different element defaults to true.
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetTransitionOnlyOnEvent(true)
                       .Build())
          .Build();

  sequence->Start();
  // Trigger step 2.
  element1.Hide();
  element1.Show();
  // Break the sequence at step 3.
  EXPECT_CALL_IN_SCOPE(
      aborted,
      Run(test::SequenceAbortedMatcher(
          3, testing::_, element1.identifier(),
          InteractionSequence::StepType::kShown,
          InteractionSequence::AbortedReason::kElementHiddenDuringStep)),
      element1.Hide());
}

// SetTransitionOnlyOnEvent tests:

TEST(InteractionSequenceTest,
     SetTransitionOnlyOnEvent_TransitionsOnDifferentElementShown) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step_start);
  // Two elements have the same identifier, but only the first is visible.
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetTransitionOnlyOnEvent(true)
                       .SetStartCallback(step_start.Get())
                       .Build())
          .Build();

  sequence->Start();

  // Fail step four.
  EXPECT_CALLS_IN_SCOPE_2(step_start, Run, completed, Run, element2.Show());
}

TEST(InteractionSequenceTest,
     SetTransitionOnlyOnEvent_TransitionsOnSameElementShown) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step_start);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetContext(element1.context())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustBeVisibleAtStart(true)
                       .SetMustRemainVisible(false)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetTransitionOnlyOnEvent(true)
                       .SetStartCallback(step_start.Get())
                       .Build())
          .Build();

  sequence->Start();
  element1.Hide();

  // Fail step four.
  EXPECT_CALLS_IN_SCOPE_2(step_start, Run, completed, Run, element1.Show());
}

TEST(InteractionSequenceTest,
     SetTransitionOnlyOnEvent_TransitionsOnElementHidden) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step_start);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetTransitionOnlyOnEvent(true)
                       .SetStartCallback(step_start.Get())
                       .Build())
          .Build();

  sequence->Start();
  element2.Show();

  // Fail step four.
  EXPECT_CALLS_IN_SCOPE_2(step_start, Run, completed, Run, element2.Hide());
}

// Named element tests:

TEST(InteractionSequenceTest,
     NameElement_ElementShown_NamedBeforeSequenceStarts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();
  sequence->NameElement(&element2, kElementName1);
  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_ElementShown_NamedDuringStepCallback_SameIdentifier) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier1, kTestContext1);
  auto step1_start = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        EXPECT_EQ(&element1, element);
        sequence->NameElement(&element2, kElementName1);
      });
  element1.Show();
  element2.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  sequence->NameElement(&element2, kElementName1);
  EXPECT_CALLS_IN_SCOPE_2(step, Run(sequence.get(), &element2), completed, Run,
                          sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_ElementShown_NamedDuringStepCallback_DifferentIdentifiers) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  auto step1_start = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        EXPECT_EQ(&element1, element);
        sequence->NameElement(&element2, kElementName1);
      });
  element1.Show();
  element2.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  sequence->NameElement(&element2, kElementName1);
  EXPECT_CALLS_IN_SCOPE_2(step, Run(sequence.get(), &element2), completed, Run,
                          sequence->Start());
}

TEST(InteractionSequenceTest, NameElement_ElementShown_FirstElementNamed) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();
  sequence->NameElement(&element1, kElementName1);
  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest, NameElement_ElementShown_MultipleNamedElements) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier1, kTestContext1);
  element1.Show();
  element2.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step1_start.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName2)
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step2_start.Get())
                       .Build())
          .Build();
  sequence->NameElement(&element1, kElementName1);
  sequence->NameElement(&element2, kElementName2);
  EXPECT_CALLS_IN_SCOPE_3(step1_start, Run(sequence.get(), &element1),
                          step2_start, Run(sequence.get(), &element2),
                          completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_ElementShown_DisappearsBeforeSequenceStart) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();
  sequence->NameElement(&element1, kElementName1);
  element1.Hide();
  EXPECT_CALL_IN_SCOPE(
      aborted,
      Run(test::SequenceAbortedMatcher(
          1, nullptr, element1.identifier(),
          InteractionSequence::StepType::kShown,
          InteractionSequence::AbortedReason::kElementNotVisibleAtStartOfStep)),
      sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_ElementShown_DisappearsBeforeStepAbortsTheSequence) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();
  sequence->NameElement(&element2, kElementName1);
  sequence->Start();
  element2.Hide();
  EXPECT_CALL_IN_SCOPE(
      aborted,
      Run(test::SequenceAbortedMatcher(
          3, nullptr, element2.identifier(),
          InteractionSequence::StepType::kShown,
          InteractionSequence::AbortedReason::kElementNotVisibleAtStartOfStep)),
      element1.Activate());
}

TEST(InteractionSequenceTest,
     NameElement_ElementShown_RespectsMustRemainVisibleFalse) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustRemainVisible(false)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->NameElement(&element2, kElementName1);
  sequence->Start();
  element2.Hide();
  EXPECT_CALL_IN_SCOPE(completed, Run, element1.Activate());
}

TEST(InteractionSequenceTest,
     NameElement_ElementActivated_NamedBeforeSequenceStarts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->NameElement(&element1, kElementName1);
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element1.Activate());
}

TEST(InteractionSequenceTest,
     NameElement_ElementActivated_NamedBeforeSequenceStarts_AbortsIfHidden) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->NameElement(&element1, kElementName1);
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, element1.Hide());
}

TEST(InteractionSequenceTest, NameElement_ElementActivated_NamedDuringStep) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto step = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        sequence->NameElement(&element2, kElementName1);
      });
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1,
                                                           std::move(step)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Activate());
}

TEST(InteractionSequenceTest,
     NameElement_ElementActivated_NamedDuringStep_AbortsIfHidden) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto step = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        sequence->NameElement(&element2, kElementName1);
      });
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1,
                                                           std::move(step)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, element2.Hide());
}

TEST(InteractionSequenceTest,
     NameElement_ElementActivated_NamedAndActivatedDuringStep) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto step = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        sequence->NameElement(&element2, kElementName1);
        element2.Activate();
      });
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1,
                                                           std::move(step)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_ElementActivated_NamedAndHiddenDuringStep_Aborts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto step = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        sequence->NameElement(&element2, kElementName1);
        element2.Hide();
      });
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1,
                                                           std::move(step)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_ElementActivated_NamedActivatedAndHiddenDuringStep) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto step = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        sequence->NameElement(&element2, kElementName1);
        element2.Activate();
        element2.Hide();
      });
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1,
                                                           std::move(step)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_CustomEvent_NamedBeforeSequenceStarts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .Build())
          .Build();
  sequence->NameElement(&element1, kElementName1);
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run,
                       element1.SendCustomEvent(kCustomEventType1));
}

TEST(InteractionSequenceTest,
     NameElement_CustomEvent_NamedBeforeSequenceStarts_AbortsIfHidden) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .Build())
          .Build();
  sequence->NameElement(&element1, kElementName1);
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, element1.Hide());
}

TEST(InteractionSequenceTest, NameElement_CustomEvent_NamedDuringStep) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto step = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        sequence->NameElement(&element2, kElementName1);
      });
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1,
                                                           std::move(step)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run,
                       element2.SendCustomEvent(kCustomEventType1));
}

TEST(InteractionSequenceTest,
     NameElement_CustomEvent_NamedDuringStep_AbortsIfHidden) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto step = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        sequence->NameElement(&element2, kElementName1);
      });
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1,
                                                           std::move(step)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, element2.Hide());
}

TEST(InteractionSequenceTest,
     NameElement_CustomEvent_NamedAndActivatedDuringStep) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto step = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        sequence->NameElement(&element2, kElementName1);
        element2.SendCustomEvent(kCustomEventType1);
      });
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1,
                                                           std::move(step)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_CustomEvent_NamedAndHiddenDuringStep_Aborts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto step = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        sequence->NameElement(&element2, kElementName1);
        element2.Hide();
      });
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1,
                                                           std::move(step)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_CustomEvent_NamedActivatedAndHiddenDuringStep) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto step = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        sequence->NameElement(&element2, kElementName1);
        element2.SendCustomEvent(kCustomEventType1);
        element2.Hide();
      });
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1,
                                                           std::move(step)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_ElementHidden_NamedBeforeSequenceAndHiddenBeforeSequence) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kHidden)
                       .Build())
          .Build();
  sequence->NameElement(&element2, kElementName1);
  element2.Hide();
  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_ElementHidden_NamedBeforeSequenceAndHiddenDuringSequence) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kHidden)
                       .Build())
          .Build();
  sequence->NameElement(&element2, kElementName1);
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Hide());
}

TEST(InteractionSequenceTest,
     NameElement_ElementHidden_NamedDuringCallbackAndHiddenDuringSequence) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto step = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        sequence->NameElement(&element2, kElementName1);
      });
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1,
                                                           std::move(step)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kHidden)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Hide());
}

TEST(InteractionSequenceTest,
     NameElement_ElementHidden_NamedAndHiddenDuringCallback) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto step = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        sequence->NameElement(&element2, kElementName1);
        element2.Hide();
      });
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1,
                                                           std::move(step)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kHidden)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest, NameElement_ElementHidden_NoElementExplicitly) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kHidden)
                       .Build())
          .Build();
  sequence->NameElement(nullptr, kElementName1);
  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_ElementShown_NoElementExplicitly_Aborts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();
  sequence->NameElement(nullptr, kElementName1);
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_ElementActivated_NoElementExplicitly_Aborts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->NameElement(nullptr, kElementName1);
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_TwoSequencesWithSameElementWithDifferentNames) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed2);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto sequence1 =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted1.Get())
          .SetCompletedCallback(completed1.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  auto sequence2 =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted2.Get())
          .SetCompletedCallback(completed2.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName2)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence1->NameElement(&element2, kElementName1);
  sequence2->NameElement(&element2, kElementName2);
  sequence1->Start();
  sequence2->Start();
  EXPECT_CALL(completed1, Run).Times(1);
  EXPECT_CALL(completed2, Run).Times(1);
  element2.Activate();
}

// RunSynchronouslyForTesting() tests:

TEST(InteractionSequenceTest,
     RunSynchronouslyForTesting_SequenceAbortsDuringStart) {
  base::test::TaskEnvironment task_environment;
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner({});
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);

  std::unique_ptr<InteractionSequence> sequence;
  sequence = InteractionSequence::Builder()
                 .SetAbortedCallback(aborted.Get())
                 .SetCompletedCallback(completed.Get())
                 .SetContext(kTestContext1)
                 .AddStep(InteractionSequence::StepBuilder()
                              .SetElementID(element1.identifier())
                              .SetType(InteractionSequence::StepType::kShown)
                              .SetMustBeVisibleAtStart(true)
                              .Build())
                 .Build();
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->RunSynchronouslyForTesting());
}

TEST(InteractionSequenceTest,
     RunSynchronouslyForTesting_SequenceCompletesDuringStart) {
  base::test::TaskEnvironment task_environment;
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner({});
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  sequence = InteractionSequence::Builder()
                 .SetAbortedCallback(aborted.Get())
                 .SetCompletedCallback(completed.Get())
                 .SetContext(kTestContext1)
                 .AddStep(InteractionSequence::StepBuilder()
                              .SetElementID(element1.identifier())
                              .SetType(InteractionSequence::StepType::kShown)
                              .SetMustBeVisibleAtStart(true)
                              .Build())
                 .Build();
  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

TEST(InteractionSequenceTest,
     RunSynchronouslyForTesting_SequenceAbortsDuringStep) {
  base::test::SingleThreadTaskEnvironment task_environment;
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  sequence =
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
                      [&](InteractionSequence*, TrackedElement*) {
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
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->RunSynchronouslyForTesting());
}

TEST(InteractionSequenceTest,
     RunSynchronouslyForTesting_SequenceCompletesDuringStep) {
  base::test::SingleThreadTaskEnvironment task_environment;

  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(
              InteractionSequence::StepBuilder()
                  .SetElementID(element1.identifier())
                  .SetType(InteractionSequence::StepType::kShown)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](InteractionSequence*, TrackedElement*) {
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
  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

TEST(InteractionSequenceTest, AddStepWithUnBuildStepBuilder) {
  auto sequence =
      InteractionSequence::Builder()
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetType(InteractionSequence::StepType::kActivated))
          .Build();
  sequence.reset();
}

// Element shown in any context tests.

TEST(InteractionSequenceTest, StartsOnElementShownInAnyContext) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element(kTestIdentifier1, kTestContext2);
  element.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetContext(InteractionSequence::ContextMode::kAny)
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest, TransitionsOnElementShownInAnyContext) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext2);
  element1.Show();
  element2.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetContext(InteractionSequence::ContextMode::kAny)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     ElementShownInAnyContextMustBeVisibleAtStartFirstStepAborts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element(kTestIdentifier1, kTestContext2);

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetContext(InteractionSequence::ContextMode::kAny)
                       .SetMustBeVisibleAtStart(true)
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     ElementShownInAnyContextMustBeVisibleAtStartLaterStepAborts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext2);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustBeVisibleAtStart(true)
                       .SetContext(InteractionSequence::ContextMode::kAny)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->Start());
}

TEST(InteractionSequenceTest, ElementShownInAnyContextTransitionOnEvent) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier1, kTestContext2);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetTransitionOnlyOnEvent(true)
                       .SetContext(InteractionSequence::ContextMode::kAny)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Show());
}

TEST(InteractionSequenceTest, ElementShownInAnyContextAssignNameAndActivate) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext2);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetContext(InteractionSequence::ContextMode::kAny)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](InteractionSequence* sequence,
                               TrackedElement* element) {
                             sequence->NameElement(element, kElementName1);
                           }))
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->Start();
  element2.Show();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Activate());
}

TEST(InteractionSequenceTest,
     ElementShownInAnyContextAssignNameAndSendCustomEvent) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext2);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetContext(InteractionSequence::ContextMode::kAny)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](InteractionSequence* sequence,
                               TrackedElement* element) {
                             sequence->NameElement(element, kElementName1);
                           }))
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .Build())
          .Build();
  sequence->Start();
  element2.Show();
  EXPECT_CALL_IN_SCOPE(completed, Run,
                       element2.SendCustomEvent(kCustomEventType1));
}

TEST(InteractionSequenceTest, ElementShownInAnyContextActivateDuringCallback) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext2);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetContext(InteractionSequence::ContextMode::kAny)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](InteractionSequence* sequence,
                               TrackedElement* element) {
                             sequence->NameElement(element, kElementName1);
                             element2.Activate();
                           }))
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Show());
}

TEST(InteractionSequenceTest,
     ElementShownInAnyContextSendCustomEventDuringCallback) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext2);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetContext(InteractionSequence::ContextMode::kAny)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](InteractionSequence* sequence,
                               TrackedElement* element) {
                             sequence->NameElement(element, kElementName1);
                             element2.SendCustomEvent(kCustomEventType1);
                           }))
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Show());
}

TEST(InteractionSequenceTest,
     ElementShownInAnyContextActivateAndHideDuringCallback) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext2);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetContext(InteractionSequence::ContextMode::kAny)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](InteractionSequence* sequence,
                               TrackedElement* element) {
                             sequence->NameElement(element, kElementName1);
                             element2.Activate();
                             element2.Hide();
                           }))
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Show());
}

TEST(InteractionSequenceTest,
     ElementShownInAnyContextSendCustomEventAndHideDuringCallback) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext2);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetContext(InteractionSequence::ContextMode::kAny)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](InteractionSequence* sequence,
                               TrackedElement* element) {
                             sequence->NameElement(element, kElementName1);
                             element2.SendCustomEvent(kCustomEventType1);
                             element2.Hide();
                           }))
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Show());
}

// Elements destroyed by external code during callbacks.

TEST(InteractionSequenceTest, DestroyElementDuringShow) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);

  auto subscription =
      ElementTracker::GetElementTracker()->AddElementShownCallback(
          element1.identifier(), element1.context(),
          base::BindLambdaForTesting(
              [&](TrackedElement*) { element1.Hide(); }));

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetContext(element1.context())
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetElementID(element1.identifier())
                       .Build())
          .Build();

  sequence->Start();

  // This show will be pre-empted by the initial subscription we added which
  // hides the element.
  element1.Show();

  // Remove the callback that hides the element.
  subscription = ElementTracker::Subscription();

  // Now the sequence should work.
  EXPECT_CALL_IN_SCOPE(completed, Run, element1.Show());
}

TEST(InteractionSequenceTest, DestroyElementDuringActivate) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);

  auto subscription =
      ElementTracker::GetElementTracker()->AddElementActivatedCallback(
          element1.identifier(), element1.context(),
          base::BindLambdaForTesting(
              [&](TrackedElement*) { element1.Hide(); }));

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetContext(element1.context())
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetElementID(element1.identifier())
                       .SetMustBeVisibleAtStart(false)
                       .SetMustRemainVisible(false)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetElementID(element1.identifier())
                       .SetMustBeVisibleAtStart(false)
                       .Build())
          .Build();

  sequence->Start();
  element1.Show();

  // This activate will be pre-empted by the initial subscription we added which
  // hides the element.
  element1.Activate();

  // Remove the callback that hides the element.
  subscription = ElementTracker::Subscription();

  // Now the sequence should work.
  element1.Show();
  EXPECT_CALL_IN_SCOPE(completed, Run, element1.Activate());
}

TEST(InteractionSequenceTest, DestroyedElementDuringNestedEvents) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);

  std::unique_ptr<InteractionSequence> sequence =
      InteractionSequence::Builder()
          .SetContext(element1.context())
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetElementID(element1.identifier())
                       .SetMustBeVisibleAtStart(false)
                       .SetMustRemainVisible(false)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetElementID(element1.identifier())
                       .SetMustBeVisibleAtStart(false)
                       .Build())
          .Build();

  // The first step will register its listener first.
  sequence->Start();

  // Now we can register a couple of chained listeners.
  auto subscription2 =
      ElementTracker::GetElementTracker()->AddElementActivatedCallback(
          element1.identifier(), element1.context(),
          base::BindLambdaForTesting(
              [&](TrackedElement*) { element1.Hide(); }));
  auto subscription =
      ElementTracker::GetElementTracker()->AddElementShownCallback(
          element1.identifier(), element1.context(),
          base::BindLambdaForTesting(
              [&](TrackedElement*) { element1.Activate(); }));

  // This will transition the first step, and activate the second callback, but
  // it will be pre-empted by the second subscription.
  element1.Show();

  // Remove the callback that hides the element.
  subscription2 = ElementTracker::Subscription();

  // Now the sequence should work.
  EXPECT_CALL_IN_SCOPE(completed, Run, element1.Show());
}

TEST(InteractionSequenceTest, StepStartEndConvenienceMethods) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<void(TrackedElement*)>,
                         step1_start);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step1_end);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step2_start);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustBeVisibleAtStart(false)
                       .SetStartCallback(step1_start.Get())
                       .SetEndCallback(step1_end.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2_start.Get()))
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(step1_start, Run(&element), element.Show());
  EXPECT_CALLS_IN_SCOPE_3(step1_end, Run, step2_start, Run, completed, Run,
                          element.Activate());
}

// Fail for testing tests.

TEST(InteractionSequenceTest, FailForTestingBetweenSteps) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element.context())
          .AddStep(InteractionSequence::StepBuilder().SetElementID(
              element.identifier()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetElementID(element.identifier()))
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(
      aborted,
      Run(test::SequenceAbortedMatcher(
          1, &element, element.identifier(),
          InteractionSequence::StepType::kShown,
          InteractionSequence::AbortedReason::kFailedForTesting)),
      sequence->FailForTesting());
}

TEST(InteractionSequenceTest, FailForTestingOnLastStepCallback) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element.context())
          .AddStep(InteractionSequence::StepBuilder().SetElementID(
              element.identifier()))
          .AddStep(
              InteractionSequence::StepBuilder()
                  .SetType(InteractionSequence::StepType::kActivated)
                  .SetElementID(element.identifier())
                  .SetStartCallback(base::BindOnce(
                      [](ui::InteractionSequence* seq, ui::TrackedElement* el) {
                        seq->FailForTesting();
                      })))
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(
      aborted,
      Run(test::SequenceAbortedMatcher(
          2, &element, element.identifier(),
          InteractionSequence::StepType::kActivated,
          InteractionSequence::AbortedReason::kFailedForTesting)),
      element.Activate());
}

// Context-switching and ContextMode tests.

TEST(InteractionSequenceTest, ExplicitContextChange) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier1, kTestContext2);
  element.Show();
  element2.Show();

  auto sequence = InteractionSequence::Builder()
                      .SetAbortedCallback(aborted.Get())
                      .SetCompletedCallback(completed.Get())
                      .AddStep(InteractionSequence::StepBuilder()
                                   .SetElementID(element.identifier())
                                   .SetContext(element.context())
                                   .SetStartCallback(step1.Get()))
                      .AddStep(InteractionSequence::StepBuilder()
                                   .SetElementID(element2.identifier())
                                   .SetContext(element2.context())
                                   .SetStartCallback(step2.Get()))
                      .Build();

  EXPECT_CALLS_IN_SCOPE_3(step1, Run(sequence.get(), &element), step2,
                          Run(sequence.get(), &element2), completed, Run,
                          sequence->Start());
}

TEST(InteractionSequenceTest, InheritContextFromPreviousStep) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier1, kTestContext2);
  element.Show();
  element2.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetContext(element2.context())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetContext(element.context()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetContext(
                           InteractionSequence::ContextMode::kFromPreviousStep)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step1.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetContext(element2.context()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetContext(
                           InteractionSequence::ContextMode::kFromPreviousStep)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2.Get()))
          .Build();

  sequence->Start();
  element2.Activate();
  EXPECT_CALL_IN_SCOPE(step1, Run, element.Activate());
  element.Activate();
  EXPECT_CALLS_IN_SCOPE_2(step2, Run, completed, Run, element2.Activate());
}

TEST(InteractionSequenceTest, InheritContextFromNamedElement) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  constexpr char kOtherElementName[] = "Other Element";
  test::TestElement element(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier1, kTestContext2);
  test::TestElement element3(kTestIdentifier2, kTestContext2);
  element.Show();
  element2.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetContext(element.context())
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](InteractionSequence* seq, TrackedElement*) {
                             seq->NameElement(&element2, kOtherElementName);
                           })))
          .AddStep(InteractionSequence::StepBuilder().SetElementName(
              kOtherElementName))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kOtherElementName)
                       .SetType(InteractionSequence::StepType::kActivated))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetContext(
                           InteractionSequence::ContextMode::kFromPreviousStep))
          .Build();

  sequence->Start();
  element2.Activate();
  EXPECT_CALL_IN_SCOPE(completed, Run, element3.Show());
}

TEST(InteractionSequenceTest,
     InheritContextFromNamedElement_TransitionOnEventDuringCallback) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  constexpr char kOtherElementName[] = "Other Element";
  test::TestElement element(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier1, kTestContext2);
  test::TestElement element3(kTestIdentifier2, kTestContext2);
  element.Show();
  element2.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetContext(element.context())
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](InteractionSequence* seq, TrackedElement*) {
                             seq->NameElement(&element2, kOtherElementName);
                           })))
          .AddStep(InteractionSequence::StepBuilder().SetElementName(
              kOtherElementName))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kOtherElementName)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&]() { element3.Show(); })))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetContext(
                           InteractionSequence::ContextMode::kFromPreviousStep)
                       .SetTransitionOnlyOnEvent(true))
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Activate());
}

TEST(InteractionSequenceTest, AnyContext_TransitionOnEventDuringCallback) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  constexpr char kOtherElementName[] = "Other Element";
  test::TestElement element(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier1, kTestContext2);
  test::TestElement element3(kTestIdentifier2, kTestContext3);
  element.Show();
  element2.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetContext(element.context())
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](InteractionSequence* seq, TrackedElement*) {
                             seq->NameElement(&element2, kOtherElementName);
                           })))
          .AddStep(InteractionSequence::StepBuilder().SetElementName(
              kOtherElementName))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kOtherElementName)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&]() { element3.Show(); })))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetContext(InteractionSequence::ContextMode::kAny)
                       .SetTransitionOnlyOnEvent(true))
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Activate());
}

// AbortData tests.

TEST(InteractionSequenceTest, BuildAbortDataForTimeout) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element.Show();

  // The same values should be returned if the sequence times out waiting for
  // the first step vs. during the first step start callback. Note that
  // `element` will only be set if the step is currently in progress.
  auto check_aborted_data =
      [](const InteractionSequence* seq, int step_index, ElementIdentifier id,
         const TrackedElement* element, const char* description) {
        const auto data = seq->BuildAbortedData(
            InteractionSequence::AbortedReason::kSequenceTimedOut);
        EXPECT_EQ(InteractionSequence::AbortedReason::kSequenceTimedOut,
                  data.aborted_reason);
        EXPECT_EQ(step_index, data.step_index);
        EXPECT_EQ(InteractionSequence::StepType::kShown, data.step_type);
        EXPECT_EQ(description, data.step_description);
        EXPECT_EQ(id, data.element_id);
        EXPECT_EQ(element, data.element.get());
      };

  auto sequence =
      InteractionSequence::Builder()
          .SetContext(element.context())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetDescription(kStepDescription)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&check_aborted_data](InteractionSequence* seq,
                                                 TrackedElement* el) {
                             // Verify that the correct values are returned if a
                             // timeout happens during a step start callback.
                             // Note how the element is set in this case, as a
                             // step is currently executing.
                             check_aborted_data(seq, 1, el->identifier(), el,
                                                kStepDescription);
                           })))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetDescription(kStepDescription2))
          .Build();

  // Verify that the correct values are returned if a timeout happens before the
  // first step.
  check_aborted_data(sequence.get(), 1, element.identifier(), nullptr,
                     kStepDescription);

  // Verify that the correct values are returned if a timeout hasppens between
  // steps.
  sequence->Start();
  check_aborted_data(sequence.get(), 2, element2.identifier(), nullptr,
                     kStepDescription2);

  // Complete the sequence.
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Show());
}

// ContextMode::kAny tests.

TEST(InteractionSequenceTest, ActivatedInAnyContext) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step3);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier1, kTestContext2);
  test::TestElement duplicate(kTestIdentifier1, kTestContext2);
  element2.Show();
  duplicate.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetContext(element.context())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetContext(InteractionSequence::ContextMode::kAny)
                       .SetStartCallback(step1.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetContext(
                           InteractionSequence::ContextMode::kFromPreviousStep)
                       .SetStartCallback(step2.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetContext(InteractionSequence::ContextMode::kAny)
                       .SetTransitionOnlyOnEvent(true))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetContext(InteractionSequence::ContextMode::kAny)
                       .SetStartCallback(step3.Get())
                       .SetMustBeVisibleAtStart(true))
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(step1, Run, element2.Activate());
  EXPECT_CALL_IN_SCOPE(step2, Run, duplicate.Activate());
  element.Show();
  EXPECT_CALLS_IN_SCOPE_2(step3, Run, completed, Run, element.Activate());
}

TEST(InteractionSequenceTest, ActivatedInAnyContextDuringStepTransition) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier1, kTestContext2);
  element2.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetContext(element.context())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetContext(InteractionSequence::ContextMode::kAny)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&element2]() { element2.Activate(); }))
                       .SetTransitionOnlyOnEvent(true))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetContext(InteractionSequence::ContextMode::kAny))
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element.Show());
}

TEST(InteractionSequenceTest, EventInAnyContext) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step3);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier1, kTestContext2);
  test::TestElement duplicate(kTestIdentifier1, kTestContext2);
  element.Show();
  element2.Show();
  duplicate.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetContext(element.context())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .SetContext(InteractionSequence::ContextMode::kAny)
                       .SetStartCallback(step1.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .SetContext(
                           InteractionSequence::ContextMode::kFromPreviousStep)
                       .SetStartCallback(step2.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType2)
                       .SetContext(InteractionSequence::ContextMode::kAny)
                       .SetStartCallback(step3.Get())
                       .SetMustBeVisibleAtStart(true))
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(step1, Run, element2.SendCustomEvent(kCustomEventType1));
  EXPECT_CALL_IN_SCOPE(step2, Run,
                       duplicate.SendCustomEvent(kCustomEventType1));
  EXPECT_CALLS_IN_SCOPE_2(step3, Run, completed, Run,
                          element.SendCustomEvent(kCustomEventType2));
}

TEST(InteractionSequenceTest, EventInAnyContextDuringStepTransition) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier1, kTestContext2);
  element2.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetContext(element.context())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(
              InteractionSequence::StepBuilder()
                  .SetElementID(element.identifier())
                  .SetContext(InteractionSequence::ContextMode::kAny)
                  .SetStartCallback(base::BindLambdaForTesting([&element2]() {
                    element2.SendCustomEvent(kCustomEventType1);
                  }))
                  .SetTransitionOnlyOnEvent(true))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kCustomEvent,
                                kCustomEventType1)
                       .SetContext(InteractionSequence::ContextMode::kAny))
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element.Show());
}

TEST(InteractionSequenceTest,
     ActivatedInAnyContextMustBeVisibleFailsIfLastElementIsHidden) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext2);
  element2.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetContext(element.context())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::StepBuilder().SetElementID(
              element.identifier()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetContext(InteractionSequence::ContextMode::kAny))
          .Build();

  sequence->Start();
  element.Show();
  EXPECT_CALL_IN_SCOPE(aborted, Run, element2.Hide());
}

TEST(
    InteractionSequenceTest,
    ActivatedInAnyContextMustBeVisibleSucceedsIfLastElementInDefaultContextIsHidden) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext2);
  element2.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetContext(element.context())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetMustRemainVisible(false))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetContext(InteractionSequence::ContextMode::kAny))
          .Build();

  sequence->Start();
  element.Show();
  element.Hide();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Activate());
}

TEST(InteractionSequenceTest,
     HiddenInAnyContextTransitionsOnNoElementsPresent) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element(kTestIdentifier1, kTestContext1);

  auto sequence =
      InteractionSequence::Builder()
          .SetContext(element.context())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::StepBuilder().SetElementID(
              element.identifier()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier2)
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetContext(InteractionSequence::ContextMode::kAny))
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element.Show());
}

TEST(InteractionSequenceTest, HiddenInAnyContextTransitionsOnElementHidden) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier1, kTestContext2);
  element.Show();
  element2.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetContext(element.context())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::StepBuilder().SetElementID(
              element.identifier()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetContext(InteractionSequence::ContextMode::kAny))
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Hide());
}

TEST(InteractionSequenceTest, HiddenInAnyContextTransitionOnlyOnEvent) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element(kTestIdentifier1, kTestContext2);

  auto sequence =
      InteractionSequence::Builder()
          .SetContext(kTestContext1)
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetContext(InteractionSequence::ContextMode::kAny)
                       .SetMustBeVisibleAtStart(false)
                       .SetTransitionOnlyOnEvent(true))
          .Build();

  sequence->Start();
  element.Show();
  EXPECT_CALL_IN_SCOPE(completed, Run, element.Hide());
}

// Subsequence tests.

class InteractionSequenceSubsequenceTest
    : public testing::TestWithParam<InteractionSequence::SubsequenceMode> {
 public:
  InteractionSequenceSubsequenceTest() = default;
  ~InteractionSequenceSubsequenceTest() override = default;

  static void FlushEvents() {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  static auto DoNotRun() {
    return base::BindOnce([](const InteractionSequence*,
                             const TrackedElement*) { return false; });
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_P(InteractionSequenceSubsequenceTest, SubsequenceRuns) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step3);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetStartCallback(step1.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetSubsequenceMode(GetParam())
                       .AddSubsequence(
                           std::move(InteractionSequence::Builder().AddStep(
                               InteractionSequence::StepBuilder()
                                   .SetElementID(element2.identifier())
                                   .SetStartCallback(subsequence.Get()))))
                       .SetStartCallback(step2.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetStartCallback(step3.Get()))
          .Build();

  EXPECT_CALL_IN_SCOPE(step1, Run, sequence->Start());
  EXPECT_CALLS_IN_SCOPE_2(subsequence, Run, step2, Run, {
    FlushEvents();
    element2.Show();
  });
  EXPECT_CALLS_IN_SCOPE_2(step3, Run, completed, Run, element3.Show());
}

TEST_P(InteractionSequenceSubsequenceTest,
       SubsequenceWithFailedConditionSkips) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence2);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetStartCallback(step1.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetSubsequenceMode(GetParam())
                       // This subsequence will not run.
                       .AddSubsequence(
                           std::move(InteractionSequence::Builder().AddStep(
                               InteractionSequence::StepBuilder()
                                   .SetElementID(element2.identifier())
                                   .SetStartCallback(subsequence1.Get()))),
                           DoNotRun())
                       .AddSubsequence(
                           std::move(InteractionSequence::Builder().AddStep(
                               InteractionSequence::StepBuilder()
                                   .SetElementID(element3.identifier())
                                   .SetStartCallback(subsequence2.Get()))))
                       .SetStartCallback(step2.Get()))
          .Build();

  EXPECT_CALL_IN_SCOPE(step1, Run, sequence->Start());
  FlushEvents();
  EXPECT_CALLS_IN_SCOPE_3(subsequence2, Run, step2, Run, completed, Run,
                          element3.Show());
}

TEST_P(InteractionSequenceSubsequenceTest, SubsequenceDoesNotRun) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step3);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetStartCallback(step1.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetSubsequenceMode(GetParam())
                       .AddSubsequence(
                           std::move(InteractionSequence::Builder().AddStep(
                               InteractionSequence::StepBuilder()
                                   .SetElementID(element2.identifier())
                                   .SetStartCallback(subsequence.Get()))),
                           DoNotRun())
                       .SetStartCallback(step2.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetStartCallback(step3.Get()))
          .Build();

  switch (GetParam()) {
    case InteractionSequence::SubsequenceMode::kAll:
    case InteractionSequence::SubsequenceMode::kAtMostOne:
      // These will skip the test and then succeed.
      EXPECT_CALLS_IN_SCOPE_2(step1, Run, step2, Run, sequence->Start());
      FlushEvents();
      element2.Show();
      EXPECT_CALLS_IN_SCOPE_2(step3, Run, completed, Run, element3.Show());
      break;
    case InteractionSequence::SubsequenceMode::kAtLeastOne:
    case InteractionSequence::SubsequenceMode::kExactlyOne:
      // These will fail.
      EXPECT_CALLS_IN_SCOPE_2(step1, Run, aborted, Run, sequence->Start());
      // Ensure no crashes if items fail to queue.
      FlushEvents();
      break;
  }
}

TEST_P(InteractionSequenceSubsequenceTest, TwoEligibleSubsequences) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence2);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetStartCallback(step1.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetSubsequenceMode(GetParam())
                       .AddSubsequence(
                           std::move(InteractionSequence::Builder().AddStep(
                               InteractionSequence::StepBuilder()
                                   .SetElementID(element2.identifier())
                                   .SetStartCallback(subsequence1.Get()))))
                       .AddSubsequence(
                           std::move(InteractionSequence::Builder().AddStep(
                               InteractionSequence::StepBuilder()
                                   .SetElementID(element3.identifier())
                                   .SetStartCallback(subsequence2.Get()))))
                       .SetStartCallback(step2.Get()))
          .Build();

  EXPECT_CALL_IN_SCOPE(step1, Run, sequence->Start());
  FlushEvents();
  switch (GetParam()) {
    case InteractionSequence::SubsequenceMode::kAll:
      // Both sequences must complete before the sequence proceeds.
      EXPECT_CALL_IN_SCOPE(subsequence1, Run, element2.Show());
      EXPECT_CALLS_IN_SCOPE_3(subsequence2, Run, step2, Run, completed, Run,
                              element3.Show());
      break;
    case InteractionSequence::SubsequenceMode::kAtMostOne:
    case InteractionSequence::SubsequenceMode::kAtLeastOne:
    case InteractionSequence::SubsequenceMode::kExactlyOne:
      // Only the first sequence will run, or the second will abort as soon as
      // the first completes.
      EXPECT_CALLS_IN_SCOPE_3(subsequence1, Run, step2, Run, completed, Run,
                              element2.Show());
      break;
  }
}

TEST_P(InteractionSequenceSubsequenceTest,
       MultipleEligibleSubsequencesSomeIneligible) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence3);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetStartCallback(step1.Get())
                       .SetMustRemainVisible(false))
          .AddStep(
              InteractionSequence::StepBuilder()
                  .SetSubsequenceMode(GetParam())
                  // This subsequence will not run.
                  .AddSubsequence(
                      std::move(InteractionSequence::Builder().AddStep(
                          InteractionSequence::StepBuilder()
                              .SetElementID(element2.identifier())
                              .SetStartCallback(subsequence1.Get()))),
                      DoNotRun())
                  // The following two subsequences are identical and will call
                  // the same callback.
                  .AddSubsequence(
                      std::move(InteractionSequence::Builder().AddStep(
                          InteractionSequence::StepBuilder()
                              .SetElementID(element3.identifier())
                              .SetStartCallback(subsequence2.Get()))))
                  .AddSubsequence(
                      std::move(InteractionSequence::Builder().AddStep(
                          InteractionSequence::StepBuilder()
                              .SetElementID(element3.identifier())
                              .SetStartCallback(subsequence2.Get()))))
                  // This subsequence is unique.
                  .AddSubsequence(
                      std::move(InteractionSequence::Builder().AddStep(
                          InteractionSequence::StepBuilder()
                              .SetType(InteractionSequence::StepType::kHidden)
                              .SetElementID(element1.identifier())
                              .SetStartCallback(subsequence3.Get()))))
                  .SetStartCallback(step2.Get()))
          .Build();

  EXPECT_CALL_IN_SCOPE(step1, Run, sequence->Start());
  FlushEvents();
  element2.Show();
  switch (GetParam()) {
    case InteractionSequence::SubsequenceMode::kAll:
      // All three enabled sequences must complete before the sequence proceeds.
      EXPECT_CALL(subsequence2, Run).Times(2);
      element3.Show();
      EXPECT_CALLS_IN_SCOPE_3(subsequence3, Run, step2, Run, completed, Run,
                              element1.Hide());
      break;
    case InteractionSequence::SubsequenceMode::kAtMostOne:
    case InteractionSequence::SubsequenceMode::kAtLeastOne:
    case InteractionSequence::SubsequenceMode::kExactlyOne:
      // In these cases, either only one subsequence will run, or only one
      // will complete. Therefore, there should only be one subsequence
      // callback.
      EXPECT_CALLS_IN_SCOPE_3(subsequence2, Run, step2, Run, completed, Run,
                              element3.Show());
      element1.Hide();
      break;
  }
}

TEST_P(InteractionSequenceSubsequenceTest, SubsequenceWithMultipleSteps) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence2);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetStartCallback(step1.Get()))
          .AddStep(
              InteractionSequence::StepBuilder()
                  .SetSubsequenceMode(GetParam())
                  .AddSubsequence(std::move(
                      InteractionSequence::Builder()
                          .AddStep(InteractionSequence::StepBuilder()
                                       .SetElementID(element2.identifier())
                                       .SetStartCallback(subsequence1.Get()))
                          .AddStep(InteractionSequence::StepBuilder()
                                       .SetElementID(element3.identifier())
                                       .SetStartCallback(subsequence2.Get()))))
                  .SetStartCallback(step2.Get()))
          .Build();

  EXPECT_CALL_IN_SCOPE(step1, Run, sequence->Start());
  EXPECT_CALL_IN_SCOPE(subsequence1, Run, {
    FlushEvents();
    element2.Show();
  });
  EXPECT_CALLS_IN_SCOPE_3(subsequence2, Run, step2, Run, completed, Run,
                          element3.Show());
}

TEST_P(InteractionSequenceSubsequenceTest, SubsequenceFailsAtStart) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence1);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetStartCallback(step1.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetSubsequenceMode(GetParam())
                       .AddSubsequence(
                           std::move(InteractionSequence::Builder().AddStep(
                               InteractionSequence::StepBuilder()
                                   .SetElementID(element2.identifier())
                                   .SetMustBeVisibleAtStart(true)
                                   .SetStartCallback(subsequence1.Get()))))
                       .SetStartCallback(step2.Get()))
          .Build();

  EXPECT_CALL_IN_SCOPE(step1, Run, sequence->Start());
  EXPECT_CALL_IN_SCOPE(
      aborted,
      Run(test::SequenceAbortedMatcher(
          2, nullptr, ElementIdentifier(),
          InteractionSequence::StepType::kSubsequence,
          InteractionSequence::AbortedReason::kSubsequenceFailed, testing::_,
          testing::ElementsAre(testing::Optional(test::SequenceAbortedMatcher(
              1, nullptr, element2.identifier(),
              InteractionSequence::StepType::kShown,
              InteractionSequence::AbortedReason::
                  kElementNotVisibleAtStartOfStep))))),
      FlushEvents());
}

TEST_P(InteractionSequenceSubsequenceTest, SubsequenceFailsInMiddle) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence2);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetStartCallback(step1.Get()))
          .AddStep(
              InteractionSequence::StepBuilder()
                  .SetSubsequenceMode(GetParam())
                  .AddSubsequence(std::move(
                      InteractionSequence::Builder()
                          .AddStep(InteractionSequence::StepBuilder()
                                       .SetElementID(element2.identifier())
                                       .SetStartCallback(subsequence1.Get()))
                          .AddStep(InteractionSequence::StepBuilder()
                                       .SetElementID(element3.identifier())
                                       .SetMustBeVisibleAtStart(true)
                                       .SetStartCallback(subsequence2.Get()))))
                  .SetStartCallback(step2.Get()))
          .Build();

  EXPECT_CALL_IN_SCOPE(step1, Run, sequence->Start());
  EXPECT_CALLS_IN_SCOPE_2(
      subsequence1, Run, aborted,
      Run(test::SequenceAbortedMatcher(
          2, nullptr, ElementIdentifier(),
          InteractionSequence::StepType::kSubsequence,
          InteractionSequence::AbortedReason::kSubsequenceFailed, testing::_,
          testing::ElementsAre(testing::Optional(test::SequenceAbortedMatcher(
              2, nullptr, element3.identifier(),
              InteractionSequence::StepType::kShown,
              InteractionSequence::AbortedReason::
                  kElementNotVisibleAtStartOfStep))))),
      {
        FlushEvents();
        element2.Show();
      });
}

TEST_P(InteractionSequenceSubsequenceTest, FirstFailsSecondSucceeds) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence2);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetStartCallback(step1.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetSubsequenceMode(GetParam())
                       .AddSubsequence(
                           std::move(InteractionSequence::Builder().AddStep(
                               InteractionSequence::StepBuilder()
                                   .SetElementID(element2.identifier())
                                   .SetMustBeVisibleAtStart(true)
                                   .SetStartCallback(subsequence1.Get()))))
                       .AddSubsequence(
                           std::move(InteractionSequence::Builder().AddStep(
                               InteractionSequence::StepBuilder()
                                   .SetElementID(element3.identifier())
                                   .SetStartCallback(subsequence2.Get()))))
                       .SetStartCallback(step2.Get()))
          .Build();

  EXPECT_CALL_IN_SCOPE(step1, Run, sequence->Start());

  switch (GetParam()) {
    case InteractionSequence::SubsequenceMode::kAll:
    case InteractionSequence::SubsequenceMode::kAtMostOne:
    case InteractionSequence::SubsequenceMode::kExactlyOne:
      // The first failure should scuttle this attempt.
      EXPECT_CALL_IN_SCOPE(
          aborted,
          Run(test::SequenceAbortedMatcher(
              2, nullptr, ElementIdentifier(),
              InteractionSequence::StepType::kSubsequence,
              InteractionSequence::AbortedReason::kSubsequenceFailed,
              testing::_,
              testing::ElementsAre(
                  testing::Optional(test::SequenceAbortedMatcher(
                      1, nullptr, element2.identifier(),
                      InteractionSequence::StepType::kShown,
                      InteractionSequence::AbortedReason::
                          kElementNotVisibleAtStartOfStep)),
                  testing::Eq(absl::nullopt)))),
          FlushEvents());
      break;
    case InteractionSequence::SubsequenceMode::kAtLeastOne:
      // As long as one of the sequences succeeds, the step succeeds.
      FlushEvents();
      EXPECT_CALLS_IN_SCOPE_3(subsequence2, Run, step2, Run, completed, Run,
                              element3.Show());
      break;
  }
}

TEST_P(InteractionSequenceSubsequenceTest, FirstSucceedsSecondFails) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence2);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetStartCallback(step1.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetSubsequenceMode(GetParam())
                       .AddSubsequence(
                           std::move(InteractionSequence::Builder().AddStep(
                               InteractionSequence::StepBuilder()
                                   .SetElementID(element2.identifier())
                                   .SetStartCallback(subsequence1.Get()))))
                       .AddSubsequence(
                           std::move(InteractionSequence::Builder().AddStep(
                               InteractionSequence::StepBuilder()
                                   .SetElementID(element3.identifier())
                                   .SetMustBeVisibleAtStart(true)
                                   .SetStartCallback(subsequence2.Get()))))
                       .SetStartCallback(step2.Get()))
          .Build();

  EXPECT_CALL_IN_SCOPE(step1, Run, sequence->Start());

  switch (GetParam()) {
    case InteractionSequence::SubsequenceMode::kAll:
      // The first failure should scuttle this attempt.
      EXPECT_CALL_IN_SCOPE(
          aborted,
          Run(test::SequenceAbortedMatcher(
              2, nullptr, ElementIdentifier(),
              InteractionSequence::StepType::kSubsequence,
              InteractionSequence::AbortedReason::kSubsequenceFailed,
              testing::_,
              testing::ElementsAre(
                  testing::Eq(absl::nullopt),
                  testing::Optional(test::SequenceAbortedMatcher(
                      1, nullptr, element3.identifier(),
                      InteractionSequence::StepType::kShown,
                      InteractionSequence::AbortedReason::
                          kElementNotVisibleAtStartOfStep))))),
          FlushEvents());
      break;
    case InteractionSequence::SubsequenceMode::kAtMostOne:
    case InteractionSequence::SubsequenceMode::kExactlyOne:
    case InteractionSequence::SubsequenceMode::kAtLeastOne:
      // As long as one of the sequences succeeds, the step succeeds.
      FlushEvents();
      EXPECT_CALLS_IN_SCOPE_3(subsequence1, Run, step2, Run, completed, Run,
                              element2.Show());
      break;
  }
}

TEST_F(InteractionSequenceSubsequenceTest, ElementPassedToCondition) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::SubsequenceCondition, condition);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();
  element2.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetStartCallback(step1.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .AddSubsequence(
                           std::move(InteractionSequence::Builder().AddStep(
                               InteractionSequence::StepBuilder()
                                   .SetElementID(element3.identifier())
                                   .SetStartCallback(subsequence.Get()))),
                           condition.Get())
                       .SetStartCallback(step2.Get()))
          .Build();

  EXPECT_CALL(condition, Run(sequence.get(), &element2))
      .WillOnce(testing::Return(true));
  EXPECT_CALL_IN_SCOPE(step1, Run, sequence->Start());
  EXPECT_CALLS_IN_SCOPE_3(subsequence, Run, step2, Run, completed, Run, {
    FlushEvents();
    element3.Show();
  });
}

TEST_F(InteractionSequenceSubsequenceTest,
       ElementNotPassedToConditionIfNotVisible) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::SubsequenceCondition, condition);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetStartCallback(step1.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .AddSubsequence(
                           std::move(InteractionSequence::Builder().AddStep(
                               InteractionSequence::StepBuilder()
                                   .SetElementID(element3.identifier())
                                   .SetStartCallback(subsequence.Get()))),
                           condition.Get())
                       .SetStartCallback(step2.Get()))
          .Build();

  EXPECT_CALL(condition, Run(sequence.get(), nullptr))
      .WillOnce(testing::Return(true));
  EXPECT_CALL_IN_SCOPE(step1, Run, sequence->Start());
  EXPECT_CALLS_IN_SCOPE_3(subsequence, Run, step2, Run, completed, Run, {
    FlushEvents();
    element3.Show();
  });
}

TEST_F(InteractionSequenceSubsequenceTest, TriggerDuringPreviousStep) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&element1](InteractionSequence*, TrackedElement*) {
                             element1.SendCustomEvent(kCustomEventType1);
                           })))
          .AddStep(
              InteractionSequence::StepBuilder()
                  .AddSubsequence(
                      std::move(InteractionSequence::Builder().AddStep(
                          InteractionSequence::StepBuilder()
                              .SetElementID(element1.identifier())
                              .SetType(
                                  InteractionSequence::StepType::kCustomEvent,
                                  kCustomEventType1)
                              .SetStartCallback(subsequence.Get()))))
                  .SetStartCallback(step2.Get()))
          .Build();

  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_3(subsequence, Run, step2, Run, completed, Run,
                          FlushEvents());
}

TEST_F(InteractionSequenceSubsequenceTest, SubsequenceAsFirstStep) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .AddSubsequence(
                           std::move(InteractionSequence::Builder().AddStep(
                               InteractionSequence::StepBuilder()
                                   .SetElementID(element1.identifier())
                                   .SetStartCallback(subsequence.Get()))))
                       .SetStartCallback(step1.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetStartCallback(step2.Get()))
          .Build();

  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_2(subsequence, Run, step1, Run, FlushEvents());
  EXPECT_CALLS_IN_SCOPE_2(step2, Run, completed, Run, element2.Show());
}

TEST_F(InteractionSequenceSubsequenceTest, NestedSubsequenceAsFirstStep) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence2);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(element1.context())
          .AddStep(
              InteractionSequence::StepBuilder()
                  .AddSubsequence(
                      std::move(InteractionSequence::Builder().AddStep(
                          InteractionSequence::StepBuilder()
                              .AddSubsequence(std::move(
                                  InteractionSequence::Builder().AddStep(
                                      InteractionSequence::StepBuilder()
                                          .SetElementID(element1.identifier())
                                          .SetStartCallback(
                                              subsequence2.Get()))))
                              .SetStartCallback(subsequence1.Get()))))
                  .SetStartCallback(step1.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetStartCallback(step2.Get()))
          .Build();

  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_3(subsequence2, Run, subsequence1, Run, step1, Run, {
    FlushEvents();
    FlushEvents();
  });
  EXPECT_CALLS_IN_SCOPE_2(step2, Run, completed, Run, element2.Show());
}

TEST_F(InteractionSequenceSubsequenceTest,
       NestedSubsequencesTriggeredByEventBeforeStarted) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence2);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&element1](InteractionSequence*, TrackedElement*) {
                             element1.SendCustomEvent(kCustomEventType1);
                           })))
          .AddStep(
              InteractionSequence::StepBuilder()
                  .AddSubsequence(
                      std::move(InteractionSequence::Builder().AddStep(
                          InteractionSequence::StepBuilder()
                              .AddSubsequence(std::move(
                                  InteractionSequence::Builder().AddStep(
                                      InteractionSequence::StepBuilder()
                                          .SetElementID(element1.identifier())
                                          .SetType(InteractionSequence::
                                                       StepType::kCustomEvent,
                                                   kCustomEventType1)
                                          .SetStartCallback(
                                              subsequence2.Get()))))
                              .SetStartCallback(subsequence1.Get()))))
                  .SetStartCallback(step2.Get()))
          .Build();

  sequence->Start();
  EXPECT_CALL(subsequence2, Run);
  EXPECT_CALL(subsequence1, Run);
  EXPECT_CALL(step2, Run);
  EXPECT_CALL(completed, Run);
  FlushEvents();
  FlushEvents();
}

TEST_F(InteractionSequenceSubsequenceTest, SubsequenceInSameContext) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step3);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence1);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier1, kTestContext2);
  test::TestElement element3(kTestIdentifier2, kTestContext2);
  element1.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetStartCallback(step1.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetContext(element2.context())
                       .SetStartCallback(step2.Get()))
          .AddStep(
              InteractionSequence::StepBuilder()
                  .AddSubsequence(
                      std::move(InteractionSequence::Builder().AddStep(
                          InteractionSequence::StepBuilder()
                              .SetElementID(element3.identifier())
                              .SetContext(InteractionSequence::ContextMode::
                                              kFromPreviousStep)
                              .SetStartCallback(subsequence1.Get()))))
                  .SetStartCallback(step3.Get()))
          .Build();

  EXPECT_CALL_IN_SCOPE(step1, Run, sequence->Start());
  EXPECT_CALL_IN_SCOPE(step2, Run, element2.Show());
  EXPECT_CALLS_IN_SCOPE_3(subsequence1, Run, step3, Run, completed, Run, {
    FlushEvents();
    element3.Show();
  });
}

TEST_F(InteractionSequenceSubsequenceTest, NestedSubsequenceInSameContext) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step3);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence2);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier1, kTestContext2);
  test::TestElement element3(kTestIdentifier2, kTestContext2);
  element1.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetStartCallback(step1.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetContext(element2.context())
                       .SetStartCallback(step2.Get()))
          .AddStep(
              InteractionSequence::StepBuilder()
                  .AddSubsequence(
                      std::move(InteractionSequence::Builder().AddStep(
                          InteractionSequence::StepBuilder()
                              .AddSubsequence(std::move(
                                  InteractionSequence::Builder().AddStep(
                                      InteractionSequence::StepBuilder()
                                          .SetElementID(element3.identifier())
                                          .SetContext(
                                              InteractionSequence::ContextMode::
                                                  kFromPreviousStep)
                                          .SetStartCallback(
                                              subsequence2.Get()))))
                              .SetStartCallback(subsequence1.Get()))))
                  .SetStartCallback(step3.Get()))
          .Build();

  EXPECT_CALL_IN_SCOPE(step1, Run, sequence->Start());
  EXPECT_CALL_IN_SCOPE(step2, Run, element2.Show());
  EXPECT_CALL(subsequence2, Run);
  EXPECT_CALL(subsequence1, Run);
  EXPECT_CALL(step3, Run);
  EXPECT_CALL(completed, Run);
  FlushEvents();
  FlushEvents();
  element3.Show();
}

TEST_P(InteractionSequenceSubsequenceTest, SequenceDeletedDuringSubsequences) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, subsequence2);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetStartCallback(step1.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetSubsequenceMode(GetParam())
                       .AddSubsequence(
                           std::move(InteractionSequence::Builder().AddStep(
                               InteractionSequence::StepBuilder()
                                   .SetElementID(element2.identifier())
                                   .SetStartCallback(subsequence1.Get()))))
                       .AddSubsequence(
                           std::move(InteractionSequence::Builder().AddStep(
                               InteractionSequence::StepBuilder()
                                   .SetElementID(element3.identifier())
                                   .SetStartCallback(subsequence2.Get()))))
                       .SetStartCallback(step2.Get()))
          .Build();

  EXPECT_CALL_IN_SCOPE(step1, Run, sequence->Start());
  EXPECT_CALL_IN_SCOPE(
      aborted,
      Run(test::SequenceAbortedMatcher(
          2, nullptr, ElementIdentifier(),
          InteractionSequence::StepType::kSubsequence,
          InteractionSequence::AbortedReason::kSequenceDestroyed, testing::_,
          // TODO(dfried): Should kill and capture where each subsequence
          // aborted.
          testing::IsEmpty())),
      sequence.reset());
}

TEST_P(InteractionSequenceSubsequenceTest, NamedElements) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  test::TestElement element1(kTestIdentifier1, kTestContext1);
  test::TestElement element2(kTestIdentifier2, kTestContext1);
  test::TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();
  element2.Show();
  element3.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(element1.context())
          // Name a single element.
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&element2](InteractionSequence* seq,
                                       TrackedElement* el) {
                             seq->NameElement(&element2, kElementName1);
                           })))
          // Run one or more subsequences which refer to the named element
          // above, and which [re-]assign conflicting names.
          .AddStep(
              InteractionSequence::StepBuilder()
                  .SetSubsequenceMode(GetParam())
                  .AddSubsequence(std::move(
                      InteractionSequence::Builder()
                          .AddStep(
                              InteractionSequence::StepBuilder()
                                  .SetElementName(kElementName1)
                                  .SetStartCallback(base::BindLambdaForTesting(
                                      [&element3](InteractionSequence* seq,
                                                  TrackedElement* el) {
                                        // This assigns a second name.
                                        seq->NameElement(&element3,
                                                         kElementName2);
                                      })))
                          // Verify that the element is named as above, then
                          // reassign the first name.
                          .AddStep(
                              InteractionSequence::StepBuilder()
                                  .SetElementName(kElementName2)
                                  .SetStartCallback(base::BindLambdaForTesting(
                                      [&element1, &element3](
                                          InteractionSequence* seq,
                                          TrackedElement* el) {
                                        EXPECT_EQ(&element3, el);
                                        seq->NameElement(&element1,
                                                         kElementName1);
                                      })))
                          // Verify the reassignmed name.
                          .AddStep(
                              InteractionSequence::StepBuilder()
                                  .SetElementName(kElementName1)
                                  .SetStartCallback(base::BindLambdaForTesting(
                                      [&element1](TrackedElement* el) {
                                        EXPECT_EQ(&element1, el);
                                      })))))
                  .AddSubsequence(std::move(
                      InteractionSequence::Builder()
                          .AddStep(
                              InteractionSequence::StepBuilder()
                                  .SetElementID(element3.identifier())
                                  .SetStartCallback(base::BindLambdaForTesting(
                                      [&element2](InteractionSequence* seq,
                                                  TrackedElement* el) {
                                        // This assigns a second name.
                                        seq->NameElement(&element2,
                                                         kElementName2);
                                      })))
                          // Verify that the element is named above. Then
                          // reassign the original name.
                          .AddStep(
                              InteractionSequence::StepBuilder()
                                  .SetElementName(kElementName2)
                                  .SetStartCallback(base::BindLambdaForTesting(
                                      [&element2, &element3](
                                          InteractionSequence* seq,
                                          TrackedElement* el) {
                                        EXPECT_EQ(&element2, el);
                                        seq->NameElement(&element3,
                                                         kElementName1);
                                      })))
                          // Verify the reassignmed name.
                          .AddStep(
                              InteractionSequence::StepBuilder()
                                  .SetElementName(kElementName1)
                                  .SetStartCallback(base::BindLambdaForTesting(
                                      [&element3](TrackedElement* el) {
                                        EXPECT_EQ(&element3, el);
                                      }))))))
          // None of the (possibly conflicting) names changed in subsequences
          // should be present here.
          .AddStep(
              InteractionSequence::StepBuilder()
                  .SetElementName(kElementName1)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&element2](InteractionSequence* seq,
                                  TrackedElement* el) {
                        EXPECT_EQ(&element2, el)
                            << "Original named element should be unchanged.";
                        EXPECT_FALSE(
                            base::Contains(seq->named_elements_, kElementName2))
                            << "Element named in subsequence should not be "
                               "present here.";
                      })))
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, FlushEvents());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    InteractionSequenceSubsequenceTest,
    testing::Values(InteractionSequence::SubsequenceMode::kAtMostOne,
                    InteractionSequence::SubsequenceMode::kExactlyOne,
                    InteractionSequence::SubsequenceMode::kAtLeastOne,
                    InteractionSequence::SubsequenceMode::kAll),
    [](const testing::TestParamInfo<InteractionSequence::SubsequenceMode>&
           mode) {
      std::ostringstream oss;
      oss << mode.param;
      return oss.str();
    });

}  // namespace ui
