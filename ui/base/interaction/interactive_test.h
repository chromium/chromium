// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_INTERACTIVE_TEST_H_
#define UI_BASE_INTERACTION_INTERACTIVE_TEST_H_

#include <concepts>
#include <functional>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/rectify_callback.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/interaction/interactive_test_internal.h"
#include "ui/base/interaction/polling_state_observer.h"
#include "ui/base/interaction/state_observer.h"

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
// This class is not a test fixture; it is a mixin that can be added to an
// existing test fixture using `InteractiveTestT<T>` - or just use
// `InteractiveTest`, which *is* a test fixture.
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
  using OnIncompatibleAction =
      internal::InteractiveTestPrivate::OnIncompatibleAction;

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
  // std::string_view denoting a named element in the test sequence.
  using ElementSpecifier = internal::ElementSpecifier;

  // Convenience methods for creating interaction steps of type kShown. The
  // resulting step's start callback is already set; therefore, do not try to
  // add additional logic. However, any other parameter on the step may be set,
  // such as SetMustBeVisibleAtStart(), SetTransitionOnlyOnEvent(),
  // SetContext(), etc.
  //
  // Note that `ActivateSurface()`, `SelectMenuItem()` and
  // `SelectDropdownItem()` are not outside of interactive tests (e.g.
  // interactive_ui_tests); the exception is `SelectDropdownItem()` with the
  // default `input_type`, which programmatically sets the value rather than
  // using the actual drop-down.
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

  // Logs the given arguments, in order, at level INFO.
  //
  // This is *roughly* (but not exactly) equivalent to:
  //   `Do([=](){ LOG(INFO) << args...; })`
  //
  // By default, values are captured at the time the Log step is created, rather
  // than when it is run. If you want the value to be captured at runtime, pass
  // `std::ref(value)` instead:
  //
  // ```
  //   int x = 0;
  //   RunTestSequence(
  //       /* maybe change the value of x */
  //       Log("Value of x at sequence creation: ", x),
  //       Log("Value of x right now: ", std::ref(x)));
  // ```
  template <typename... Args>
  [[nodiscard]] static StepBuilder Log(Args... args);

  // Does an action at this point in the test sequence.
  template <typename A>
    requires internal::HasSignature<A, void()>
  [[nodiscard]] static StepBuilder Do(A&& action);

  // Performs a check and fails the test if `check_callback` returns false.
  template <typename C>
    requires internal::HasSignature<C, bool()>
  [[nodiscard]] static StepBuilder Check(
      C&& check_callback,
      std::string check_description = internal::kNoCheckDescriptionSpecified);

  // Calls `function` and applies `matcher` to the result. If the matcher does
  // not match, an appropriate error message is printed and the test fails.
  //
  // `matcher` should resolve or convert to a `Matcher<R>`.
  template <typename C, typename M, typename R = internal::ReturnTypeOf<C>>
    requires internal::HasSignature<C, R()>
  [[nodiscard]] static StepBuilder CheckResult(
      C&& function,
      M&& matcher,
      std::string check_description = internal::kNoCheckDescriptionSpecified);

  // Checks the value of `variable` against `matcher`. The variable can be any
  // local or class member that is guaranteed to still exist when the step is
  // executed; if its value at the time the step is executed does not match,
  // an appropriate error message is printed and the test fails.
  //
  // There is no need to wrap `variable` in e.g. `std::ref`; it is always
  // captured by reference.
  //
  // `matcher` should resolve or convert to a `Matcher<T>`.
  template <typename V, typename M, typename T = internal::MatcherTypeFor<V>>
  [[nodiscard]] static StepBuilder CheckVariable(
      V& variable,
      M&& matcher,
      std::string check_description = internal::kNoCheckDescriptionSpecified);

  // Checks that `check` returns true for element `element`. Will fail the test
  // sequence if `check` returns false - the callback should log any specific
  // error before returning.
  //
  // Note that unless you add .SetMustBeVisibleAtStart(true), this test step
  // will wait for `element` to be shown before proceeding.
  template <typename C>
    requires internal::HasSignature<C, bool(TrackedElement*)>
  [[nodiscard]] static StepBuilder CheckElement(ElementSpecifier element,
                                                C&& check);

  // As CheckElement(), but checks that the result of calling `function` on
  // `element` matches `matcher`. If not, the mismatch is printed and the test
  // fails.
  //
  // `matcher` should resolve or convert to a `Matcher<R>`.
  template <typename F, typename M, typename R = internal::ReturnTypeOf<F>>
    requires internal::HasSignature<F, R(TrackedElement*)>
  [[nodiscard]] static StepBuilder CheckElement(ElementSpecifier element,
                                                F&& function,
                                                M&& matcher);

  // Shorthand methods for working with basic ElementTracker events. The element
  // will have `step_callback` called on it. You may specify additional
  // constraints such as SetMustBeVisibleAtStart(), SetTransitionOnlyOnEvent(),
  // SetContext(), etc.
  //
  // `step_callback` arguments may be omitted from the left-hand side.
  template <typename T>
    requires internal::IsStepCallback<T>
  [[nodiscard]] static StepBuilder AfterShow(ElementSpecifier element,
                                             T&& step_callback);
  template <typename T>
    requires internal::HasCompatibleSignature<T, void(InteractionSequence*)>
  [[nodiscard]] static StepBuilder AfterActivate(ElementSpecifier element,
                                                 T&& step_callback);
  template <typename T>
    requires internal::IsStepCallback<T>
  [[nodiscard]] static StepBuilder AfterEvent(ElementSpecifier element,
                                              CustomElementEventType event_type,
                                              T&& step_callback);
  template <typename T>
    requires internal::HasCompatibleSignature<T, void(InteractionSequence*)>
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
  template <typename T>
    requires internal::IsStepCallback<T>
  [[nodiscard]] static StepBuilder WithElement(ElementSpecifier element,
                                               T&& step_callback);

  // Ensures that `element_to_check` is not currently present/visible.
  [[nodiscard]] static StepBuilder EnsureNotPresent(
      ElementIdentifier element_to_check);

  // Opposite of EnsureNotPresent. Equivalent to:
  // ```
  //   WithElement(element_to_check, base::DoNothing())
  // ```
  [[nodiscard]] static StepBuilder EnsurePresent(
      ElementSpecifier element_to_check);

  // Specifies an element not relative to any particular other element.
  using AbsoluteElementSpecifier = std::variant<
      // Specify an element that is known at the time the sequence is created.
      // The element must persist until the step executes.
      TrackedElement*,
      // Specify an element pointer that will be valid by the time the step
      // executes. Use `std::ref()` to wrap the pointer that will receive the
      // value.
      std::reference_wrapper<TrackedElement*>,
      // Find and return an element based on an arbitrary rule.
      base::OnceCallback<TrackedElement*()>,
      // Find and return an element in the given context based on a rule.
      base::OnceCallback<TrackedElement*(ElementContext)>>;

  // Names an element specified by `spec` as `name`. If `spec` requires a
  // context, the context of the current step will be used.
  //
  // For Views, prefer `InteractiveViewsTest::NameView()`.
  [[nodiscard]] StepBuilder NameElement(std::string_view name,
                                        AbsoluteElementSpecifier spec);

  // Calls `find_callback` to locate an element relative to element
  // `relative_to` and assign it `name`.
  //
  // For Views, prefer `InteractiveViewsTest::NameViewRelative()`.
  template <typename C>
    requires internal::HasSignature<C, TrackedElement*(TrackedElement*)>
  [[nodiscard]] StepBuilder NameElementRelative(ElementSpecifier relative_to,
                                                std::string_view name,
                                                C&& find_callback);

  // Adds an observed state with identifier `id` in the current context. Use
  // `WaitForState()` to wait for state changes. This is a useful way to wait
  // for an asynchronous state that isn't a UI element.
  //
  // To construct the observer on the fly as the test is running, use the
  // argument-forwarding version below.
  //
  // Note: Some types are unavailable; for any UTF-8 string type, use
  // std::string. For any UTF-16 type, use std::u16string.
  template <typename ObserverBase, typename Observer>
    requires std::derived_from<Observer, ObserverBase> &&
             internal::IsValidMatcherType<typename Observer::ValueType>
  [[nodiscard]] StepBuilder ObserveState(
      StateIdentifier<ObserverBase> id,
      std::unique_ptr<Observer> state_observer);

  // Adds an observed state with identifier `id` in the current context. Use
  // `WaitForState()` to wait for state changes. This is a useful way to wait
  // for an asynchronous state that isn't a UI element.
  //
  // This version of the function forwards its arguments to the `Observer`'s
  // constructor, with some of them being evaluated at runtime:
  //  - Arguments wrapped in `std::ref()` will be unwrapped
  //  - Functions and callbacks will be evaluated and their return values used
  //
  // If you must pass a callback or function pointer to the observer's
  // constructor, use the other version of this method above.
  //
  // Note: Some types are unavailable; for any UTF-8 string type, use
  // std::string. For any UTF-16 type, use std::u16string.
  template <typename Observer, typename... Args>
    requires internal::IsValidMatcherType<typename Observer::ValueType>
  [[nodiscard]] StepBuilder ObserveState(StateIdentifier<Observer> id,
                                         Args&&... args);

  // Polls a state using a polling state observer with `id` and value callback
  // `callback`. See `PollingStateObserver` and
  // `DECLARE_POLLING_STATE_IDENTIFIER_VALUE()` for more info.
  //
  // Use WaitForState() to check the polled state. Note that unlike
  // `ObserveState()`, transient states may be missed, so prefer using a custom
  // event or `ObserveState()` when possible.
  template <typename T, typename C>
    requires internal::IsValidMatcherType<T>
  [[nodiscard]] StepBuilder PollState(
      StateIdentifier<PollingStateObserver<T>> id,
      C&& callback,
      base::TimeDelta polling_interval =
          PollingStateObserver<T>::kDefaultPollingInterval);

  // Polls an element using a polling element with `element_identifier` in the
  // current context using state observer with `id` and value callback
  // `callback`. See `PollingElementStateObserver` and
  // `DECLARE_POLLING_ELEMENT_STATE_IDENTIFIER_VALUE()` for more info.
  //
  // Note that the actual value type is not T, but `std::optional<T>`, as the
  // state will have the value std::nullopt if the element is not present.
  //
  // Use WaitForState() to check the polled state. Note that unlike
  // `ObserveState()`, transient states may be missed, so prefer using a custom
  // event or `ObserveState()` when possible.
  template <typename T, typename C>
    requires internal::IsValidMatcherType<T>
  [[nodiscard]] StepBuilder PollElement(
      StateIdentifier<PollingElementStateObserver<T>> id,
      ui::ElementIdentifier element_identifier,
      C&& callback,
      base::TimeDelta polling_interval =
          PollingStateObserver<T>::kDefaultPollingInterval);

  // Waits for the state of state observer `id` (bound with `ObserveState()` in
  // the current context) to match `value`. If `value` is a function, callback,
  // or `std::reference_wrapper`, it will be called or unwrapped as the step is
  // run, rather than having its value frozen when the test sequence is created.
  // A matcher may also be passed, and the step will proceed when the value of
  // the state satisfies the matcher.
  //
  // See /chrome/test/interaction/README.md for more information.
  template <typename O, typename V>
  [[nodiscard]] static MultiStep WaitForState(StateIdentifier<O> id, V&& value);

  // Ends observation of a state. Each `StateObserver` is normally cleaned up
  // at the end of a test. This cleans up the observer with `id` immediately,
  // including halting any associated polling.
  //
  // Typically unnecessary; included for completeness. Stopping an observation
  // might avoid a UAF, or allow the caller to re-use `id` for a different
  // observation in the same context.
  //
  // Must be called in the same context as `ObserveState()`, `PollState()`, etc.
  template <typename O>
  [[nodiscard]] StepBuilder StopObservingState(StateIdentifier<O> id);

  // Provides syntactic sugar so you can put "in any context" before an action
  // or test step rather than after. For example the following are equivalent:
  // ```
  //    PressButton(kElementIdentifier)
  //        .SetContext(InteractionSequence::ContextMode::kAny)
  //
  //    InAnyContext(PressButton(kElementIdentifier))
  // ```
  //
  // TODO(dfried): consider if we should have a version that takes variadic
  // arguments and applies "in any context" to all of them?
  [[nodiscard]] static MultiStep InAnyContext(MultiStep steps);
  template <typename T>
  [[nodiscard]] static StepBuilder InAnyContext(T&& step);

  // Provides syntactic sugar so you can put "inherit context from previous
  // step" around a step or steps to ensure a sequence executes in a specific
  // context. For example:
  // ```
  //    InAnyContext(WaitForShow(kMyElementInOtherContext)),
  //    InSameContext(Steps(
  //      PressButton(kMyElementInOtherContext),
  //      WaitForHide(kMyElementInOtherContext)
  //    )),
  // ```
  [[nodiscard]] static MultiStep InSameContext(MultiStep steps);
  template <typename T>
  [[nodiscard]] static StepBuilder InSameContext(T&& step);

  [[nodiscard]] MultiStep InContext(ElementContext context, MultiStep steps);
  template <typename T>
  [[nodiscard]] StepBuilder InContext(ElementContext context, T&& step);

  // Specifies that these test step(s) should be executed as soon as they are
  // eligible to trigger, one after the other. By default, once a step is
  // triggered, the system waits for a fresh call stack/message pump iteration
  // to run the step callback and/or check for the next step's triggering
  // condition.
  //
  // Use this when you want to respond to some event by doing a series of checks
  // immediately, e.g.:
  // ```
  //  PressButton(MyDialog::kCommitChangesButtonId),
  //  // Have to check the model when the dialog is completing because the model
  //  // goes away with the dialog.
  //  WithoutDelay(Steps(
  //    WaitForHide(MyDialog::kElementId),
  //    CheckResult(&CheckDialogModelCount, 3),
  //    CheckResult(&CheckDialogModelResult, MyDialogModel::Result::kUpdated))),
  // ```
  [[nodiscard]] static MultiStep WithoutDelay(MultiStep steps);
  template <typename T>
  [[nodiscard]] static StepBuilder WithoutDelay(T&& step);

  // Executes `then_steps` if `condition` is true, else executes `else_steps`.
  template <typename C, typename T, typename E = MultiStep>
    requires internal::HasSignature<C, bool()>
  [[nodiscard]] static StepBuilder If(C&& condition,
                                      T&& then_steps,
                                      E&& else_steps = MultiStep());

  // Executes `then_steps` if the result of `function` matches `matcher`, which
  // should resolve or convert to a `Matcher<R>`. Arguments to `function` may be
  // omitted.
  template <typename F,
            typename M,
            typename T,
            typename E = MultiStep,
            typename R = internal::ReturnTypeOf<F>>
    requires internal::HasCompatibleSignature<F, R(const InteractionSequence*)>
  [[nodiscard]] static StepBuilder IfMatches(F&& function,
                                             M&& matcher,
                                             T&& then_steps,
                                             E&& else_steps = MultiStep());

  // As If*(), but the `condition` receives a pointer to `element`. If the
  // element is not present, null is passed instead (the step does not wait for
  // the element to become visible). Arguments to `condition` may be omitted
  // from the left.
  template <typename C, typename T, typename E = MultiStep>
    requires internal::IsCheckCallback<C, bool>
  [[nodiscard]] static StepBuilder IfElement(ElementSpecifier element,
                                             C&& condition,
                                             T&& then_steps,
                                             E&& else_steps = MultiStep());

  // As IfElement(), but the result of `function` is compared against `matcher`.
  //
  // Arguments to `function` may be omitted from the left. `matcher` should
  // resolve or convert to a `Matcher<R>`.
  template <typename F,
            typename M,
            typename T,
            typename E = MultiStep,
            typename R = internal::ReturnTypeOf<F>>
    requires internal::IsCheckCallback<F, R>
  [[nodiscard]] static StepBuilder IfElementMatches(
      ElementSpecifier element,
      F&& function,
      M&& matcher,
      T&& then_steps,
      E&& else_steps = MultiStep());

  // Executes each of `sequences` in parallel, independently of each other, with
  // the expectation that all will succeed. Each sequence should be a step or
  // MultiStep.
  //
  // All of `sequences` must succeed or the test will fail.
  //
  // This is useful when you are waiting for several discrete events, but the
  // order they may occur in is unspecified/undefined, and there is no way to
  // wait for them in sequence in a way that won't occasionally flake due to the
  // race condition.
  //
  // Side-effects due to callbacks during these subsequences should be
  // minimized, as one sequence could theoretically interfere with the
  // functioning of another.
  template <typename... Args>
  [[nodiscard]] static StepBuilder InParallel(Args&&... sequences);

  // Executes each of `sequences` in parallel, independently of each other, with
  // the expectation that at least one will succeed. (The others will be
  // canceled.) Each sequence should be a step or MultiStep.
  //
  // At least one of `sequences` must succeed or the test will fail.
  //
  // Side-effects due to callbacks during these subsequences should be
  // minimized, as one sequence could theoretically interfere with the
  // functioning of another, and no one sequence is guaranteed to execute to
  // completion.
  template <typename... Args>
  [[nodiscard]] static StepBuilder AnyOf(Args&&... sequences);

  // Sets how to handle a case where a test attempts an operation that is not
  // supported in the current platform/build/environment. Default is to fail
  // the test. See chrome/test/interaction/README.md for best practices.
  //
  // Note that `reason` must always be specified, unless `action` is
  // `kFailTest`, in which case it may be empty.
  [[nodiscard]] StepBuilder SetOnIncompatibleAction(OnIncompatibleAction action,
                                                    const char* reason);

  // Used internally by methods in this class; do not call.
  internal::InteractiveTestPrivate& private_test_impl() {
    return *private_test_impl_;
  }

  // Adds a step or steps to the end of an existing MultiStep. Shorthand for
  // making one or more calls to `std::vector::emplace_back`.
  static void AddStep(MultiStep& dest, StepBuilder src);
  static void AddStep(MultiStep& dest, MultiStep src);

  // Equivalent to calling FormatDescription(format) on every step in `steps`.
  static void AddDescription(MultiStep& steps, std::string_view format);

  // Call this from any test verb which requires an environment suitable for
  // interactive testing. Typically, this means the test must be in an
  // environment where it can control mouse input, window activation, etc.
  //
  // Will crash a test which uses an inappropriate verb, with a description of
  // why the verb was disallowed.
  void RequireInteractiveTest();

 private:
  // Implementation for RunTestSequenceInContext().
  bool RunTestSequenceImpl(ElementContext context,
                           InteractionSequence::Builder builder);

  // Returns a callback to locate an element based on a pivot element and the
  // specification `spec`.
  using FindElementCallback =
      base::OnceCallback<TrackedElement*(TrackedElement*)>;
  static FindElementCallback GetFindElementCallback(
      AbsoluteElementSpecifier spec);

  // Helper method to add a step or steps to a sequence builder.
  static void AddStep(InteractionSequence::Builder& builder, MultiStep steps);
  template <typename T>
  static void AddStep(InteractionSequence::Builder& builder, T&& step);

  std::unique_ptr<internal::InteractiveTestPrivate> private_test_impl_;
};

