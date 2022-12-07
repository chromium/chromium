// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_INTERACTIVE_TEST_H_
#define UI_BASE_INTERACTION_INTERACTIVE_TEST_H_

#include <memory>
#include <sstream>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/rectify_callback.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/interaction/interactive_test_internal.h"

#if !BUILDFLAG(IS_IOS)
#include "ui/base/accelerators/accelerator.h"
#endif

namespace ui::test {

// Provides basic interactive test functionality.
//
// Interactive tests use InteractionSequence, ElementTracker, and
// InteractionTestUtil to provide a common library of concise test methods. This
// convenience API is nicknamed "Kombucha" (see
// //chrome/test/interaction/README.md for more information).
//
// This class is not a test fixture; your test fixture can inherit from it to
// import all of the test API it provides. You will need to call
// private_test_impl().DoTestSetUp() in your SetUp() method and
// private_test_impl().DoTestTearDown() in your TearDown() method; for this
// reason, we provide a convenience class, InteractiveTest, below, which is pre-
// configured to handle all of this for you.
//
// Also, since this class does not implement input automation for any particular
// framework, you are more likely to want e.g. InteractiveViewsTest[Api] or
// InteractiveBrowserTest[Api], which inherit from this class.
class InteractiveTestApi {
 public:
  explicit InteractiveTestApi(
      std::unique_ptr<internal::InteractiveTestPrivate> private_test_impl);
  virtual ~InteractiveTestApi();
  InteractiveTestApi(const InteractiveTestApi&) = delete;
  void operator=(const InteractiveTestApi&) = delete;

 protected:
  using InputType = InteractionTestUtil::InputType;
  using MultiStep = internal::InteractiveTestPrivate::MultiStep;
  using StepBuilder = InteractionSequence::StepBuilder;
  using TextEntryMode = InteractionTestUtil::TextEntryMode;

  // Construct a MultiStep from one or more StepBuilders and/or MultiSteps.
  template <typename... Args>
  static MultiStep Steps(Args&&... args);

  // Returns an interaction simulator for things like clicking buttons.
  // Generally, prefer to use functions like PressButton() to directly using the
  // InteractionTestUtil.
  InteractionTestUtil& test_util() { return private_test_impl_->test_util(); }

  // Runs a test InteractionSequence in `context` from a series of Steps or
  // StepBuilders with RunSynchronouslyForTesting(). Hooks both the completed
  // and aborted callbacks to ensure completion, and prints an error on failure.
  template <typename... Args>
  bool RunTestSequenceInContext(ElementContext context, Args&&... steps);

  // An ElementSpecifier holds either an ElementIdentifier or a
  // base::StringPiece denoting a named element in the test sequence.
  using ElementSpecifier = internal::ElementSpecifier;

  // Convenience methods for creating interaction steps of type kShown. The
  // resulting step's start callback is already set; therefore, do not try to
  // add additional logic. However, any other parameter on the step may be set,
  // such as SetMustBeVisibleAtStart(), SetTransitionOnlyOnEvent(),
  // SetContext(), etc.
  //
  // TODO(dfried): in the future, these will be supplanted/supplemented by more
  // flexible primitives that allow multiple actions in the same step in the
  // future.
  [[nodiscard]] StepBuilder PressButton(
      ElementSpecifier button,
      InputType input_type = InputType::kDontCare);
  [[nodiscard]] StepBuilder SelectMenuItem(
      ElementSpecifier menu_item,
      InputType input_type = InputType::kDontCare);
  [[nodiscard]] StepBuilder DoDefaultAction(
      ElementSpecifier element,
      InputType input_type = InputType::kDontCare);
  [[nodiscard]] StepBuilder SelectTab(
      ElementSpecifier tab_collection,
      size_t tab_index,
      InputType input_type = InputType::kDontCare);
  [[nodiscard]] StepBuilder SelectDropdownItem(
      ElementSpecifier collection,
      size_t item,
      InputType input_type = InputType::kDontCare);
  [[nodiscard]] StepBuilder EnterText(
      ElementSpecifier element,
      std::u16string text,
      TextEntryMode mode = TextEntryMode::kReplaceAll);
  [[nodiscard]] StepBuilder ActivateSurface(ElementSpecifier element);
#if !BUILDFLAG(IS_IOS)
  [[nodiscard]] StepBuilder SendAccelerator(ElementSpecifier element,
                                            Accelerator accelerator);
#endif
  [[nodiscard]] StepBuilder Confirm(ElementSpecifier element);

