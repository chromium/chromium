// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interactive_test.h"
#include <functional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"

#if !BUILDFLAG(IS_IOS)
#include "ui/base/accelerators/accelerator.h"
#endif

namespace ui::test {

namespace {

enum class ActionType {
  kPressButton,
  kSelectMenuItem,
  kDoDefaultAction,
  kSelectTab,
  kSelectDropdownItem,
  kEnterText,
  kActivateSurface,
  kSendAccelerator,
  kConfirm
};

using ActionRecord = std::tuple<ActionType,
                                ElementIdentifier,
                                ElementContext,
                                InteractionTestUtil::InputType>;

const ui::ElementContext kTestContext1(1);
const ui::ElementContext kTestContext2(2);

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestId1);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestId2);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestId3);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestId4);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTestEvent1);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTestEvent2);

constexpr char kSetOnIncompatibleActionMessage[] =
    "Explicitly testing incompatibility-handling.";

class TestSimulator : public InteractionTestUtil::Simulator {
 public:
  TestSimulator() = default;
  ~TestSimulator() override = default;

  void set_result(ActionResult result) { result_ = result; }

  ActionResult PressButton(TrackedElement* element,
                           InputType input_type) override {
    DoAction(ActionType::kPressButton, element, input_type);
    return result_;
  }

  ActionResult SelectMenuItem(TrackedElement* element,
                              InputType input_type) override {
    DoAction(ActionType::kSelectMenuItem, element, input_type);
    return result_;
  }

  ActionResult DoDefaultAction(TrackedElement* element,
                               InputType input_type) override {
    DoAction(ActionType::kDoDefaultAction, element, input_type);
    return result_;
  }

  ActionResult SelectTab(TrackedElement* tab_collection,
                         size_t index,
                         InputType input_type) override {
    DoAction(ActionType::kSelectTab, tab_collection, input_type);
    return result_;
  }

  ActionResult SelectDropdownItem(TrackedElement* collection,
                                  size_t item,
                                  InputType input_type) override {
    DoAction(ActionType::kSelectDropdownItem, collection, input_type);
    return result_;
  }

  ActionResult EnterText(TrackedElement* element,
                         std::u16string text,
                         TextEntryMode mode) override {
    DoAction(ActionType::kEnterText, element, InputType::kKeyboard);
    return result_;
  }

  ActionResult ActivateSurface(TrackedElement* element) override {
    DoAction(ActionType::kActivateSurface, element, InputType::kMouse);
    return result_;
  }

#if !BUILDFLAG(IS_IOS)
  ActionResult SendAccelerator(TrackedElement* element,
                               Accelerator accel) override {
    DoAction(ActionType::kSendAccelerator, element, InputType::kKeyboard);
    return result_;
  }
#endif

  ActionResult Confirm(TrackedElement* element) override {
    DoAction(ActionType::kConfirm, element, InputType::kDontCare);
    return result_;
  }

  const std::vector<ActionRecord>& records() const { return records_; }

 private:
  void DoAction(ActionType action_type,
                TrackedElement* element,
                InputType input_type) {
    records_.emplace_back(action_type, element->identifier(),
                          element->context(), input_type);
    element->AsA<TestElement>()->Activate();
  }

  ActionResult result_ = ActionResult::kSucceeded;
  std::vector<ActionRecord> records_;
};

void DoFunction() {
  LOG(INFO) << "In normal function.";
}

const TrackedElement* CheckElementFunction(const TrackedElement* el) {
  return el;
}

int ValueGeneratingFunction() {
  return 5;
}

struct CallableObject {
  bool operator()() const { return i != 0; }
  int i = 0;
};

struct MutableCallableObject {
  bool operator()() { return i != 0; }
  int i = 0;
};

struct EmptyCallableObject {
  void operator()() const {}
};

}  // namespace

class InteractiveTestTest : public InteractiveTest {
 public:
  InteractiveTestTest() {
    auto simulator = std::make_unique<TestSimulator>();
    simulator_ = simulator.get();
    test_util().AddSimulator(std::move(simulator));
  }

 protected:
  TestSimulator* simulator() { return simulator_.get(); }

  void QueueActions(base::OnceClosure actions) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(actions));
  }

  base::raw_ptr<TestSimulator> simulator_ = nullptr;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
};

TEST_F(InteractiveTestTest, StepsConstructsMultiStep) {
  auto result =
      Steps(StepBuilder(), Steps(StepBuilder(), StepBuilder()), StepBuilder());

  EXPECT_EQ(4U, result.size());
}

TEST_F(InteractiveTestTest, RunTestSequenceInContext) {
  TestElement el(kTestId1, kTestContext1);
  el.Show();
  EXPECT_TRUE(RunTestSequenceInContext(kTestContext1, WaitForShow(kTestId1)));
}

