/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
// This file is forked from absl.

#ifndef TENSORFLOW_LITE_SUPPORT_CC_PORT_DEFAULT_STATUS_MACROS_H_
#define TENSORFLOW_LITE_SUPPORT_CC_PORT_DEFAULT_STATUS_MACROS_H_

#include "absl/base/optimization.h"  // from @com_google_absl
#include "absl/status/status.h"      // from @com_google_absl

// Evaluates an expression that produces a `absl::Status`. If the status is not
// ok, returns it from the current function.
//
// For example:
//   absl::Status MultiStepFunction() {
//      RETURN_IF_ERROR(Function(args...));
//      RETURN_IF_ERROR(foo.Method(args...));
//     return absl::OkStatus();
//   }
#define RETURN_IF_ERROR(expr)                                          \
  STATUS_MACROS_IMPL_ELSE_BLOCKER_                                     \
  if (::tflite::support::status_macro_internal::StatusAdaptorForMacros \
          status_macro_internal_adaptor = {(expr)}) {                  \
  } else /* NOLINT */                                                  \
    return status_macro_internal_adaptor.Consume()

#define STATUS_MACROS_CONCAT_NAME(x, y) STATUS_MACROS_CONCAT_IMPL(x, y)
#define STATUS_MACROS_CONCAT_IMPL(x, y) x##y

// Executes an expression `rexpr` that returns a `tflite::support::StatusOr<T>`.
// On OK, moves its value into the variable defined by `lhs`, otherwise returns
// from the current function. By default the error status is returned
// unchanged, but it may be modified by an `error_expression`. If there is an
// error, `lhs` is not evaluated; thus any side effects that `lhs` may have
// only occur in the success case.
//
// Interface:
//
//   ASSIGN_OR_RETURN(lhs, rexpr)
//   ASSIGN_OR_RETURN(lhs, rexpr, error_expression);
//
// WARNING: if lhs is parenthesized, the parentheses are removed. See examples
// for more details.
//
// WARNING: expands into multiple statements; it cannot be used in a single
// statement (e.g. as the body of an if statement without {})!
//
// Example: Declaring and initializing a new variable (ValueType can be anything
//          that can be initialized with assignment, including references):
//   ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(arg));
//
// Example: Assigning to an existing variable:
//   ValueType value;
//   ASSIGN_OR_RETURN(value, MaybeGetValue(arg));
//
// Example: Assigning to an expression with side effects:
//   MyProto data;
//   ASSIGN_OR_RETURN(*data.mutable_str(), MaybeGetValue(arg));
//   // No field "str" is added on error.
//
// Example: Assigning to a std::unique_ptr.
//   ASSIGN_OR_RETURN(std::unique_ptr<T> ptr, MaybeGetPtr(arg));
//
// Example: Assigning to a map. Because of C preprocessor
// limitation, the type used in ASSIGN_OR_RETURN cannot contain comma, so
// wrap lhs in parentheses:
//   ASSIGN_OR_RETURN((absl::flat_hash_map<Foo, Bar> my_map), GetMap());
// Or use auto if the type is obvious enough:
//   ASSIGN_OR_RETURN(const auto& my_map, GetMapRef());
//
// Example: Assigning to structured bindings. The same situation with comma as
// in map, so wrap the statement in parentheses.
//   ASSIGN_OR_RETURN((const auto& [first, second]), GetPair());

#if defined(_WIN32)
#define ASSIGN_OR_RETURN(_1, _2, ...) ASSIGN_OR_RETURN_IMPL_2(_1, _2)
#else
#define ASSIGN_OR_RETURN(...)                                                \
  STATUS_MACROS_IMPL_GET_VARIADIC_((__VA_ARGS__,                             \
                                    STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_3_,  \
                                    STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_2_)) \
  (__VA_ARGS__)
#endif

// =================================================================
// == Implementation details, do not rely on anything below here. ==
// =================================================================

// Some builds do not support C++14 fully yet, using C++11 constexpr technique.
constexpr bool TFLSHasPotentialConditionalOperator(const char* lhs, int index) {
  return (index == -1
              ? false
              : (lhs[index] == '?'
                     ? true
                     : TFLSHasPotentialConditionalOperator(lhs, index - 1)));
}

// MSVC incorrectly expands variadic macros, splice together a macro call to
// work around the bug.
#define STATUS_MACROS_IMPL_GET_VARIADIC_HELPER_(_1, _2, _3, NAME, ...) NAME
#define STATUS_MACROS_IMPL_GET_VARIADIC_(args) \
  STATUS_MACROS_IMPL_GET_VARIADIC_HELPER_ args

#define STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_2_(lhs, rexpr) \
  STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_3_(lhs, rexpr, _)
#define STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_3_(lhs, rexpr, error_expression) \
  STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_(                                      \
      STATUS_MACROS_IMPL_CONCAT_(_status_or_value, __LINE__), lhs, rexpr,    \
      error_expression)