  // Specifies a test action that is not tied to any one UI element.
  // Returns true on success, false on failure (which will fail the test).
  using CheckCallback = base::OnceCallback<bool()>;

  // Does an action at this point in the test sequence.
  [[nodiscard]] static StepBuilder Do(base::OnceClosure action);

  // Performs a check and fails the test if `check_callback` returns false.
  [[nodiscard]] static StepBuilder Check(CheckCallback check_callback);

  // Calls `function` and applies `matcher` to the result. If the matcher does
  // not match, an appropriate error message is printed and the test fails.
  template <template <typename...> class C, typename T, typename U>
  [[nodiscard]] static StepBuilder CheckResult(C<T()> function, U&& matcher);

  // Checks that `check` returns true for element `element`. will fail the test
  // sequence if `check` returns false - the callback should log any specific
  // error before returning.
  //
  // Note that unless you add .SetMustBeVisibleAtStart(true), this test step
  // will wait for `element` to be shown before proceeding.
  [[nodiscard]] static StepBuilder CheckElement(
      ElementSpecifier element,
      base::OnceCallback<bool(TrackedElement* el)> check);

  // As CheckElement(), but checks that the result of calling `function` on
  // `element` matches `matcher`. If not, the mismatch is printed and the test
  // fails.
  template <template <typename...> class C, typename T, typename U>
  [[nodiscard]] static StepBuilder CheckElement(ElementSpecifier element,
                                                C<T(TrackedElement*)> function,
                                                U&& matcher);

  // Shorthand methods for working with basic ElementTracker events. The element
  // will have `step_callback` called on it. You may specify additional
  // constraints such as SetMustBeVisibleAtStart(), SetTransitionOnlyOnEvent(),
  // SetContext(), etc.
  template <class T>
  [[nodiscard]] static StepBuilder AfterShow(ElementSpecifier element,
                                             T&& step_callback);
  template <class T>
  [[nodiscard]] static StepBuilder AfterActivate(ElementSpecifier element,
                                                 T&& step_callback);
  template <class T>
  [[nodiscard]] static StepBuilder AfterEvent(ElementSpecifier element,
                                              CustomElementEventType event_type,
                                              T&& step_callback);
  template <class T>
  [[nodiscard]] static StepBuilder AfterHide(ElementSpecifier element,
                                             T&& step_callback);

  // Versions of the above that have no step callback; included for clarity and
  // brevity.
  [[nodiscard]] static StepBuilder WaitForShow(
      ElementSpecifier element,
      bool transition_only_on_event = false);
  [[nodiscard]] static StepBuilder WaitForHide(
      ElementSpecifier element,
      bool transition_only_on_event = false);
  [[nodiscard]] static StepBuilder WaitForActivate(ElementSpecifier element);
  [[nodiscard]] static StepBuilder WaitForEvent(ElementSpecifier element,
                                                CustomElementEventType event);

  // Equivalent to AfterShow() but the element must already be present.
  template <class T>
  [[nodiscard]] static StepBuilder WithElement(ElementSpecifier element,
                                               T&& step_callback);

  // Adds steps to the sequence that ensure that `element_to_check` is not
  // present. Flushes the current message queue to ensure that if e.g. the
  // previous step was responding to elements being added, the
  // `element_to_check` may not have had its shown event called yet.
  [[nodiscard]] static MultiStep EnsureNotPresent(
      ElementIdentifier element_to_check,
      bool in_any_context = false);

  // Opposite of EnsureNotPresent. Flushes the current message queue and then
  // checks that the specified element is [still] present. Equivalent to:
  //   FlushEvents(),
  //   WithElement(element_to_check, base::DoNothing())
  //
  // Like EnsureNotPresent(), is not compatible with InAnyContext(); set
  // `in_any_context` to true instead. Otherwise, you can still wrap this call
  // in an InContext() or InSameContext().
  [[nodiscard]] static MultiStep EnsurePresent(
      ElementSpecifier element_to_check,
      bool in_any_context = false);

  // Ensures that the next step does not piggyback on the previous step(s), but
  // rather, executes on a fresh message loop. Normally, steps will continue to
  // trigger on the same call stack until a start condition is not met.
  //
  // Use sparingly, and only when e.g. re-entrancy issues prevent the test from
  // otherwise working properly.
  [[nodiscard]] static MultiStep FlushEvents();