TEST_F(InteractiveTestTest, InteractionVerbs) {
  TestElement e1(kTestId1, kTestContext1);
  TestElement e2(kTestId2, kTestContext1);
  TestElement e3(kTestId3, kTestContext1);
  TestElement e4(kTestId4, kTestContext1);
  e1.Show();
  e2.Show();
  e3.Show();
  e4.Show();
  RunTestSequenceInContext(
      kTestContext1, PressButton(kTestId1, InputType::kDontCare),
      SelectMenuItem(kTestId2, InputType::kKeyboard),
      DoDefaultAction(kTestId3, InputType::kMouse),
      SelectTab(kTestId4, 3U, InputType::kTouch),
      SelectDropdownItem(kTestId1, 2U, InputType::kDontCare),
      EnterText(kTestId2, u"The quick brown fox.", TextEntryMode::kAppend),
      ActivateSurface(kTestId3),
#if !BUILDFLAG(IS_IOS)
      SendAccelerator(kTestId4, Accelerator()),
#endif
      Confirm(kTestId1));

  EXPECT_THAT(simulator()->records(),
              testing::ElementsAre(
                  ActionRecord{ActionType::kPressButton, kTestId1,
                               kTestContext1, InputType::kDontCare},
                  ActionRecord{ActionType::kSelectMenuItem, kTestId2,
                               kTestContext1, InputType::kKeyboard},
                  ActionRecord{ActionType::kDoDefaultAction, kTestId3,
                               kTestContext1, InputType::kMouse},
                  ActionRecord{ActionType::kSelectTab, kTestId4, kTestContext1,
                               InputType::kTouch},
                  ActionRecord{ActionType::kSelectDropdownItem, kTestId1,
                               kTestContext1, InputType::kDontCare},
                  ActionRecord{ActionType::kEnterText, kTestId2, kTestContext1,
                               InputType::kKeyboard},
                  ActionRecord{ActionType::kActivateSurface, kTestId3,
                               kTestContext1, InputType::kMouse},
#if !BUILDFLAG(IS_IOS)
                  ActionRecord{ActionType::kSendAccelerator, kTestId4,
                               kTestContext1, InputType::kKeyboard},
#endif
                  ActionRecord{ActionType::kConfirm, kTestId1, kTestContext1,
                               InputType::kDontCare}));
}

TEST_F(InteractiveTestTest, InteractionVerbsInAnyContext) {
  TestElement e1(kTestId1, kTestContext1);
  TestElement e2(kTestId2, kTestContext1);
  TestElement e3(kTestId3, kTestContext1);
  TestElement e4(kTestId4, kTestContext1);
  e1.Show();
  e2.Show();
  e3.Show();
  e4.Show();
  RunTestSequenceInContext(
      kTestContext2, InAnyContext(PressButton(kTestId1, InputType::kDontCare)),
      InAnyContext(SelectMenuItem(kTestId2, InputType::kKeyboard)),
      InAnyContext(DoDefaultAction(kTestId3, InputType::kMouse)),
      InAnyContext(Steps(SelectTab(kTestId4, 3U, InputType::kTouch),
                         SelectDropdownItem(kTestId1, 2U, InputType::kDontCare),
                         EnterText(kTestId2, u"The quick brown fox."),
                         ActivateSurface(kTestId3),
#if !BUILDFLAG(IS_IOS)
                         SendAccelerator(kTestId4, Accelerator()),
#endif
                         Confirm(kTestId1))));

  EXPECT_THAT(simulator()->records(),
              testing::ElementsAre(
                  ActionRecord{ActionType::kPressButton, kTestId1,
                               kTestContext1, InputType::kDontCare},
                  ActionRecord{ActionType::kSelectMenuItem, kTestId2,
                               kTestContext1, InputType::kKeyboard},
                  ActionRecord{ActionType::kDoDefaultAction, kTestId3,
                               kTestContext1, InputType::kMouse},
                  ActionRecord{ActionType::kSelectTab, kTestId4, kTestContext1,
                               InputType::kTouch},
                  ActionRecord{ActionType::kSelectDropdownItem, kTestId1,
                               kTestContext1, InputType::kDontCare},
                  ActionRecord{ActionType::kEnterText, kTestId2, kTestContext1,
                               InputType::kKeyboard},
                  ActionRecord{ActionType::kActivateSurface, kTestId3,
                               kTestContext1, InputType::kMouse},
#if !BUILDFLAG(IS_IOS)
                  ActionRecord{ActionType::kSendAccelerator, kTestId4,
                               kTestContext1, InputType::kKeyboard},
#endif
                  ActionRecord{ActionType::kConfirm, kTestId1, kTestContext1,
                               InputType::kDontCare}));
}

TEST_F(InteractiveTestTest, InteractionVerbsInSameContext) {
  TestElement e1(kTestId1, kTestContext1);
  TestElement e2(kTestId2, kTestContext1);
  TestElement e3(kTestId3, kTestContext1);
  TestElement e4(kTestId4, kTestContext1);
  e1.Show();
  e2.Show();
  e3.Show();
  e4.Show();
  RunTestSequenceInContext(
      kTestContext2, InAnyContext(PressButton(kTestId1, InputType::kDontCare)),
      InSameContext(SelectMenuItem(kTestId2, InputType::kKeyboard)),
      InSameContext(DoDefaultAction(kTestId3, InputType::kMouse)),
      InSameContext(
          Steps(SelectTab(kTestId4, 3U, InputType::kTouch),
                SelectDropdownItem(kTestId1, 2U, InputType::kDontCare),
                EnterText(kTestId2, u"The quick brown fox."),
                ActivateSurface(kTestId3),
#if !BUILDFLAG(IS_IOS)
                SendAccelerator(kTestId4, Accelerator()),
#endif
                Confirm(kTestId1))));

  EXPECT_THAT(simulator()->records(),
              testing::ElementsAre(
                  ActionRecord{ActionType::kPressButton, kTestId1,
                               kTestContext1, InputType::kDontCare},
                  ActionRecord{ActionType::kSelectMenuItem, kTestId2,
                               kTestContext1, InputType::kKeyboard},
                  ActionRecord{ActionType::kDoDefaultAction, kTestId3,
                               kTestContext1, InputType::kMouse},
                  ActionRecord{ActionType::kSelectTab, kTestId4, kTestContext1,
                               InputType::kTouch},
                  ActionRecord{ActionType::kSelectDropdownItem, kTestId1,
                               kTestContext1, InputType::kDontCare},
                  ActionRecord{ActionType::kEnterText, kTestId2, kTestContext1,
                               InputType::kKeyboard},
                  ActionRecord{ActionType::kActivateSurface, kTestId3,
                               kTestContext1, InputType::kMouse},
#if !BUILDFLAG(IS_IOS)
                  ActionRecord{ActionType::kSendAccelerator, kTestId4,
                               kTestContext1, InputType::kKeyboard},
#endif
                  ActionRecord{ActionType::kConfirm, kTestId1, kTestContext1,
                               InputType::kDontCare}));
}