// Template that adds InteractiveTestApi to any test fixture. No simulators are
// attached to test_util() so if you want to use verbs like PressButton() you
// will need to install your own simulator.
template <typename T>
class InteractiveTestT : public T, public InteractiveTestApi {
 public:
  template <typename... Args>
  explicit InteractiveTestT(Args&&... args)
      : T(std::forward<Args>(args)...),
        InteractiveTestApi(std::make_unique<internal::InteractiveTestPrivate>(
            std::make_unique<InteractionTestUtil>())) {}

  ~InteractiveTestT() override = default;

 protected:
  void SetUp() override {
    T::SetUp();
    private_test_impl().DoTestSetUp();
  }

  void TearDown() override {
    private_test_impl().DoTestTearDown();
    T::TearDown();
  }
};

// A simple test fixture that brings in all of the features of
// InteractiveTestApi. No simulators are attached to test_util() so if you want
// to use verbs like PressButton() you will need to install your own simulator.
//
// Provided for convenience, but generally you will want InteractiveViewsTest
// or InteractiveBrowserTest instead.
using InteractiveTest = InteractiveTestT<testing::Test>;

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
  builder.AddStep(std::forward<T>(step));
}

template <typename... Args>
bool InteractiveTestApi::RunTestSequenceInContext(ElementContext context,
                                                  Args&&... steps) {
  // TODO(dfried): is there any additional automation we need to do in order to
  // get proper error scoping, RunLoop timeout handling, etc.? We may have to
  // inject information directly into the steps or step callbacks; it's unclear.
  InteractionSequence::Builder builder;
  (AddStep(builder, std::forward<Args>(steps)), ...);
  return RunTestSequenceImpl(context, std::move(builder));
}

