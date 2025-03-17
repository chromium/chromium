// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_INTERACTIVE_TEST_DEFINITIONS_H_
#define UI_BASE_INTERACTION_INTERACTIVE_TEST_DEFINITIONS_H_

#include <type_traits>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace ui::test::internal {

// Specifies an element either by ID or by name.
using ElementSpecifier = std::variant<ElementIdentifier, std::string_view>;

// Specifies a sequence of steps.
using MultiStep = std::vector<InteractionSequence::StepBuilder>;

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

// Map string_view-like types to strings to avoid storing dangling references in
// matchers.
template <typename CharT, typename Traits>
struct MatcherTypeHelper<std::basic_string_view<CharT, Traits>> {
  using ActualType = std::basic_string<CharT, Traits>;
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

InteractionSequence::Builder BuildSubsequence(MultiStep steps);

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

// Converts value of type U to testing::Matcher<T>.
template <typename T, typename U>
testing::Matcher<T> CreateMatcherFromValue(U value) {
  if constexpr (internal::IsReferenceWrapper<U>) {
    return testing::Matcher<T>(T(value.get()));
  } else if constexpr (std::derived_from<U, testing::Matcher<T>> ||
                       std::convertible_to<U, testing::Matcher<T>>) {
    // Note that a Matcher<T> is actually a wrapper around a "matcher"
    // object, not a matcher itself.
    return value;
  } else if constexpr (internal::IsMatcher<U>) {
    // Need to wrap the "matcher" in a Matcher<T> for it to be used.
    return testing::Matcher<T>(value);
  } else {
    return testing::Matcher<T>(
        T(internal::UnwrapArgument<U>(std::move(value))));
  }
}

// Serves as a strongly-typed wrapper around a MultiStep that carries additional
// semantic or syntactic information; for example the "Then" and "Else"
// sequences of an `If()`.
//
// This object is move-constructible and move-assignable, but only to blocks of
// the same type `T`. This prevents a user from writing `Else()` where a
// `ThenBlock` is expected.
//
// Do not use directly. To create a block type for use in InteractiveTest
// primitives, see `DeclareStepBlockFactory()`, which also creates an
// appropriately-named factory method.
template <typename T>
class StepBlock {
 public:
  StepBlock();
  explicit StepBlock(MultiStep steps) : steps_(std::move(steps)) {}
  StepBlock(StepBlock<T>&& other) noexcept = default;
  StepBlock& operator=(StepBlock<T>&& other) noexcept = default;
  ~StepBlock() = default;

  MultiStep& steps() { return steps_; }
  const MultiStep& steps() const { return steps_; }

 private:
  MultiStep steps_;
};

// Explicitly exclude lvalue references when calling certain methods. These
// methods would not compile anyway, but this concept is provided as a courtesy
// to generate better compile errors.
template <typename T>
concept IsValueOrRvalue = !std::is_lvalue_reference_v<T>;

}  // namespace ui::test::internal

#endif  // UI_BASE_INTERACTION_INTERACTIVE_TEST_DEFINITIONS_H_
