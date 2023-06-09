// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_INTERACTIVE_TEST_INTERNAL_H_
#define UI_BASE_INTERACTION_INTERACTIVE_TEST_INTERNAL_H_

#include <memory>
#include <tuple>
#include <type_traits>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/rectify_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"

namespace ui::test {

class InteractiveTestApi;

namespace internal {

// Element that is present during interactive tests that actions can bounce
// events off of.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kInteractiveTestPivotElementId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kInteractiveTestPivotEventType);

extern const char kInteractiveTestFailedMessagePrefix[];

// Class that implements functionality for InteractiveTest* that should be
// hidden from tests that inherit the API.
class InteractiveTestPrivate {
 public:
  using MultiStep = std::vector<InteractionSequence::StepBuilder>;

  // Describes what should happen when an action isn't compatible with the
  // current build, platform, or environment. For example, not all tests are set
  // up to handle screenshots, and some Linux window managers cannot bring a
  // background window to the front.
  //
  // See chrome/test/interaction/README.md for best practices.
  enum class OnIncompatibleAction {
    // The test should fail. This is the default, and should be used in almost
    // all cases.
    kFailTest,
    // The sequence should abort immediately and the test should be skipped.
    // Use this when the remainder of the test would depend on the result of the
    // incompatible step. Good for smoke/regression tests that have known
    // incompatibilities but still need to be run in as many environments as
    // possible.
    kSkipTest,
    // As `kSkipTest`, but instead of marking the test as skipped, just stops
    // the test sequence. This is useful when the test cannot continue past the
    // problematic step, but you also want to preserve any non-fatal errors that
    // may have occurred up to that point (or check any conditions after the
    // test stops).
    kHaltTest,
    // The failure should be ignored and the test should continue.
    // Use this when the step does not affect the outcome of the test, such as
    // taking an incidental screenshot in a test job that doesn't support
    // screenshots.
    kIgnoreAndContinue,
  };

  explicit InteractiveTestPrivate(
      std::unique_ptr<InteractionTestUtil> test_util);
  virtual ~InteractiveTestPrivate();
  InteractiveTestPrivate(const InteractiveTestPrivate&) = delete;
  void operator=(const InteractiveTestPrivate&) = delete;

  InteractionTestUtil& test_util() { return *test_util_; }

  OnIncompatibleAction on_incompatible_action() const {
    return on_incompatible_action_;
  }

  bool sequence_skipped() const { return sequence_skipped_; }

  // Possibly fails or skips a sequence based on the result of an action
  // simulation.
  void HandleActionResult(InteractionSequence* seq,
                          const TrackedElement* el,
                          const std::string& operation_name,
                          ActionResult result);

  // Gets the pivot element for the specified context, which must exist.
  TrackedElement* GetPivotElement(ElementContext context) const;

  // Call this method during test SetUp(), or SetUpOnMainThread() for browser
  // tests.
  virtual void DoTestSetUp();

  // Call this method during test TearDown(), or TearDownOnMainThread() for
  // browser tests.
  virtual void DoTestTearDown();

  // Called when the sequence ends, but before we break out of the run loop
  // in RunTestSequenceImpl().
  virtual void OnSequenceComplete();
  virtual void OnSequenceAborted(const InteractionSequence::AbortedData& data);

  // Sets a callback that is called if the test sequence fails instead of
  // failing the current test. Should only be called in tests that are testing
  // InteractiveTestApi or descendant classes.
  void set_aborted_callback_for_testing(
      InteractionSequence::AbortedCallback aborted_callback_for_testing) {
    aborted_callback_for_testing_ = std::move(aborted_callback_for_testing);
  }

  // Places a callback in the message queue to bounce an event off of the pivot
  // element, then responds by executing `task`.
  template <typename T>
  static MultiStep PostTask(const base::StringPiece& description, T&& task);

 private:
  friend class ui::test::InteractiveTestApi;

  // Prepare for a sequence to start.
  void Init(ElementContext initial_context);

  // Clean up after a sequence.
  void Cleanup();

  // Note when a new element appears; we may update the context list.
  void OnElementAdded(TrackedElement* el);

  // Maybe adds a pivot element for the given context.
  void MaybeAddPivotElement(ElementContext context);

  // Tracks whether a sequence succeeded or failed.
  bool success_ = false;

  // Specifies how an incompatible action should be handled.
  OnIncompatibleAction on_incompatible_action_ =
      OnIncompatibleAction::kFailTest;
  std::string on_incompatible_action_reason_;