template <typename A>
  requires internal::HasSignature<A, void()>
// static
InteractiveTestApi::StepBuilder InteractiveTestApi::Do(A&& action) {
  StepBuilder builder;
  builder.SetDescription("Do()");
  builder.SetElementID(internal::kInteractiveTestPivotElementId);
  builder.SetStartCallback(
      base::OnceClosure(internal::MaybeBind(std::forward<A>(action))));
  return builder;
}

// static
template <typename T>
  requires internal::IsStepCallback<T>
InteractionSequence::StepBuilder InteractiveTestApi::AfterShow(
    ElementSpecifier element,
    T&& step_callback) {
  StepBuilder builder;
  builder.SetDescription("AfterShow()");
  internal::SpecifyElement(builder, element);
  builder.SetStartCallback(
      base::RectifyCallback<InteractionSequence::StepStartCallback>(
          internal::MaybeBind(std::forward<T>(step_callback))));
  return builder;
}

// static
template <typename T>
  requires internal::HasCompatibleSignature<T, void(InteractionSequence*)>
InteractionSequence::StepBuilder InteractiveTestApi::AfterActivate(
    ElementSpecifier element,
    T&& step_callback) {
  StepBuilder builder;
  builder.SetDescription("AfterActivate()");
  internal::SpecifyElement(builder, element);
  builder.SetType(InteractionSequence::StepType::kActivated);
  using Callback = base::OnceCallback<void(InteractionSequence*)>;
  builder.SetStartCallback(
      base::BindOnce([](Callback callback, InteractionSequence* seq,
                        TrackedElement*) { std::move(callback).Run(seq); },
                     base::RectifyCallback<Callback>(
                         internal::MaybeBind(std::forward<T>(step_callback)))));
  return builder;
}