#define STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_(statusor, lhs, rexpr,        \
                                             error_expression)            \
  auto statusor = (rexpr);                                                \
  if (ABSL_PREDICT_FALSE(!statusor.ok())) {                               \
    ::absl::Status _(std::move(statusor).status());                       \
    (void)_; /* error_expression is allowed to not use this variable */   \
    return (error_expression);                                            \
  }                                                                       \
  {                                                                       \
    static_assert(                                                        \
        #lhs[0] != '(' || #lhs[sizeof(#lhs) - 2] != ')' ||                \
            !TFLSHasPotentialConditionalOperator(#lhs, sizeof(#lhs) - 2), \
        "Identified potential conditional operator, consider not "        \
        "using ASSIGN_OR_RETURN");                                        \
  }                                                                       \
  STATUS_MACROS_IMPL_UNPARENTHESIZE_IF_PARENTHESIZED(lhs) =               \
      std::move(statusor).value()

#define ASSIGN_OR_RETURN_IMPL_2(lhs, rexpr) ASSIGN_OR_RETURN_IMPL_3(lhs, rexpr)

#define ASSIGN_OR_RETURN_IMPL_3(lhs, rexpr) \
  ASSIGN_OR_RETURN_IMPL(                    \
      STATUS_MACROS_CONCAT_NAME(_status_or_value, __COUNTER__), lhs, rexpr)

#define ASSIGN_OR_RETURN_IMPL(statusor, lhs, rexpr) \
  auto statusor = (rexpr);                          \
  if (ABSL_PREDICT_FALSE(!statusor.ok())) {         \
    return statusor.status();                       \
  }                                                 \
  lhs = std::move(statusor).value()

// Internal helpers for macro expansion.
#define STATUS_MACROS_IMPL_EAT(...)
#define STATUS_MACROS_IMPL_REM(...) __VA_ARGS__
#define STATUS_MACROS_IMPL_EMPTY()

// Internal helpers for emptyness arguments check.
#define STATUS_MACROS_IMPL_IS_EMPTY_INNER(...) \
  STATUS_MACROS_IMPL_IS_EMPTY_INNER_I(__VA_ARGS__, 0, 1)
#define STATUS_MACROS_IMPL_IS_EMPTY_INNER_I(e0, e1, is_empty, ...) is_empty

#define STATUS_MACROS_IMPL_IS_EMPTY(...) \
  STATUS_MACROS_IMPL_IS_EMPTY_I(__VA_ARGS__)
#define STATUS_MACROS_IMPL_IS_EMPTY_I(...) \
  STATUS_MACROS_IMPL_IS_EMPTY_INNER(_, ##__VA_ARGS__)

// Internal helpers for if statement.
#define STATUS_MACROS_IMPL_IF_1(_Then, _Else) _Then
#define STATUS_MACROS_IMPL_IF_0(_Then, _Else) _Else
#define STATUS_MACROS_IMPL_IF(_Cond, _Then, _Else)          \
  STATUS_MACROS_IMPL_CONCAT_(STATUS_MACROS_IMPL_IF_, _Cond) \
  (_Then, _Else)

// Expands to 1 if the input is parenthesized. Otherwise expands to 0.
#define STATUS_MACROS_IMPL_IS_PARENTHESIZED(...) \
  STATUS_MACROS_IMPL_IS_EMPTY(STATUS_MACROS_IMPL_EAT __VA_ARGS__)

// If the input is parenthesized, removes the parentheses. Otherwise expands to
// the input unchanged.
#define STATUS_MACROS_IMPL_UNPARENTHESIZE_IF_PARENTHESIZED(...)             \
  STATUS_MACROS_IMPL_IF(STATUS_MACROS_IMPL_IS_PARENTHESIZED(__VA_ARGS__),   \
                        STATUS_MACROS_IMPL_REM, STATUS_MACROS_IMPL_EMPTY()) \
  __VA_ARGS__

// Internal helper for concatenating macro values.
#define STATUS_MACROS_IMPL_CONCAT_INNER_(x, y) x##y
#define STATUS_MACROS_IMPL_CONCAT_(x, y) STATUS_MACROS_IMPL_CONCAT_INNER_(x, y)

// The GNU compiler emits a warning for code like:
//
//   if (foo)
//     if (bar) { } else baz;
//
// because it thinks you might want the else to bind to the first if.  This
// leads to problems with code like:
//
//   if (do_expr)  RETURN_IF_ERROR(expr) << "Some message";
//
// The "switch (0) case 0:" idiom is used to suppress this.
#define STATUS_MACROS_IMPL_ELSE_BLOCKER_ \
  switch (0)                             \
  case 0:                                \
  default:  // NOLINT

namespace tflite {
namespace support {
namespace status_macro_internal {

// Provides a conversion to bool so that it can be used inside an if statement
// that declares a variable.
class StatusAdaptorForMacros {
 public:
  StatusAdaptorForMacros(const ::absl::Status& status)  // NOLINT
      : status_(status) {}

  StatusAdaptorForMacros(::absl::Status&& status)  // NOLINT
      : status_(std::move(status)) {}

  StatusAdaptorForMacros(const StatusAdaptorForMacros&) = delete;
  StatusAdaptorForMacros& operator=(const StatusAdaptorForMacros&) = delete;

  explicit operator bool() const { return ABSL_PREDICT_TRUE(status_.ok()); }

  ::absl::Status&& Consume() { return std::move(status_); }

 private:
  ::absl::Status status_;
};

}  // namespace status_macro_internal
}  // namespace support
}  // namespace tflite

#endif  // TENSORFLOW_LITE_SUPPORT_CC_PORT_DEFAULT_STATUS_MACROS_H_