TEST_F(InteractiveTestTest, Do) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, f);
  EXPECT_CALL_IN_SCOPE(f, Run,
                       RunTestSequenceInContext(kTestContext1, Do(f.Get())));
}

TEST_F(InteractiveTestTest, Check) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<bool()>, check);

  EXPECT_CALL(check, Run).WillOnce([]() { return true; });
  RunTestSequenceInContext(kTestContext1, Check(check.Get()));
}

TEST_F(InteractiveTestTest, CheckFails) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<bool()>, check);

  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  EXPECT_CALL(check, Run).WillOnce([]() { return false; });
  EXPECT_CALL_IN_SCOPE(aborted, Run, {
    EXPECT_FALSE(RunTestSequenceInContext(kTestContext1, Check(check.Get())));
  });
}

TEST_F(InteractiveTestTest, CheckResult) {
  UNCALLED_MOCK_CALLBACK(base::RepeatingCallback<int()>, f);
  EXPECT_CALL(f, Run).WillOnce([]() { return 2; }).WillOnce([]() { return 3; });

  UNCALLED_MOCK_CALLBACK(base::OnceCallback<std::string()>, f2);
  const char kString[] = "a string";
  EXPECT_CALL(f2, Run).WillOnce([=]() { return std::string(kString); });

  RunTestSequenceInContext(kTestContext1, CheckResult(f.Get(), 2),
                           CheckResult(f.Get(), testing::Gt(2)),
                           CheckResult(f2.Get(), kString));
}

TEST_F(InteractiveTestTest, CheckResultFails) {
  UNCALLED_MOCK_CALLBACK(base::RepeatingCallback<int()>, f);
  EXPECT_CALL(f, Run).WillOnce([]() { return 2; });

  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  EXPECT_CALL_IN_SCOPE(aborted, Run, {
    EXPECT_FALSE(
        RunTestSequenceInContext(kTestContext1, CheckResult(f.Get(), 3)));
  });
}

TEST_F(InteractiveTestTest, CheckElement) {
  TestElement e1(kTestId1, kTestContext1);
  TestElement e2(kTestId2, kTestContext1);

  UNCALLED_MOCK_CALLBACK(base::RepeatingCallback<bool(TrackedElement * el)>,
                         cb1);
  EXPECT_CALL(cb1, Run).WillRepeatedly(
      [&e1](TrackedElement* el) { return el == &e1; });

  UNCALLED_MOCK_CALLBACK(base::OnceCallback<bool(TrackedElement * el)>, cb2);
  EXPECT_CALL(cb2, Run).WillOnce(
      [&e2](TrackedElement* el) { return el == &e2; });

  e1.Show();
  e2.Show();

  RunTestSequenceInContext(kTestContext1, CheckElement(kTestId1, cb1.Get()),
                           CheckElement(kTestId2, cb2.Get()),
                           CheckElement(kTestId1, cb1.Get()));
}

TEST_F(InteractiveTestTest, CheckElementFails) {
  TestElement e1(kTestId1, kTestContext1);

  UNCALLED_MOCK_CALLBACK(base::RepeatingCallback<bool(TrackedElement * el)>,
                         cb1);
  EXPECT_CALL(cb1, Run).WillRepeatedly(
      [&e1](TrackedElement* el) { return el != &e1; });

  e1.Show();

  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  EXPECT_CALL_IN_SCOPE(aborted, Run, {
    EXPECT_FALSE(RunTestSequenceInContext(kTestContext1,
                                          CheckElement(kTestId1, cb1.Get())));
  });
}

TEST_F(InteractiveTestTest, CheckElementWithMatcher) {
  TestElement e1(kTestId1, kTestContext1);
  TestElement e2(kTestId2, kTestContext1);

  UNCALLED_MOCK_CALLBACK(
      base::RepeatingCallback<TrackedElement*(TrackedElement * el)>, cb1);
  EXPECT_CALL(cb1, Run).WillRepeatedly([](TrackedElement* el) { return el; });

  UNCALLED_MOCK_CALLBACK(base::OnceCallback<int(TrackedElement * el)>, cb2);
  EXPECT_CALL(cb2, Run).WillOnce(
      [&e1](TrackedElement* el) { return el == &e1 ? 1 : 2; });

  e1.Show();
  e2.Show();

  RunTestSequenceInContext(kTestContext1,
                           // Implicitly create testing::Eq from value.
                           CheckElement(kTestId1, cb1.Get(), &e1),
                           // Test explicit matchers.
                           CheckElement(kTestId2, cb2.Get(), testing::Gt(1)),
                           CheckElement(kTestId2, cb1.Get(), testing::Ne(&e1)));
}