// static
template <typename T>
  requires internal::IsStepCallback<T>
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
          internal::MaybeBind(std::forward<T>(step_callback))));
  return builder;
}

// static
template <typename T>
  requires internal::HasCompatibleSignature<T, void(InteractionSequence*)>
InteractionSequence::StepBuilder InteractiveTestApi::AfterHide(
    ElementSpecifier element,
    T&& step_callback) {
  StepBuilder builder;
  builder.SetDescription("AfterHide()");
  internal::SpecifyElement(builder, element);
  builder.SetType(InteractionSequence::StepType::kHidden);
  using Callback = base::OnceCallback<void(InteractionSequence*)>;
  builder.SetStartCallback(
      base::BindOnce([](Callback callback, InteractionSequence* seq,
                        TrackedElement*) { std::move(callback).Run(seq); },
                     base::RectifyCallback<Callback>(
                         internal::MaybeBind(std::forward<T>(step_callback)))));
  return builder;
}

// static
template <typename T>
  requires internal::IsStepCallback<T>
InteractionSequence::StepBuilder InteractiveTestApi::WithElement(
    ElementSpecifier element,
    T&& step_callback) {
  StepBuilder builder;
  builder.SetDescription("WithElement()");
  internal::SpecifyElement(builder, element);
  builder.SetStartCallback(
      base::RectifyCallback<InteractionSequence::StepStartCallback>(
          internal::MaybeBind(std::forward<T>(step_callback))));
  builder.SetMustBeVisibleAtStart(true);
  return builder;
}