  // Provides syntactic sugar so you can put "in any context" before an action
  // or test step rather than after. For example the following are equivalent:
  //
  //    PressButton(kElementIdentifier)
  //        .SetContext(InteractionSequence::ContextMode::kAny)
  //
  //    InAnyContext(PressButton(kElementIdentifier))
  //
  // Note: does not work with EnsureNotPresent; use the `in_any_context`
  // parameter. Also does not work with all event types (yet).
  //
  // TODO(dfried): consider if we should have a version that takes variadic
  // arguments and applies "in any context" to all of them?
  [[nodiscard]] static MultiStep InAnyContext(MultiStep steps);
  template <typename T>
  [[nodiscard]] static StepBuilder InAnyContext(T&& step);

  // Provides syntactic sugar so you can put "inherit context from previous
  // step" around a step or steps to ensure a sequence executes in a specific
  // context. For example:
  //
  //    InAnyContext(WaitForShow(kMyElementInOtherContext)),
  //    InSameContext(Steps(
  //      PressButton(kMyElementInOtherContext),
  //      WaitForHide(kMyElementInOtherContext)
  //    )),
  [[nodiscard]] static MultiStep InSameContext(MultiStep steps);
  template <typename T>
  [[nodiscard]] static StepBuilder InSameContext(T&& step);

  [[nodiscard]] MultiStep InContext(ElementContext context, MultiStep steps);
  template <typename T>
  [[nodiscard]] StepBuilder InContext(ElementContext context, T&& step);

  // Used internally by methods in this class; do not call.
  internal::InteractiveTestPrivate& private_test_impl() {
    return *private_test_impl_;
  }

 private:
  // Implementation for RunTestSequenceInContext().
  bool RunTestSequenceImpl(ElementContext context,
                           InteractionSequence::Builder builder);

  // Helper method to add a step or steps to a sequence builder.
  static void AddStep(InteractionSequence::Builder& builder, MultiStep steps);
  template <typename T>
  static void AddStep(InteractionSequence::Builder& builder, T&& step);

  static void AddStep(MultiStep& dest, StepBuilder src);
  static void AddStep(MultiStep& dest, MultiStep src);

  std::unique_ptr<internal::InteractiveTestPrivate> private_test_impl_;
};

// A simple test fixture that brings in all of the features of
// InteractiveTestApi. No simulators are attached to test_util() so if you want
// to use verbs like PressButton() you will need to install your own simulator.
//
// Provided for convenience, but generally you will want InteractiveViewsTest
// instead.
class InteractiveTest : public testing::Test, public InteractiveTestApi {
 public:
  InteractiveTest();
  ~InteractiveTest() override;