TEST_F(InteractiveTestTest, CheckElementWithMatcherFails) {
  TestElement e1(kTestId1, kTestContext1);

  UNCALLED_MOCK_CALLBACK(base::RepeatingCallback<int(TrackedElement * el)>,
                         cb1);
  EXPECT_CALL(cb1, Run).WillRepeatedly(
      [&e1](TrackedElement* el) { return el == &e1 ? 1 : 2; });

  e1.Show();

  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  EXPECT_CALL_IN_SCOPE(aborted, Run, {
    EXPECT_FALSE(RunTestSequenceInContext(
        kTestContext1, CheckElement(kTestId1, cb1.Get(), 2)));
  });
}

TEST_F(InteractiveTestTest, After) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, cb1);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, cb2);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, cb3);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, cb4);
  TestElement el(kTestId1, kTestContext1);

  QueueActions(base::BindLambdaForTesting([&]() {
    EXPECT_CALL_IN_SCOPE(cb1, Run, el.Show());
    EXPECT_CALL_IN_SCOPE(cb2, Run, el.Activate());
    el.SendCustomEvent(kTestEvent1);
    EXPECT_CALL_IN_SCOPE(cb3, Run, el.SendCustomEvent(kTestEvent2));
    EXPECT_CALL_IN_SCOPE(cb4, Run, el.Hide());
  }));

  RunTestSequenceInContext(kTestContext1, AfterShow(kTestId1, cb1.Get()),
                           AfterActivate(kTestId1, cb2.Get()),
                           AfterEvent(kTestId1, kTestEvent2, cb3.Get()),
                           AfterHide(kTestId1, cb4.Get()));
}

TEST_F(InteractiveTestTest, WaitFor) {
  TestElement e1(kTestId1, kTestContext1);
  TestElement e2(kTestId2, kTestContext1);

  QueueActions(base::BindLambdaForTesting([&]() {
    // Already in step 1, this triggers step 2.
    e2.Show();
    // Transition to step 3.
    e1.Activate();
    // Hide before moving to step 4.
    e1.Hide();
    // This should transition both 4 and 5.
    e2.SendCustomEvent(kTestEvent1);
    // This should transition step 6.
    e2.Hide();
  }));

  e1.Show();

  RunTestSequenceInContext(
      kTestContext1, WaitForShow(kTestId1),
      WaitForShow(kTestId2, /* transition_only_on_event =*/true),
      WaitForActivate(kTestId1), WaitForEvent(kTestId2, kTestEvent1),
      WaitForHide(kTestId1),
      WaitForHide(kTestId2, /* transition_only_on_event =*/true));
}

TEST_F(InteractiveTestTest, PresentOrNotPresentInAnyContext) {
  TestElement e1(kTestId1, kTestContext1);
  TestElement e2(kTestId2, kTestContext2);
  e1.Show();
  e2.Show();

  RunTestSequenceInContext(
      kTestContext1, EnsurePresent(kTestId1),
      // Not present in the current context.
      EnsureNotPresent(kTestId2),
      EnsureNotPresent(kTestId3, /* in_any_context = */ true),
      EnsurePresent(kTestId2, /* in_any_context = */ true));
}

TEST_F(InteractiveTestTest, WithElementFails) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, callback);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  EXPECT_CALL_IN_SCOPE(aborted, Run, {
    EXPECT_FALSE(RunTestSequenceInContext(
        kTestContext1, WithElement(kTestId1, callback.Get())));
  });
}

TEST_F(InteractiveTestTest, EnsureNotPresentFails) {
  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  EXPECT_CALL_IN_SCOPE(aborted, Run, {
    EXPECT_FALSE(
        RunTestSequenceInContext(kTestContext1, EnsureNotPresent(kTestId1)));
  });
}

TEST_F(InteractiveTestTest, EnsureNotPresentInAnyContextFails) {
  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  EXPECT_CALL_IN_SCOPE(aborted, Run, {
    EXPECT_FALSE(RunTestSequenceInContext(
        kTestContext2,
        EnsureNotPresent(kTestId1, /* in_any_context = */ true)));
  });
}

TEST_F(InteractiveTestTest, NamedElement) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<void(TrackedElement*)>, cb);

  TestElement e1(kTestId1, kTestContext1);
  TestElement e2(kTestId2, kTestContext2);
  e1.Show();
  e2.Show();
  constexpr char kName[] = "name";

  EXPECT_CALL_IN_SCOPE(
      cb, Run(&e2),
      RunTestSequenceInContext(
          kTestContext1,
          WithElement(kTestId1,
                      base::BindLambdaForTesting(
                          [&](InteractionSequence* seq, TrackedElement*) {
                            seq->NameElement(&e2, kName);
                          })),
          WithElement(kName, cb.Get())));
}

TEST_F(InteractiveTestTest, SimulatorSucceeds_SkipOnUnsupported) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  private_test_impl().set_aborted_callback_for_testing(aborted.Get());
  RunTestSequenceInContext(
      kTestContext1,
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kSetOnIncompatibleActionMessage),
      PressButton(kTestId1));
  EXPECT_FALSE(private_test_impl().sequence_skipped());
}