// static
template <typename C>
  requires internal::HasSignature<C, TrackedElement*(TrackedElement*)>
InteractionSequence::StepBuilder InteractiveTestApi::NameElementRelative(
    ElementSpecifier relative_to,
    std::string_view name,
    C&& find_callback) {
  StepBuilder builder;
  builder.SetDescription(
      base::StringPrintf("NameElementRelative( \"%s\" )", name.data()));
  ui::test::internal::SpecifyElement(builder, relative_to);
  builder.SetMustBeVisibleAtStart(true);
  builder.SetStartCallback(base::BindOnce(
      [](base::OnceCallback<TrackedElement*(TrackedElement*)> find_callback,
         std::string name, ui::InteractionSequence* seq,
         ui::TrackedElement* el) {
        TrackedElement* const result = std::move(find_callback).Run(el);
        if (!result) {
          LOG(ERROR) << "NameElement(): No View found.";
          seq->FailForTesting();
          return;
        }
        seq->NameElement(result, name);
      },
      ui::test::internal::MaybeBind(std::forward<C>(find_callback)),
      std::string(name)));
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
template <typename T>
InteractionSequence::StepBuilder InteractiveTestApi::WithoutDelay(T&& step) {
  return std::move(
      step.SetStepStartMode(InteractionSequence::StepStartMode::kImmediate)
          .FormatDescription("WithoutDelay( %s )"));
}

// static
template <typename C, typename T, typename E>
  requires internal::IsCheckCallback<C, bool>
InteractionSequence::StepBuilder InteractiveTestApi::IfElement(
    ElementSpecifier element,
    C&& condition,
    T&& then_steps,
    E&& else_steps) {
  auto step = IfElementMatches(element, std::forward<C>(condition), true,
                               std::forward<T>(then_steps),
                               std::forward<E>(else_steps));
  step.SetDescription("IfElement()");
  return step;
}

// static
template <typename F, typename M, typename T, typename E, typename R>
  requires internal::IsCheckCallback<F, R>
InteractionSequence::StepBuilder InteractiveTestApi::IfElementMatches(
    ElementSpecifier element,
    F&& function,
    M&& matcher,
    T&& then_steps,
    E&& else_steps) {
  InteractionSequence::StepBuilder step;
  internal::SpecifyElement(step, element);
  using FunctionType =
      base::OnceCallback<R(const InteractionSequence*, const TrackedElement*)>;
  step.SetSubsequenceMode(InteractionSequence::SubsequenceMode::kAtMostOne);
  step.AddSubsequence(
      internal::BuildSubsequence(Steps(std::forward<T>(then_steps))),
      base::BindOnce(
          [](FunctionType function, testing::Matcher<R> matcher,
             const InteractionSequence* seq, const TrackedElement* el) -> bool {
            return matcher.Matches(std::move(function).Run(seq, el));
          },
          base::RectifyCallback<FunctionType>(
              internal::MaybeBind(std::forward<F>(function))),
          std::forward<M>(matcher)));
  auto temp = Steps(std::forward<E>(else_steps));
  if (!temp.empty()) {
    step.AddSubsequence(internal::BuildSubsequence(std::move(temp)));
  }
  step.SetDescription("IfElementMatches()");
  return step;
}

// static
template <typename C, typename T, typename E>
  requires internal::HasSignature<C, bool()>
InteractionSequence::StepBuilder InteractiveTestApi::If(C&& condition,
                                                        T&& then_steps,
                                                        E&& else_steps) {
  auto step =
      IfMatches(std::forward<C>(condition), true, std::forward<T>(then_steps),
                std::forward<E>(else_steps));
  step.SetDescription("If()");
  return step;
}

// static
template <typename F, typename M, typename T, typename E, typename R>
  requires internal::HasCompatibleSignature<F, R(const InteractionSequence*)>
InteractionSequence::StepBuilder InteractiveTestApi::IfMatches(F&& function,
                                                               M&& matcher,
                                                               T&& then_steps,
                                                               E&& else_steps) {
  auto step = IfElementMatches(
      internal::kInteractiveTestPivotElementId,
      base::BindOnce(
          [](base::OnceCallback<R(const InteractionSequence*)> function,
             const InteractionSequence* seq, const ui::TrackedElement*) {
            return std::move(function).Run(seq);
          },
          base::RectifyCallback<R(const InteractionSequence*)>(
              internal::MaybeBind(std::forward<F>(function)))),
      std::forward<M>(matcher), std::forward<T>(then_steps),
      std::forward<E>(else_steps));
  step.SetDescription("IfMatches()");
  return step;
}

// static
template <typename... Args>
InteractionSequence::StepBuilder InteractiveTestApi::InParallel(
    Args&&... sequences) {
  InteractionSequence::StepBuilder step;
  step.SetElementID(internal::kInteractiveTestPivotElementId);
  step.SetSubsequenceMode(InteractionSequence::SubsequenceMode::kAll);
  (step.AddSubsequence(
       internal::BuildSubsequence(Steps(std::forward<Args>(sequences)))),
   ...);
  step.SetDescription("InParallel()");
  return step;
}

// static
template <typename... Args>
InteractionSequence::StepBuilder InteractiveTestApi::AnyOf(
    Args&&... sequences) {
  InteractionSequence::StepBuilder step;
  step.SetElementID(internal::kInteractiveTestPivotElementId);
  step.SetSubsequenceMode(InteractionSequence::SubsequenceMode::kAtLeastOne);
  (step.AddSubsequence(
       internal::BuildSubsequence(Steps(std::forward<Args>(sequences)))),
   ...);
  step.SetDescription("AnyOf()");
  return step;
}

template <typename ObserverBase, typename Observer>
  requires std::derived_from<Observer, ObserverBase> &&
           internal::IsValidMatcherType<typename Observer::ValueType>
InteractionSequence::StepBuilder InteractiveTestApi::ObserveState(
    StateIdentifier<ObserverBase> id,
    std::unique_ptr<Observer> observer) {
  auto step = CheckElement(
      internal::kInteractiveTestPivotElementId,
      base::BindOnce(
          [](InteractiveTestApi* api, ElementIdentifier id,
             std::unique_ptr<Observer> observer, TrackedElement* el) {
            return api->private_test_impl().AddStateObserver(
                id, el->context(), std::move(observer));
          },
          base::Unretained(this), id.identifier(), std::move(observer)));
  step.SetDescription("ObserveState()");
  return step;
}

template <typename Observer, typename... Args>
  requires internal::IsValidMatcherType<typename Observer::ValueType>
InteractionSequence::StepBuilder InteractiveTestApi::ObserveState(
    StateIdentifier<Observer> id,
    Args&&... args) {
  auto step = CheckElement(
      internal::kInteractiveTestPivotElementId,
      base::BindOnce(
          [](InteractiveTestApi* api, ElementIdentifier id,
             std::remove_cvref_t<Args>... args, TrackedElement* el) {
            return api->private_test_impl().AddStateObserver(
                id, el->context(),
                std::make_unique<Observer>(
                    internal::UnwrapArgument<Args>(std::move(args))...));
          },
          base::Unretained(this), id.identifier(), std::move(args)...));
  step.SetDescription("ObserveState()");
  return step;
}

template <typename T, typename C>
  requires internal::IsValidMatcherType<T>
InteractionSequence::StepBuilder InteractiveTestApi::PollState(
    StateIdentifier<PollingStateObserver<T>> id,
    C&& callback,
    base::TimeDelta polling_interval) {
  using Cb = PollingStateObserver<T>::PollCallback;
  auto step = CheckElement(
      internal::kInteractiveTestPivotElementId,
      base::BindOnce(
          [](InteractiveTestApi* api, ElementIdentifier id, Cb callback,
             base::TimeDelta polling_interval, TrackedElement* el) {
            return api->private_test_impl().AddStateObserver(
                id, el->context(),
                std::make_unique<PollingStateObserver<T>>(std::move(callback),
                                                          polling_interval));
          },
          base::Unretained(this), id.identifier(),
          internal::MaybeBindRepeating(std::forward<C>(callback)),
          polling_interval));
  step.SetDescription("PollState()");
  return step;
}

template <typename T, typename C>
  requires internal::IsValidMatcherType<T>
InteractionSequence::StepBuilder InteractiveTestApi::PollElement(
    StateIdentifier<PollingElementStateObserver<T>> id,
    ui::ElementIdentifier element_identifier,
    C&& callback,
    base::TimeDelta polling_interval) {
  using Cb = PollingElementStateObserver<T>::PollElementCallback;
  auto step = WithElement(
      internal::kInteractiveTestPivotElementId,
      base::BindOnce(
          [](InteractiveTestApi* api, ElementIdentifier id,
             ElementIdentifier element_id, Cb callback,
             base::TimeDelta polling_interval, InteractionSequence* seq,
             TrackedElement* el) {
            if (!api->private_test_impl().AddStateObserver(
                    id, el->context(),
                    std::make_unique<PollingElementStateObserver<T>>(
                        element_id,
                        seq->IsCurrentStepInAnyContextForTesting()
                            ? std::nullopt
                            : std::make_optional(el->context()),
                        std::move(callback), polling_interval))) {
              seq->FailForTesting();
            }
          },
          base::Unretained(this), id.identifier(), element_identifier,
          internal::MaybeBindRepeating(std::forward<C>(callback)),
          polling_interval));
  step.SetDescription(base::StringPrintf("PollElementState(%s)",
                                         element_identifier.GetName().c_str()));
  return step;
}

// static
template <typename O, typename V>
InteractiveTestApi::MultiStep InteractiveTestApi::WaitForState(
    StateIdentifier<O> id,
    V&& value) {
  using T = typename O::ValueType;
  using U = internal::MatcherTypeFor<V>;
  auto wait_callback = base::BindOnce(
      [](ElementIdentifier id, U value, InteractionSequence* seq,
         TrackedElement* el) {
        auto* const typed = internal::StateObserverElementT<T>::LookupElement(
            id, el->context(), seq->IsCurrentStepInAnyContextForTesting());
        if (!typed) {
          LOG(ERROR) << "No state observer registered for identifier " << id
                     << " in the current context. You must observe a state in "
                        "the same context you observed it in.";
          seq->FailForTesting();
          return;
        }
        if constexpr (internal::IsReferenceWrapper<U>) {
          typed->SetTarget(testing::Matcher<T>(T(value.get())));
        } else if constexpr (std::derived_from<U, testing::Matcher<T>>) {
          // Note that a Matcher<T> is actually a wrapper around a "matcher"
          // object, not a matcher itself.
          typed->SetTarget(value);
        } else if constexpr (internal::IsMatcher<U>) {
          // Need to wrap the "matcher" in a Matcher<T> for it to be used.
          typed->SetTarget(testing::Matcher<T>(value));
        } else {
          typed->SetTarget(testing::Matcher<T>(
              T(internal::UnwrapArgument<U>(std::move(value)))));
        }
      },
      id.identifier(), U(std::forward<V>(value)));
  auto result = Steps(WithElement(internal::kInteractiveTestPivotElementId,
                                  std::move(wait_callback)),
                      WaitForShow(id.identifier()));
  AddDescription(result, "WaitForState( %s )");
  return result;
}

template <typename O>
InteractiveTestApi::StepBuilder InteractiveTestApi::StopObservingState(
    StateIdentifier<O> id) {
  auto step = WithElement(
      internal::kInteractiveTestPivotElementId,
      base::BindOnce(
          [](InteractiveTestApi* api, ElementIdentifier id,
             InteractionSequence* seq, TrackedElement* el) {
            const auto context = seq->IsCurrentStepInAnyContextForTesting()
                                     ? ElementContext()
                                     : el->context();
            if (!api->private_test_impl().RemoveStateObserver(id, context)) {
              seq->FailForTesting();
            }
          },
          base::Unretained(this), id.identifier()));
  step.SetDescription(base::StringPrintf("StopObservingState(%s)",
                                         id.identifier().GetName().c_str()));
  return step;
}

// static
template <typename... Args>
InteractiveTestApi::StepBuilder InteractiveTestApi::Log(Args... args) {
  auto step = Do(base::BindOnce(
      [](std::remove_cvref_t<Args>... args) {
        auto info = COMPACT_GOOGLE_LOG_INFO;
        ((info.stream() << internal::UnwrapArgument<Args>(std::move(args))),
         ...);
      },
      std::move(args)...));
  step.SetDescription("Log()");
  return step;
}

// static
template <typename C>
  requires internal::HasSignature<C, bool()>
InteractiveTestApi::StepBuilder InteractiveTestApi::Check(
    C&& check_callback,
    std::string check_description) {
  StepBuilder builder;
  builder.SetDescription(
      base::StringPrintf("Check(\"%s\")", check_description.c_str()));
  builder.SetElementID(internal::kInteractiveTestPivotElementId);
  builder.SetStartCallback(base::BindOnce(
      [](base::OnceCallback<bool()> check_callback, InteractionSequence* seq,
         TrackedElement*) {
        const bool result = std::move(check_callback).Run();
        if (!result) {
          seq->FailForTesting();
        }
      },
      internal::MaybeBind(std::forward<C>(check_callback))));
  return builder;
}

// static
template <typename C, typename M, typename R>
  requires internal::HasSignature<C, R()>
InteractionSequence::StepBuilder InteractiveTestApi::CheckResult(
    C&& function,
    M&& matcher,
    std::string check_description) {
  return std::move(Check(base::BindOnce(
                             [](base::OnceCallback<R()> function,
                                testing::Matcher<R> matcher) {
                               return internal::MatchAndExplain(
                                   "CheckResult()", matcher,
                                   std::move(function).Run());
                             },
                             internal::MaybeBind(std::forward<C>(function)),
                             testing::Matcher<R>(std::forward<M>(matcher))))
                       .SetDescription(base::StringPrintf(
                           "CheckResult(\"%s\")", check_description.c_str())));
}

// static
template <typename V, typename M, typename T>
InteractionSequence::StepBuilder InteractiveTestApi::CheckVariable(
    V& variable,
    M&& matcher,
    std::string check_description) {
  return std::move(
      Check(base::BindOnce(
                [](std::reference_wrapper<V> ref, testing::Matcher<T> matcher) {
                  return internal::MatchAndExplain("CheckVariable()", matcher,
                                                   ref.get());
                },
                std::ref(variable),
                testing::Matcher<T>(std::forward<M>(matcher))))
          .SetDescription(base::StringPrintf("CheckVariable(\"%s\")",
                                             check_description.c_str())));
}

// static
template <typename C>
  requires internal::HasSignature<C, bool(TrackedElement*)>
InteractionSequence::StepBuilder InteractiveTestApi::CheckElement(
    ElementSpecifier element,
    C&& check) {
  return CheckElement(element, std::forward<C>(check), true);
}

// static
template <typename F, typename M, typename R>
  requires internal::HasSignature<F, R(TrackedElement*)>
InteractionSequence::StepBuilder InteractiveTestApi::CheckElement(
    ElementSpecifier element,
    F&& function,
    M&& matcher) {
  StepBuilder builder;
  builder.SetDescription("CheckElement()");
  internal::SpecifyElement(builder, element);
  builder.SetStartCallback(base::BindOnce(
      [](base::OnceCallback<R(TrackedElement*)> function,
         testing::Matcher<R> matcher, InteractionSequence* seq,
         TrackedElement* el) {
        if (!internal::MatchAndExplain("CheckElement()", matcher,
                                       std::move(function).Run(el))) {
          seq->FailForTesting();
        }
      },
      internal::MaybeBind(std::forward<F>(function)),
      testing::Matcher<R>(std::forward<M>(matcher))));
  return builder;
}

}  // namespace ui::test

#endif  // UI_BASE_INTERACTION_INTERACTIVE_TEST_H_
