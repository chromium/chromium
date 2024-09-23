// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_INTERACTIVE_TEST_INTERNAL_H_
#define UI_BASE_INTERACTION_INTERACTIVE_TEST_INTERNAL_H_

#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>

#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/rectify_callback.h"
#include "base/types/is_instantiation.h"
#include "base/types/pass_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/interaction/state_observer.h"

class ChromeOSTestLauncherDelegate;
class InteractiveUITestSuite;

namespace ui::test {

class InteractiveTestApi;
class InteractiveTestTest;

namespace internal {

// Element that is present during interactive tests that actions can bounce
// events off of.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kInteractiveTestPivotElementId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kInteractiveTestPivotEventType);

extern const char kInteractiveTestFailedMessagePrefix[];
extern const char kNoCheckDescriptionSpecified[];

class StateObserverElement;

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

  // Adds `state_observer` and associates it with an element with identifier
  // `id` and context `context`. Must be unique in its context.
  // Returns true on success.
  template <typename Observer, typename V = Observer::ValueType>
  bool AddStateObserver(ElementIdentifier id,
                        ElementContext context,
                        std::unique_ptr<Observer> state_observer);

  // Removes `StateObserver` with identifier `id` in `context`; if the context
  // is null, assumes there is exactly one matching observer in some context.
  // Returns true on success.
  bool RemoveStateObserver(ElementIdentifier id, ElementContext context);

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

  // The following are the classes allowed to set the "allow interactive test
  // verbs" flag.
  template <typename T>
    requires std::same_as<T, ui::test::InteractiveTestTest> ||
             std::same_as<T, ChromeOSTestLauncherDelegate> ||
             std::same_as<T, InteractiveUITestSuite>
  static void set_interactive_test_verbs_allowed(base::PassKey<T>) {
    allow_interactive_test_verbs_ = true;
  }

 private:
  friend class ui::test::InteractiveTestTest;
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

  // Used to track state observers and their associated elements.
  std::vector<std::unique_ptr<StateObserverElement>> state_observer_elements_;

  // Used to relay events to trigger follow-up steps.
  std::map<ElementContext, std::unique_ptr<TrackedElement>> pivot_elements_;

  // Overrides the default test failure behavior to test the API itself.
  InteractionSequence::AbortedCallback aborted_callback_for_testing_;

  // Whether interactive test verbs are allowed. See
  // `InteractiveTestApi::RequireInteractiveTest()` for more info.
  static bool allow_interactive_test_verbs_;
};

// Specifies an element either by ID or by name.
using ElementSpecifier = std::variant<ElementIdentifier, std::string_view>;

class StateObserverElement : public TestElementBase {
 public:
  StateObserverElement(ElementIdentifier id, ElementContext context);
  ~StateObserverElement() override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()
};

// Implements an element that is shown when an observed state matches a desired
// value or pattern, and hidden when it does not.
template <typename T>
class StateObserverElementT : public StateObserverElement {
 public:
  // A lookup table is provided per value of `T`.
  using LookupTable = std::map<std::pair<ElementIdentifier, ElementContext>,
                               StateObserverElementT<T>*>;

  // Specify the `id` and `context` of the element to be created, as well as the
  // associated `observer` which will be linked to this element.
  StateObserverElementT(ElementIdentifier id,
                        ElementContext context,
                        std::unique_ptr<StateObserver<T>> observer)
      : StateObserverElement(id, context),
        current_value_(observer->GetStateObserverInitialState()),
        observer_(std::move(observer)) {
    auto& table = GetLookupTable();
    CHECK(!base::Contains(table, std::make_pair(id, context)))
        << "Duplicate ID + context for StateObserver not allowed: " << id
        << ", " << context;
    table.emplace(std::make_pair(id, context), this);
    observer_->SetStateObserverStateChangedCallback(base::BindRepeating(
        &StateObserverElementT::OnStateChanged, base::Unretained(this)));
    OnStateChanged(current_value_);
  }
  ~StateObserverElementT() override {
    CHECK(GetLookupTable().erase(std::make_pair(identifier(), context())));
  }

  void SetTarget(testing::Matcher<T> target) {
    target_value_ = std::move(target);
    UpdateVisibility();
  }

  // Helper method that looks up an element based on `id`, `context`, and
  // whether `seq` allows all contexts to be searched. Fails the sequence if the
  // element is not found.
  static StateObserverElementT<T>* LookupElement(ElementIdentifier id,
                                                 ElementContext context,
                                                 bool search_all_contexts) {
    const auto& lookup_table = GetLookupTable();
    const auto it = lookup_table.find(std::make_pair(id, context));
    if (it != lookup_table.end()) {
      return it->second;
    }

    if (search_all_contexts) {
      for (const auto& [key, ptr] : lookup_table) {
        if (key.first == id) {
          return ptr;
        }
      }
    }

    return nullptr;
  }

 private:
  void OnStateChanged(T new_state) {
    current_value_ = new_state;
    UpdateVisibility();
  }