TEST_F(InteractiveTestTest, SimulatorSucceeds_IgnoreOnUnsupported) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  private_test_impl().set_aborted_callback_for_testing(aborted.Get());
  RunTestSequenceInContext(
      kTestContext1,
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kSetOnIncompatibleActionMessage),
      PressButton(kTestId1));
  EXPECT_FALSE(private_test_impl().sequence_skipped());
}

TEST_F(InteractiveTestTest, SimulatorFailureFails) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  private_test_impl().set_aborted_callback_for_testing(aborted.Get());
  simulator_->set_result(ActionResult::kFailed);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequenceInContext(kTestContext1, PressButton(kTestId1)));
  EXPECT_FALSE(private_test_impl().sequence_skipped());
}

TEST_F(InteractiveTestTest, SimulatorFailureFails_SkipOnUnsupported) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  private_test_impl().set_aborted_callback_for_testing(aborted.Get());
  simulator_->set_result(ActionResult::kFailed);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequenceInContext(
          kTestContext1,
          SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                                  kSetOnIncompatibleActionMessage),
          PressButton(kTestId1)));
  EXPECT_FALSE(private_test_impl().sequence_skipped());
}

TEST_F(InteractiveTestTest, SimulatorFailureFails_IgnoreOnUnsupported) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  private_test_impl().set_aborted_callback_for_testing(aborted.Get());
  simulator_->set_result(ActionResult::kFailed);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequenceInContext(
          kTestContext1,
          SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                                  kSetOnIncompatibleActionMessage),
          PressButton(kTestId1)));
  EXPECT_FALSE(private_test_impl().sequence_skipped());
}

TEST_F(InteractiveTestTest, SimulatorCannotSimulateFails) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  private_test_impl().set_aborted_callback_for_testing(aborted.Get());
  simulator_->set_result(ActionResult::kNotAttempted);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequenceInContext(kTestContext1, PressButton(kTestId1)));
  EXPECT_FALSE(private_test_impl().sequence_skipped());
}

TEST_F(InteractiveTestTest, SimulatorCannotSimulateFails_SkipOnUnsupported) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  private_test_impl().set_aborted_callback_for_testing(aborted.Get());
  simulator_->set_result(ActionResult::kNotAttempted);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequenceInContext(
          kTestContext1,
          SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                                  kSetOnIncompatibleActionMessage),
          PressButton(kTestId1)));
  EXPECT_FALSE(private_test_impl().sequence_skipped());
}

TEST_F(InteractiveTestTest, SimulatorCannotSimulateFails_IgnoreOnUnsupported) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  private_test_impl().set_aborted_callback_for_testing(aborted.Get());
  simulator_->set_result(ActionResult::kNotAttempted);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequenceInContext(
          kTestContext1,
          SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                                  kSetOnIncompatibleActionMessage),
          PressButton(kTestId1)));
  EXPECT_FALSE(private_test_impl().sequence_skipped());
}

TEST_F(InteractiveTestTest, SimulatorNotSupportedFails) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  private_test_impl().set_aborted_callback_for_testing(aborted.Get());
  simulator_->set_result(ActionResult::kKnownIncompatible);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequenceInContext(kTestContext1, PressButton(kTestId1)));
  EXPECT_FALSE(private_test_impl().sequence_skipped());
}

TEST_F(InteractiveTestTest, SimulatorNotSupportedSkipsOnUnsupported) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  private_test_impl().set_aborted_callback_for_testing(aborted.Get());
  simulator_->set_result(ActionResult::kKnownIncompatible);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequenceInContext(
          kTestContext1,
          SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                                  kSetOnIncompatibleActionMessage),
          PressButton(kTestId1)));
  EXPECT_TRUE(private_test_impl().sequence_skipped());
}

TEST_F(InteractiveTestTest, SimulatorNotSupportedContinuesOnUnsupported) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  bool result = false;

  private_test_impl().set_aborted_callback_for_testing(aborted.Get());
  simulator_->set_result(ActionResult::kKnownIncompatible);
  RunTestSequenceInContext(
      kTestContext1,
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kSetOnIncompatibleActionMessage),
      PressButton(kTestId1),
      Do(base::BindLambdaForTesting([&result]() { result = true; })));
  EXPECT_TRUE(result);
}

TEST_F(InteractiveTestTest, CanChangeOnIncompatibleAction) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  private_test_impl().set_aborted_callback_for_testing(aborted.Get());
  simulator_->set_result(ActionResult::kKnownIncompatible);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequenceInContext(
          kTestContext1,
          // Based on previous tests, this will fall through to the next step.
          SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                                  kSetOnIncompatibleActionMessage),
          PressButton(kTestId1),
          // By changing the incompatible mode, the step after this one should
          // fail.
          SetOnIncompatibleAction(OnIncompatibleAction::kFailTest, ""),
          PressButton(kTestId1)));
}

TEST_F(InteractiveTestTest, SimulatorNotSupportedHaltAndSucceedOnUnsupported) {
  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  bool result = false;

  simulator_->set_result(ActionResult::kKnownIncompatible);
  RunTestSequenceInContext(
      kTestContext1,
      SetOnIncompatibleAction(OnIncompatibleAction::kHaltTest,
                              kSetOnIncompatibleActionMessage),
      PressButton(kTestId1),
      Do(base::BindLambdaForTesting([&result]() { result = true; })));
  EXPECT_FALSE(result);
}