  // Tracks whether a sequence is skipped. Will only be set if
  // `skip_on_unsupported_operation` is true.
  bool sequence_skipped_ = false;

  // Used to simulate input to UI elements.
  std::unique_ptr<InteractionTestUtil> test_util_;

  // Used to keep track of valid contexts.
  base::CallbackListSubscription context_subscription_;

  // Used to relay events to trigger follow-up steps.
  std::map<ElementContext, std::unique_ptr<TrackedElement>> pivot_elements_;

  // Overrides the default test failure behavior to test the API itself.
  InteractionSequence::AbortedCallback aborted_callback_for_testing_;
};

// Specifies an element either by ID or by name.
using ElementSpecifier = absl::variant<ElementIdentifier, base::StringPiece>;

// Applies `matcher` to `value` and returns the result; on failure a useful
// error message is printed using `test_name`, `value`, and `matcher`.
//
// Steps which use this method will fail if it returns false, printing out the
// details of the step in the usual way.
template <typename T>
bool MatchAndExplain(const base::StringPiece& test_name,
                     testing::Matcher<T>& matcher,
                     T&& value) {
  if (matcher.Matches(value))
    return true;
  std::ostringstream oss;
  oss << test_name << " failed.\nExpected: ";
  matcher.DescribeTo(&oss);
  oss << "\nActual: " << testing::PrintToString(value);
  LOG(ERROR) << oss.str();
  return false;
}

// static
template <typename T>
InteractiveTestPrivate::MultiStep InteractiveTestPrivate::PostTask(
    const base::StringPiece& description,
    T&& task) {
  MultiStep result;
  result.emplace_back(std::move(
      InteractionSequence::StepBuilder()
          .SetDescription(base::StrCat({description, ": PostTask()"}))
          .SetElementID(kInteractiveTestPivotElementId)
          .SetStartCallback(base::BindOnce([](ui::TrackedElement* el) {
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](ElementIdentifier id, ElementContext context) {
                      auto* const el =
                          ui::ElementTracker::GetElementTracker()
                              ->GetFirstMatchingElement(id, context);
                      if (el) {
                        ui::ElementTracker::GetFrameworkDelegate()
                            ->NotifyCustomEvent(el,
                                                kInteractiveTestPivotEventType);
                      }
                      // If there is no pivot element, the test sequence has
                      // been aborted and there's no need to send an additional
                      // error.
                    },
                    el->identifier(), el->context()));
          }))));
  result.emplace_back(std::move(
      InteractionSequence::StepBuilder()
          .SetDescription(base::StrCat({description, ": WaitForComplete()"}))
          .SetElementID(kInteractiveTestPivotElementId)
          .SetContext(InteractionSequence::ContextMode::kFromPreviousStep)
          .SetType(InteractionSequence::StepType::kCustomEvent,
                   kInteractiveTestPivotEventType)
          .SetStartCallback(
              base::RectifyCallback<InteractionSequence::StepStartCallback>(
                  std::move(task)))));
  return result;
}

template <typename T>
constexpr bool IsCallbackValue = base::IsBaseCallback<T>::value;

template <typename T, typename SFINAE = void>
struct IsCallable {
  static constexpr bool value = false;
};

template <typename T>
struct IsCallable<T, std::void_t<decltype(&T::operator())>> {
  static constexpr bool value = true;
};

template <typename T>
constexpr bool IsCallableValue = IsCallable<std::remove_reference_t<T>>::value;

template <typename T, typename SFINAE = void>
struct IsFunctionPointer {
  static constexpr bool value = false;
};

template <typename R, typename... Args>
struct IsFunctionPointer<R (*)(Args...), void> {
  static constexpr bool value = true;
};

template <typename T>
constexpr bool IsFunctionPointerValue = IsFunctionPointer<T>::value;

// Uses SFINAE to choose the correct implementation for `MaybeBind`.
template <typename F, typename SFINAE = void>
struct MaybeBindHelper;

// Callbacks are already callbacks, so can be returned as-is.
template <typename F>
struct MaybeBindHelper<F, std::enable_if_t<IsCallbackValue<F>>> {
  template <class G>
  static auto MaybeBind(G&& function) {
    return std::forward<G>(function);
  }
};

// Callable objects with state can only be bound with
// base::BindLambdaForTesting.
template <typename F>
struct MaybeBindHelper<
    F,
    std::enable_if_t<IsCallableValue<F> && !std::is_empty_v<F>>> {
  template <class G>
  static auto MaybeBind(G&& function) {
    return base::BindLambdaForTesting(std::forward<G>(function));
  }
};