 protected:
  void SetUp() override;
  void TearDown() override;
};

// Template definitions:

// static
template <typename... Args>
InteractiveTestApi::MultiStep InteractiveTestApi::Steps(Args&&... args) {
  MultiStep result;
  (AddStep(result, std::forward<Args>(args)), ...);
  return result;
}

// static
template <typename T>
void InteractiveTestApi::AddStep(InteractionSequence::Builder& builder,
                                 T&& step) {
  builder.AddStep(std::move(step));
}

template <typename... Args>
bool InteractiveTestApi::RunTestSequenceInContext(ElementContext context,
                                                  Args&&... steps) {
  // TODO(dfried): is there any additional automation we need to do in order to
  // get proper error scoping, RunLoop timeout handling, etc.? We may have to
  // inject information directly into the steps or step callbacks; it's unclear.
  InteractionSequence::Builder builder;
  (AddStep(builder, std::move(steps)), ...);
  return RunTestSequenceImpl(context, std::move(builder));
}

// static
template <class T>
InteractionSequence::StepBuilder InteractiveTestApi::AfterShow(
    ElementSpecifier element,
    T&& step_callback) {
  StepBuilder builder;
  builder.SetDescription("AfterShow()");
  internal::SpecifyElement(builder, element);
  builder.SetStartCallback(
      base::RectifyCallback<InteractionSequence::StepStartCallback>(
          std::forward<T>(step_callback)));
  return builder;
}

// static
template <class T>
InteractionSequence::StepBuilder InteractiveTestApi::AfterActivate(
    ElementSpecifier element,
    T&& step_callback) {
  StepBuilder builder;
  builder.SetDescription("AfterActivate()");
  internal::SpecifyElement(builder, element);
  builder.SetType(InteractionSequence::StepType::kActivated);
  builder.SetStartCallback(
      base::RectifyCallback<InteractionSequence::StepStartCallback>(
          std::forward<T>(step_callback)));
  return builder;
}

// static
template <class T>
InteractionSequence::StepBuilder InteractiveTestApi::AfterEvent(
    ElementSpecifier element,
    CustomElementEventType event_type,
    T&& step_callback) {
  StepBuilder builder;
  builder.SetDescription(
      base::StrCat({"AfterEvent( ", event_type.GetName(), " )"}));
  internal::SpecifyElement(builder, element);
  builder.SetType(InteractionSequence::StepType::kCustomEvent, event_type);
  builder.SetStartCallback(
      base::RectifyCallback<InteractionSequence::StepStartCallback>(
          std::forward<T>(step_callback)));
  return builder;
}

// static
template <class T>
InteractionSequence::StepBuilder InteractiveTestApi::AfterHide(
    ElementSpecifier element,
    T&& step_callback) {
  StepBuilder builder;
  builder.SetDescription("AfterHide()");
  internal::SpecifyElement(builder, element);
  builder.SetType(InteractionSequence::StepType::kHidden);
  builder.SetStartCallback(
      base::RectifyCallback<InteractionSequence::StepStartCallback>(
          std::forward<T>(step_callback)));
  return builder;
}

// static
template <class T>
InteractionSequence::StepBuilder InteractiveTestApi::WithElement(
    ElementSpecifier element,
    T&& step_callback) {
  StepBuilder builder;
  builder.SetDescription("WithElement()");
  internal::SpecifyElement(builder, element);
  builder.SetStartCallback(
      base::RectifyCallback<InteractionSequence::StepStartCallback>(
          std::forward<T>(step_callback)));
  builder.SetMustBeVisibleAtStart(true);
  return builder;
}

// static
template <typename T>
InteractionSequence::StepBuilder InteractiveTestApi::InAnyContext(T&& step) {
  return std::move(step.SetContext(InteractionSequence::ContextMode::kAny)
                       .FormatDescription("InAnyContext( %s )"));
}

// static
template <typename T>
InteractionSequence::StepBuilder InteractiveTestApi::InSameContext(T&& step) {
  return std::move(
      step.SetContext(InteractionSequence::ContextMode::kFromPreviousStep)
          .FormatDescription("InSameContext( %s )"));
}

template <typename T>
InteractionSequence::StepBuilder InteractiveTestApi::InContext(
    ElementContext context,
    T&& step) {
  const auto fmt = base::StringPrintf("InContext( %p, %%s )",
                                      static_cast<const void*>(context));
  return std::move(step.SetContext(context).FormatDescription(fmt));
}

// static
template <template <typename...> typename C, typename T, typename U>
InteractionSequence::StepBuilder InteractiveTestApi::CheckResult(
    C<T()> function,
    U&& matcher) {
  return std::move(Check(base::BindOnce(
                             [](base::OnceCallback<T()> function,
                                testing::Matcher<T> matcher) {
                               return internal::MatchAndExplain(
                                   "CheckResult()", matcher,
                                   std::move(function).Run());
                             },
                             base::OnceCallback<T()>(std::move(function)),
                             testing::Matcher<T>(std::forward<U>(matcher))))
                       .SetDescription("CheckResult"));
}

// static
template <template <typename...> typename C, typename T, typename U>
InteractionSequence::StepBuilder InteractiveTestApi::CheckElement(
    ElementSpecifier element,
    C<T(TrackedElement*)> function,
    U&& matcher) {
  StepBuilder builder;
  builder.SetDescription("CheckElement()");
  internal::SpecifyElement(builder, element);
  builder.SetStartCallback(base::BindOnce(
      [](base::OnceCallback<T(TrackedElement*)> function,
         testing::Matcher<T> matcher, InteractionSequence* seq,
         TrackedElement* el) {
        if (!internal::MatchAndExplain("CheckElement()", matcher,
                                       std::move(function).Run(el))) {
          seq->FailForTesting();
        }
      },
      base::OnceCallback<T(TrackedElement*)>(std::move(function)),
      testing::Matcher<T>(std::forward<U>(matcher))));
  return builder;
}

}  // namespace ui::test

#endif  // UI_BASE_INTERACTION_INTERACTIVE_TEST_BASE_H_