  void UpdateVisibility() {
    if (target_value_ && target_value_->Matches(current_value_)) {
      Show();
    } else {
      Hide();
    }
  }

  // Fetch the lookup table associated with a value type/template instantiation.
  //
  // This table does not own the instances, just tracks them as long as they are
  // alive and allows them to be retrieved. There is one static table per
  // template instantiation due to the use of `base::NoDestructor`,
  static LookupTable& GetLookupTable() {
    static base::NoDestructor<LookupTable> lookup_table;
    return *lookup_table;
  }

 private:
  T current_value_;
  std::optional<testing::Matcher<T>> target_value_;
  std::unique_ptr<StateObserver<T>> observer_;
};

// Applies `matcher` to `value` and returns the result; on failure a useful
// error message is printed using `test_name`, `value`, and `matcher`.
//
// Steps which use this method will fail if it returns false, printing out the
// details of the step in the usual way.
template <typename T, typename V = std::decay_t<T>>
bool MatchAndExplain(std::string_view test_name,
                     const testing::Matcher<V>& matcher,
                     const T& value) {
  testing::StringMatchResultListener listener;
  if (matcher.MatchAndExplain(value, &listener)) {
    return true;
  }
  std::ostringstream oss;
  oss << test_name << " failed.\nExpected: ";
  matcher.DescribeTo(&oss);
  oss << "\nActual: " << testing::PrintToString(value);
  if (!listener.str().empty()) {
    oss << "\n" << listener.str();
  }
  LOG(ERROR) << oss.str();
  return false;
}

template <typename Observer, typename V>
bool InteractiveTestPrivate::AddStateObserver(
    ElementIdentifier id,
    ElementContext context,
    std::unique_ptr<Observer> state_observer) {
  CHECK(id);
  CHECK(context);
  for (const auto& existing : state_observer_elements_) {
    if (existing->identifier() == id && existing->context() == context) {
      LOG(ERROR) << "AddStateObserver: Duplicate observer added for " << id;
      return false;
    }
  }
  state_observer_elements_.emplace_back(
      std::make_unique<StateObserverElementT<V>>(id, context,
                                                 std::move(state_observer)));
  return true;
}

// Similar to `std::invocable<T, Args...>`, but does not put constraints on the
// parameters passed to the invocation method.
template <typename T>
concept IsCallable = requires { &std::decay_t<T>::operator(); };

// Applies if `T` has bound state (such as a lambda expression with captures).
template <typename T>
concept HasState = !std::is_empty_v<std::remove_reference_t<T>>;

// Helper for matching a function pointer.
template <typename T>
inline constexpr bool IsFunctionPointerValue = false;

template <typename R, typename... Args>
inline constexpr bool IsFunctionPointerValue<R (*)(Args...)> = true;

// Applies if `T` is a function pointer (but not a pointer to an instance
// member function).
template <typename T>
concept IsFunctionPointer = IsFunctionPointerValue<T>;

// Optionally converts `function` to something that is compatible with a
// base::OnceCallback.
template <typename F>
auto MaybeBind(F&& function) {
  if constexpr (base::IsBaseCallback<F>) {
    // Callbacks are already callbacks, so can be returned as-is.
    return std::forward<F>(function);
  } else if constexpr (IsCallable<F> && HasState<F>) {
    // Callable objects with state can only be bound with
    // `base::BindLambdaForTesting`.
    return base::BindLambdaForTesting(std::forward<F>(function));
  } else if constexpr ((IsCallable<F> && !HasState<F>) ||
                       IsFunctionPointer<F>) {
    // Function pointers and empty callable objects can be bound using
    // `base::BindOnce`.
    return base::BindOnce(std::forward<F>(function));
  } else if constexpr (std::same_as<F, decltype(base::DoNothing())>) {
    // base::DoNothing() is compatible with callbacks, so return it as-is.
    return function;
  } else {
    static_assert(base::AlwaysFalse<F>, "Can only bind callable objects.");
  }
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

// Optionally converts `function` to something that is compatible with a
// base::RepeatingCallback, or returns it as-is if it's already a callback.
template <typename F>
base::RepeatingCallback<typename MaybeBindTypeHelper<F>::Signature>
MaybeBindRepeating(F&& function) {
  if constexpr (IsCallable<F> && !HasState<F> &&
                std::copy_constructible<std::decay_t<F>>) {
    return base::BindRepeating(std::forward<F>(function));
  } else {
    return MaybeBind(std::forward<F>(function));
  }
}

template <typename T>
struct ArgsExtractor;

template <typename R, typename... Args>
struct ArgsExtractor<R(Args...)> {
  using Holder = std::tuple<Args...>;
};

template <typename F>
using ReturnTypeOf = MaybeBindTypeHelper<F>::ReturnType;

template <size_t N, typename F>
using NthArgumentOf = std::tuple_element_t<
    N,
    typename ArgsExtractor<typename MaybeBindTypeHelper<F>::Signature>::Holder>;

// Requires that `F` resolves to some kind of callable object with call
// signature `S`.
template <typename F, typename S>
concept HasSignature =
    std::same_as<typename MaybeBindTypeHelper<F>::Signature, S> ||
    std::same_as<F, decltype(base::DoNothing())>;

// Helper for `HasCompatibleSignature`; see recursive implementation below.
template <typename F, typename S>
inline constexpr bool HasCompatibleSignatureValue = false;

// Requires that `F` resolves to some kind of callable object whose signature
// can be rectified to `S`; see `base::RectifyCallback` for more information.
// (Basically, `F` can omit arguments from the left of `S`; these arguments
// will be ignored.)
template <typename F, typename S>
concept HasCompatibleSignature = HasCompatibleSignatureValue<F, S>;

// This is the leaf state for the recursive compatibility computation; see
// below.
template <typename F, typename R>
  requires HasSignature<F, R()>
inline constexpr bool HasCompatibleSignatureValue<F, R()> = true;

// Implementation for `HasCompatibleSignature`.
//
// This removes arguments one by one from the left of the target signature `S`
// to see if `F` has that signature. The recursion stops when one matches, or
// when the arg list is empty (in which case the leaf state is hit, above).
template <typename F, typename R, typename A, typename... Args>
  requires HasSignature<F, R(A, Args...)> ||
               HasCompatibleSignature<F, R(Args...)>
inline constexpr bool HasCompatibleSignatureValue<F, R(A, Args...)> = true;

// Checks that `T` is a reference wrapper around any type.
template <typename T>
concept IsReferenceWrapper = base::is_instantiation<std::reference_wrapper, T>;

// Helper to determine the type used to match a value. The default is to just
// use the decayed value type.
template <typename T>
struct MatcherTypeHelper {
  using ActualType = T;
};

// Specialization for string types used in Chrome. For any representation of a
// string using character type, the type used for matching is the corresponding
// `std::basic_string`.
//
// Add to this template if different character formats become supported (e.g.
// char8_t, char32_t, wchar_t, etc.)
template <typename C>
  requires(std::same_as<std::remove_const_t<C>, char> ||
           std::same_as<std::remove_const_t<C>, char16_t>)
struct MatcherTypeHelper<C*> {
  using ActualType = std::basic_string<std::remove_const_t<C>>;
};

// Gets the appropriate matchable type for `T`. This affects string-like types
// (e.g. `const char*`) as the corresponding `Matcher` should match a
// `std::string` or `std::u16string`.
template <typename T>
using MatcherTypeFor = MatcherTypeHelper<std::decay_t<T>>::ActualType;

// Determines if `T` is a valid type to be used in a matcher. This precludes
// string-like types (const char*, constexpr char16_t[], etc.) in favor of
// `std::string` and `std::u16string`.
template <typename T>
concept IsValidMatcherType = std::same_as<T, MatcherTypeFor<T>>;

template <typename T>
concept IsGtestMatcher = requires { typename T::is_gtest_matcher; };

template <typename T>
concept HasMatchAndExplain = requires { &T::MatchAndExplain; };

template <typename T>
concept IsMatcher = IsGtestMatcher<T> || HasMatchAndExplain<T> ||
                    base::is_instantiation<testing::PolymorphicMatcher, T>;

// Accepts any function-like object that is compatible with
// `InteractionSequence::StepCallback`.
template <typename F>
concept IsStepCallback = internal::
    HasCompatibleSignature<F, void(InteractionSequence*, TrackedElement*)>;

// Accepts any function-like object that can be used with `Check()` and
// `CheckResult()`.
template <typename F, typename R>
concept IsCheckCallback =
    internal::HasCompatibleSignature<F,
                                     R(const InteractionSequence*,
                                       const TrackedElement*)>;

// Converts an ElementSpecifier to an element ID or name and sets it onto
// `builder`.
void SpecifyElement(ui::InteractionSequence::StepBuilder& builder,
                    ElementSpecifier element);

std::string DescribeElement(ElementSpecifier spec);

InteractionSequence::Builder BuildSubsequence(
    InteractiveTestPrivate::MultiStep steps);

// Takes an argument expected to be a literal value and retrieves the literal
// value by either calling the object (if it's callable), unwrapping it (if it's
// a `std::reference_wrapper`) or just returning it otherwise.
//
// This allows e.g. passing deferred or computed values to the `Log()` verb.
template <typename Arg>
auto UnwrapArgument(Arg arg) {
  if constexpr (base::IsBaseCallback<Arg>) {
    return std::move(arg).Run();
  } else if constexpr (internal::IsFunctionPointer<Arg>) {
    return (*arg)();
  } else if constexpr (internal::IsCallable<Arg>) {
    return arg();
  } else {
    return arg;
  }
}

}  // namespace internal

}  // namespace ui::test

#endif  // UI_BASE_INTERACTION_INTERACTIVE_TEST_INTERNAL_H_