TEST_F(InteractiveTestTest, ActuallySkipsTestOnSimulatorFailure) {
  TestElement e1(kTestId1, kTestContext1);
  e1.Show();
  simulator_->set_result(ActionResult::kKnownIncompatible);
  RunTestSequenceInContext(
      kTestContext1,
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kSetOnIncompatibleActionMessage),
      PressButton(kTestId1));

  // Note: this test will either be marked as skipped or failed, but never
  // succeeded. The important thing is that it does not fail.
  if (!testing::Test::IsSkipped()) {
    GTEST_FAIL();
  }
}

TEST_F(InteractiveTestTest, IfTrue) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<bool(void)>, condition);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step);

  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  EXPECT_CALL(condition, Run).WillOnce(testing::Return(true));
  EXPECT_CALL(step, Run);
  RunTestSequenceInContext(e1.context(), If(condition.Get(), Do(step.Get())));
}

TEST_F(InteractiveTestTest, IfFalse) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<bool(void)>, condition);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step);

  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  EXPECT_CALL(condition, Run).WillOnce(testing::Return(false));
  RunTestSequenceInContext(e1.context(), If(condition.Get(), Do(step.Get())));
}

TEST_F(InteractiveTestTest, IfMatcherTrue) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<int(void)>, condition);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step);

  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  EXPECT_CALL(condition, Run).WillOnce(testing::Return(1));
  EXPECT_CALL(step, Run);
  RunTestSequenceInContext(
      e1.context(), IfMatches(condition.Get(), testing::Eq(1), Do(step.Get())));
}

TEST_F(InteractiveTestTest, IfMatcherFalse) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<int(void)>, condition);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step);

  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  EXPECT_CALL(condition, Run).WillOnce(testing::Return(0));
  RunTestSequenceInContext(
      e1.context(), IfMatches(condition.Get(), testing::Eq(1), Do(step.Get())));
}

TEST_F(InteractiveTestTest, IfImplicitMatcherTrue) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<int(void)>, condition);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step);

  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  EXPECT_CALL(condition, Run).WillOnce(testing::Return(1));
  EXPECT_CALL(step, Run);
  RunTestSequenceInContext(e1.context(),
                           IfMatches(condition.Get(), 1, Do(step.Get())));
}

TEST_F(InteractiveTestTest, IfImplicitMatcherFalse) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<int(void)>, condition);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step);

  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  EXPECT_CALL(condition, Run).WillOnce(testing::Return(0));
  RunTestSequenceInContext(e1.context(),
                           IfMatches(condition.Get(), 1, Do(step.Get())));
}

TEST_F(InteractiveTestTest, IfWithMultiStep) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<bool(void)>, condition);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step1);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step2);

  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  EXPECT_CALL(condition, Run).WillOnce(testing::Return(true));
  EXPECT_CALL(step1, Run);
  EXPECT_CALL(step2, Run);
  RunTestSequenceInContext(
      e1.context(),
      If(condition.Get(), Steps(Do(step1.Get()), Do(step2.Get()))));
}

TEST_F(InteractiveTestTest, IfElementTrue) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<bool(const TrackedElement*)>,
                         condition);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step);

  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  EXPECT_CALL(condition, Run(&e1)).WillOnce(testing::Return(true));
  EXPECT_CALL(step, Run);
  RunTestSequenceInContext(
      e1.context(),
      IfElement(e1.identifier(), condition.Get(), Do(step.Get())));
}

TEST_F(InteractiveTestTest, IfElementFalse) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<bool(const TrackedElement*)>,
                         condition);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step);

  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  EXPECT_CALL(condition, Run(&e1)).WillOnce(testing::Return(false));
  RunTestSequenceInContext(
      e1.context(),
      IfElement(e1.identifier(), condition.Get(), Do(step.Get())));
}

TEST_F(InteractiveTestTest, IfElementMatchesTrue) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<std::string(const TrackedElement*)>,
                         condition);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step);

  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  EXPECT_CALL(condition, Run(&e1))
      .WillOnce(testing::Return(std::string("foo")));
  EXPECT_CALL(step, Run);
  RunTestSequenceInContext(
      e1.context(), IfElementMatches(e1.identifier(), condition.Get(), "foo",
                                     Do(step.Get())));
}

TEST_F(InteractiveTestTest, IfElementMatchesFalse) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<std::string(const TrackedElement*)>,
                         condition);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step);

  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  EXPECT_CALL(condition, Run(&e1))
      .WillOnce(testing::Return(std::string("bar")));
  RunTestSequenceInContext(
      e1.context(), IfElementMatches(e1.identifier(), condition.Get(), "foo",
                                     Do(step.Get())));
}

TEST_F(InteractiveTestTest, IfElementWithMultiStep) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<bool(const TrackedElement*)>,
                         condition);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step1);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step2);

  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  EXPECT_CALL(condition, Run(&e1)).WillOnce(testing::Return(true));
  EXPECT_CALL(step1, Run);
  EXPECT_CALL(step2, Run);
  RunTestSequenceInContext(e1.context(),
                           IfElement(e1.identifier(), condition.Get(),
                                     Steps(Do(step1.Get()), Do(step2.Get()))));
}

TEST_F(InteractiveTestTest, IfFails) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<bool(void)>, condition);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);

  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  EXPECT_CALL(condition, Run).WillOnce(testing::Return(true));
  EXPECT_CALL(aborted, Run);
  RunTestSequenceInContext(
      e1.context(),
      If(condition.Get(), Check(base::BindOnce([]() { return false; }))));
}

