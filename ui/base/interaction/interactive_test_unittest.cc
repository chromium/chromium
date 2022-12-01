// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interactive_test.h"

#include "base/functional/callback_forward.h"
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

class TestSimulator : public InteractionTestUtil::Simulator {
 public:
  TestSimulator() = default;
  ~TestSimulator() override = default;

  bool PressButton(TrackedElement* element, InputType input_type) override {
    DoAction(ActionType::kPressButton, element, input_type);
    return true;
  }

  bool SelectMenuItem(TrackedElement* element, InputType input_type) override {
    DoAction(ActionType::kSelectMenuItem, element, input_type);
    return true;
  }

  bool DoDefaultAction(TrackedElement* element, InputType input_type) override {
    DoAction(ActionType::kDoDefaultAction, element, input_type);
    return true;
  }

  bool SelectTab(TrackedElement* tab_collection,
                 size_t index,
                 InputType input_type) override {
    DoAction(ActionType::kSelectTab, tab_collection, input_type);
    return true;
  }

  bool SelectDropdownItem(TrackedElement* collection,
                          size_t item,
                          InputType input_type) override {
    DoAction(ActionType::kSelectDropdownItem, collection, input_type);
    return true;
  }

  bool EnterText(TrackedElement* element,
                 const std::u16string& text,
                 TextEntryMode mode) override {
    DoAction(ActionType::kEnterText, element, InputType::kKeyboard);
    return true;
  }

  bool ActivateSurface(TrackedElement* element) override {
    DoAction(ActionType::kActivateSurface, element, InputType::kMouse);
    return true;
  }

#if !BUILDFLAG(IS_IOS)
  bool SendAccelerator(TrackedElement* element,
                       const Accelerator& accel) override {
    DoAction(ActionType::kSendAccelerator, element, InputType::kKeyboard);
    return true;
  }
#endif

  bool Confirm(TrackedElement* element) override {
    DoAction(ActionType::kConfirm, element, InputType::kDontCare);
    return true;
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

  std::vector<ActionRecord> records_;
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

 private:
  base::raw_ptr<TestSimulator> simulator_ = nullptr;

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
  UNCALLED_MOCK_CALLBACK(CheckCallback, check);

  EXPECT_CALL(check, Run).WillOnce([]() { return true; });
  RunTestSequenceInContext(kTestContext1, Check(check.Get()));
}

TEST_F(InteractiveTestTest, CheckFails) {
  UNCALLED_MOCK_CALLBACK(CheckCallback, check);

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

}  // namespace ui::test