// Function pointers and empty callable objects can be bound using
// base::BindOnce.
template <typename F>
struct MaybeBindHelper<
    F,
    std::enable_if_t<(IsCallableValue<F> && std::is_empty_v<F>) ||
                     IsFunctionPointerValue<F>>> {
  template <class G>
  static auto MaybeBind(G&& function) {
    return base::BindOnce(std::forward<G>(function));
  }
};

// base::DoNothing() is compatible with callbacks, so return it as-is.
template <>
struct MaybeBindHelper<decltype(base::DoNothing()), void> {
  static auto MaybeBind(decltype(base::DoNothing()) function) {
    return function;
  }
};

// Optionally converts `function` to something that is compatible with a
// base::OnceCallback.
template <typename F>
auto MaybeBind(F&& function) {
  return MaybeBindHelper<F>::MaybeBind(std::forward<F>(function));
}

// Helper struct that captures information about what signature a function-like
// object would have if it were bound.
template <typename F>
struct MaybeBindTypeHelper {
  using CallbackType = std::invoke_result_t<decltype(&MaybeBind<F>), F>;
  using ReturnType = typename CallbackType::ResultType;
  using Signature = typename CallbackType::RunType;
};

// DoNothing always has a void return type but no defined signature.
template <>
struct MaybeBindTypeHelper<decltype(base::DoNothing())> {
  using ReturnType = void;
};

template <typename T>
struct ArgsExtractor;

template <typename R, typename... Args>
struct ArgsExtractor<R(Args...)> {
  using holder = std::tuple<Args...>;
};

template <typename F>
using ReturnTypeOf = MaybeBindTypeHelper<F>::ReturnType;

template <size_t N, typename F>
using NthArgumentOf = std::tuple_element_t<
    N,
    typename ArgsExtractor<typename MaybeBindTypeHelper<F>::Signature>::holder>;

// Implementation for HasSignature that uses SFINAE to check whether the
// signature of a callable object `F` matches signature `S`.
template <typename F, typename S>
struct HasSignatureHelper {
  static constexpr bool value =
      std::is_same_v<typename MaybeBindTypeHelper<F>::Signature, S>;
};

// DoNothing() can match any signature that returns void.
template <typename... Args>
struct HasSignatureHelper<decltype(base::DoNothing()), void(Args...)> {
  static constexpr bool value = true;
};

template <typename F, typename S>
constexpr bool HasSignature = HasSignatureHelper<F, S>::value;

// Requires that `F` resolves to some kind of callable object with call
// signature `S`; causes a compile failure on mismatch.
template <typename F, typename S>
using RequireSignature = std::enable_if_t<HasSignature<F, S>>;

template <typename F, typename S>
struct HasCompatibleSignatureHelper;

// This is the leaf state for the recursive compatibility computation; see
// below.
template <typename F, typename R>
struct HasCompatibleSignatureHelper<F, R()> {
  static constexpr bool value = HasSignature<F, R()>;
};

// Implementation for `HasCompatibleSignature` and `RequireCompatibleSignature`.
//
// This removes arguments one by one from the left of the target signature `S`
// to see if `F` has that signature. The recursion stops when one matches, or
// when the arg list is empty (in which case the leaf state is hit, above).
template <typename F, typename R, typename A, typename... Args>
struct HasCompatibleSignatureHelper<F, R(A, Args...)> {
  static constexpr bool value =
      HasSignature<F, R(A, Args...)> ||
      HasCompatibleSignatureHelper<F, R(Args...)>::value;
};

template <typename F, typename S>
constexpr bool HasCompatibleSignature =
    HasCompatibleSignatureHelper<F, S>::value;

// Requires that `F` resolves to some kind of callable object whose signature
// can be rectified to `S`; see `base::RectifyCallback` for more information.
// (Basically, `F` can omit arguments from the left of `S`; these arguments
// will be ignored.)
template <typename F, typename S>
using RequireCompatibleSignature =
    std::enable_if_t<HasCompatibleSignature<F, S>>;

// Converts an ElementSpecifier to an element ID or name and sets it onto
// `builder`.
void SpecifyElement(ui::InteractionSequence::StepBuilder& builder,
                    ElementSpecifier element);

std::string DescribeElement(ElementSpecifier spec);

InteractionSequence::Builder BuildSubsequence(
    InteractiveTestPrivate::MultiStep steps);

}  // namespace internal

}  // namespace ui::test

#endif  // UI_BASE_INTERACTION_INTERACTIVE_TEST_INTERNAL_H_