TEST_F(InteractiveTestTest, IfThenElse_OnlyRunsThen) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<bool(void)>, condition);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, a);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, b);

  EXPECT_CALL(condition, Run).WillOnce(testing::Return(true));
  EXPECT_CALL(a, Run);
  RunTestSequenceInContext(kTestContext1,
                           If(condition.Get(), Do(a.Get()), Do(b.Get())));
}

TEST_F(InteractiveTestTest, IfThenElse_OnlyRunsElse) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<bool(void)>, condition);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, a);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, b);

  EXPECT_CALL(condition, Run).WillOnce(testing::Return(false));
  EXPECT_CALL(b, Run);
  RunTestSequenceInContext(kTestContext1,
                           If(condition.Get(), Do(a.Get()), Do(b.Get())));
}

TEST_F(InteractiveTestTest, IfThenElse_ThenFails) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<bool(void)>, condition);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);

  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  EXPECT_CALL(condition, Run).WillOnce(testing::Return(true));
  EXPECT_CALL(aborted, Run);
  RunTestSequenceInContext(
      kTestContext1,
      If(condition.Get(), Check(base::BindOnce([]() { return false; })),
         Do(base::BindOnce([]() {}))));
}

TEST_F(InteractiveTestTest, IfThenElse_ElseFails) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<bool(void)>, condition);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);

  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  EXPECT_CALL(condition, Run).WillOnce(testing::Return(false));
  EXPECT_CALL(aborted, Run);
  RunTestSequenceInContext(kTestContext1,
                           If(condition.Get(), Do(base::BindOnce([]() {}))),
                           Check(base::BindOnce([]() { return false; })));
}

TEST_F(InteractiveTestTest, InParallel) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, seq1);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, seq2);

  EXPECT_CALL(seq1, Run);
  EXPECT_CALL(seq2, Run);
  RunTestSequenceInContext(kTestContext1,
                           InParallel(Do(seq1.Get()), Do(seq2.Get())));
}

TEST_F(InteractiveTestTest, InParallelMultiStep) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, seq11);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, seq12);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, seq21);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, seq22);

  EXPECT_CALL(seq11, Run);
  EXPECT_CALL(seq12, Run);
  EXPECT_CALL(seq21, Run);
  EXPECT_CALL(seq22, Run);
  RunTestSequenceInContext(kTestContext1,
                           InParallel(Steps(Do(seq11.Get()), Do(seq12.Get())),
                                      Steps(Do(seq21.Get()), Do(seq22.Get()))));
}

TEST_F(InteractiveTestTest, InParallelAsync) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<void(ui::TrackedElement*)>, seq1);
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<void(ui::TrackedElement*)>, seq2);

  TestElement e1(kTestId1, kTestContext1);
  TestElement e2(kTestId2, kTestContext1);

  QueueActions(base::BindLambdaForTesting([&e1]() { e1.Show(); }));
  QueueActions(base::BindLambdaForTesting([&e2]() { e2.Show(); }));
  EXPECT_CALL(seq1, Run(&e1));
  EXPECT_CALL(seq2, Run(&e2));
  RunTestSequenceInContext(kTestContext1,
                           InParallel(AfterShow(e1.identifier(), seq1.Get()),
                                      AfterShow(e2.identifier(), seq2.Get())));
}

// Parallel sequences where one sequence triggers a step in another.
TEST_F(InteractiveTestTest, InParallelDependent) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<void(ui::TrackedElement*)>, seq1);
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<void(ui::TrackedElement*)>, seq2);

  TestElement e1(kTestId1, kTestContext1);
  TestElement e2(kTestId2, kTestContext1);

  QueueActions(base::BindLambdaForTesting([&e1]() { e1.Show(); }));
  EXPECT_CALL(seq1, Run(&e1)).WillOnce([&e2](TrackedElement*) { e2.Show(); });
  EXPECT_CALL(seq2, Run(&e2));
  RunTestSequenceInContext(kTestContext1,
                           InParallel(AfterShow(e1.identifier(), seq1.Get()),
                                      AfterShow(e2.identifier(), seq2.Get())));
}

// Parallel sequences where one sequence triggers a step in another, which then
// triggers the final step in the first subsequence.
TEST_F(InteractiveTestTest, InParallelPingPong) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<void(ui::TrackedElement*)>, seq1);
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<void(ui::TrackedElement*)>, seq2);
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<void(ui::TrackedElement*)>, seq3);

  TestElement e1(kTestId1, kTestContext1);
  TestElement e2(kTestId2, kTestContext1);

  QueueActions(base::BindLambdaForTesting([&e1]() { e1.Show(); }));
  EXPECT_CALL(seq1, Run(&e1)).WillOnce([&e2](TrackedElement*) { e2.Show(); });
  EXPECT_CALL(seq2, Run(&e2)).WillOnce([&e1](TrackedElement*) {
    e1.SendCustomEvent(kTestEvent1);
  });
  EXPECT_CALL(seq3, Run(&e1));
  RunTestSequenceInContext(
      kTestContext1,
      InParallel(Steps(AfterShow(e1.identifier(), seq1.Get()),
                       AfterEvent(e1.identifier(), kTestEvent1, seq3.Get())),
                 AfterShow(e2.identifier(), seq2.Get())));
}

TEST_F(InteractiveTestTest, InParallelFails) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);

  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  EXPECT_CALL(aborted, Run);
  RunTestSequenceInContext(
      e1.context(), InParallel(Do(base::DoNothing()),
                               Check(base::BindOnce([]() { return false; }))));
}

TEST_F(InteractiveTestTest, AnyOf) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, seq1);

  EXPECT_CALL(seq1, Run).Times(1);
  RunTestSequenceInContext(kTestContext1,
                           AnyOf(Do(seq1.Get()), Do(seq1.Get())));
}

TEST_F(InteractiveTestTest, AnyOfOneFailsOneSucceeds) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, seq1);

  EXPECT_CALL(seq1, Run).Times(1);
  RunTestSequenceInContext(kTestContext1,
                           AnyOf(Check(base::BindOnce([]() { return false; })),
                                 Do(seq1.Get()), Do(seq1.Get())));
}

TEST_F(InteractiveTestTest, AnyOfAllFail) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);

  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  EXPECT_CALL(aborted, Run);
  RunTestSequenceInContext(
      e1.context(), InParallel(Check(base::BindOnce([]() { return false; })),
                               Check(base::BindOnce([]() { return false; }))));
}

// This test that various types of logging can compile with different types of
// parameters. The output of this test must be verified manually.
TEST_F(InteractiveTestTest, Log) {
  TestElement e1(kTestId1, kTestContext1);
  int x = 1;
  int y = 0;
  constexpr char kSomeString[] = "A string.";
  std::u16string deferred_string1;
  const char* deferred_string2;
  struct {
    bool b = false;
  } unnamed_struct, *unnamed_struct_ptr = nullptr;

  RunTestSequenceInContext(
      e1.context(), Do(base::BindLambdaForTesting([&]() {
        y = 2;
        deferred_string1 = u"The quick brown fox";
        deferred_string2 = "Lorem ipsum";
        unnamed_struct_ptr = &unnamed_struct;
      })),
      Log(
          "Log() output follows:\nliteral int: ", x,
          "\ndeferred int: ", std::ref(y), "\nconstexpr string: ", kSomeString,
          "\ndeferred string 1: ", std::ref(deferred_string1),
          "\ndeferred string 2: ", std::ref(deferred_string2),
          "\npointer to object: ", &unnamed_struct,
          "\ndeferred pointer to object: ", std::ref(unnamed_struct_ptr),
          "\nlambda - should be 7: ", [x, &y]() { return x + y + 4; },
          "\nOnceCallback - should be 3: ",
          base::BindOnce([](int x, int* y) { return x + *y; }, x,
                         base::Unretained(&y)),
          "\nRepeatingCallback - should be 4: ",
          base::BindRepeating([](int x, int* y) { return x + *y + 1; }, x,
                              base::Unretained(&y)),
          "\nfunction pointer - should be 5: ", &ValueGeneratingFunction));
}

// This test ensures that binding of various types of functions and function
// arguments works correctly with actions. If the template logic is not correct,
// this test will likely not compile.
TEST_F(InteractiveTestTest, ActionBindingMethods) {
  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  CallableObject callable{2};
  MutableCallableObject mutable_callable{0};
  EmptyCallableObject empty_callable;
  auto lambda = []() { LOG(INFO) << "Stored lambda."; };
  auto once_callback = base::BindOnce([]() { LOG(INFO) << "Once callback."; });
  auto repeating_callback =
      base::BindRepeating([]() { LOG(INFO) << "Repeating callback."; });
  int x = 1;
  int y = 2;
  RunTestSequenceInContext(
      e1.context(),

      // Check all of the various ways to bind methods with Do().
      Do(base::DoNothing()), Do(&DoFunction), Do(lambda),
      Do(std::move(once_callback)), Do(repeating_callback),
      Do([x, &y]() { LOG(INFO) << "Bound args " << x << ", " << y; }),
      Do(empty_callable),

      // Check various ways to verify a return value.
      Check(base::BindOnce([]() { return true; })),
      Check([]() { return true; }), CheckResult([x, &y]() { return x + y; }, 3),
      Check(callable), CheckResult(std::move(mutable_callable), false),
      CheckElement(
          e1.identifier(), [](TrackedElement* el) { return el; }, &e1),

      // Verify that argument list reduction works with bare callbacks.
      AfterShow(e1.identifier(),
                [&e1](TrackedElement* el) { EXPECT_EQ(el, &e1); }),
      WithElement(e1.identifier(), []() {}),
      WithElement(e1.identifier(),
                  [](InteractionSequence*, TrackedElement*) {}));
}

// This test ensures that binding of various types of functions and function
// arguments works correctly with conditionals. If the template logic is not
// correct, this test will likely not compile.
TEST_F(InteractiveTestTest, ConditionalBindingMethods) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, correct);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, incorrect);
  TestElement e1(kTestId1, kTestContext1);
  e1.Show();

  int x = 1;
  int y = 2;

  EXPECT_CALL(correct, Run).Times(4);
  RunTestSequenceInContext(
      e1.context(),
      If([]() { return true; }, Do(correct.Get()), Do(incorrect.Get())),
      IfMatches([x, &y]() { return x + y; }, 2, Do(incorrect.Get()),
                Do(correct.Get())),
      IfElement(
          e1.identifier(),
          [&e1](const TrackedElement* el) { return el == &e1; },
          Do(correct.Get()), Do(incorrect.Get())),
      IfElementMatches(kTestId2, &CheckElementFunction, testing::Ne(nullptr),
                       Do(incorrect.Get()), Do(correct.Get())));
}

}  // namespace ui::test
