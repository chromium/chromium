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

#ifndef TENSORFLOW_LITE_SUPPORT_CC_PORT_DEFAULT_STATUSOR_H_
#define TENSORFLOW_LITE_SUPPORT_CC_PORT_DEFAULT_STATUSOR_H_

#include <exception>
#include <initializer_list>
#include <new>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/types/variant.h"
#include "absl/utility/utility.h"
#include "tensorflow_lite_support/cc/port/default/statusor_internals.h"

namespace tflite {
namespace support {

#ifndef SWIG
class BadStatusOrAccess : public std::exception {
 public:
  explicit BadStatusOrAccess(absl::Status status);
  ~BadStatusOrAccess() override;
  const char* what() const noexcept override;
  const absl::Status& status() const;

 private:
  absl::Status status_;
};
#endif  // !SWIG

// Returned StatusOr objects may not be ignored.
// Note: Disabled for SWIG as it doesn't parse attributes correctly.  Codesearch
// doesn't handle ifdefs as part of a class definitions (b/6995610), so we use a
// forward declaration.
#ifndef SWIG
template <typename T>
class ABSL_MUST_USE_RESULT StatusOr;
#endif

template <typename T>
class StatusOr : private internal_statusor::StatusOrData<T>,
                 private internal_statusor::CopyCtorBase<T>,
                 private internal_statusor::MoveCtorBase<T>,
                 private internal_statusor::CopyAssignBase<T>,
                 private internal_statusor::MoveAssignBase<T> {
  template <typename U>
  friend class StatusOr;

  typedef internal_statusor::StatusOrData<T> Base;

 public:
  typedef T value_type;

  // Constructs a new StatusOr with Status::UNKNOWN status.  This is marked
  // 'explicit' to try to catch cases like 'return {};', where people think
  // tflite::support::StatusOr<std::vector<int>> will be initialized with an
  // empty vector, instead of a Status::UNKNOWN status.
  explicit StatusOr();

  // StatusOr<T> is copy constructible if T is copy constructible.
  StatusOr(const StatusOr&) = default;
  // StatusOr<T> is copy assignable if T is copy constructible and copy
  // assignable.
  StatusOr& operator=(const StatusOr&) = default;

#ifndef SWIG

  // StatusOr<T> is move constructible if T is move constructible.
  StatusOr(StatusOr&&) = default;
  // StatusOr<T> is moveAssignable if T is move constructible and move
  // assignable.
  StatusOr& operator=(StatusOr&&) = default;

  // Converting constructors from StatusOr<U>, when T is constructible from U.
  // To avoid ambiguity, they are disabled if T is also constructible from
  // StatusOr<U>. Explicit iff the corresponding construction of T from U is
  // explicit.
  template <
      typename U,
      absl::enable_if_t<
          absl::conjunction<
              absl::negation<std::is_same<T, U>>,
              std::is_constructible<T, const U&>,
              std::is_convertible<const U&, T>,
              absl::negation<
                  internal_statusor::
                      IsConstructibleOrConvertibleFromStatusOr<T, U>>>::value,
          int> = 0>
  StatusOr(const StatusOr<U>& other)  // NOLINT
      : Base(static_cast<const typename StatusOr<U>::Base&>(other)) {}
  template <
      typename U,
      absl::enable_if_t<
          absl::conjunction<
              absl::negation<std::is_same<T, U>>,
              std::is_constructible<T, const U&>,
              absl::negation<std::is_convertible<const U&, T>>,
              absl::negation<
                  internal_statusor::
                      IsConstructibleOrConvertibleFromStatusOr<T, U>>>::value,
          int> = 0>
  explicit StatusOr(const StatusOr<U>& other)
      : Base(static_cast<const typename StatusOr<U>::Base&>(other)) {}

  template <
      typename U,
      absl::enable_if_t<
          absl::conjunction<
              absl::negation<std::is_same<T, U>>,
              std::is_constructible<T, U&&>,
              std::is_convertible<U&&, T>,
              absl::negation<
                  internal_statusor::
                      IsConstructibleOrConvertibleFromStatusOr<T, U>>>::value,
          int> = 0>
  StatusOr(StatusOr<U>&& other)  // NOLINT
      : Base(static_cast<typename StatusOr<U>::Base&&>(other)) {}
  template <
      typename U,
      absl::enable_if_t<
          absl::conjunction<
              absl::negation<std::is_same<T, U>>,
              std::is_constructible<T, U&&>,
              absl::negation<std::is_convertible<U&&, T>>,
              absl::negation<
                  internal_statusor::
                      IsConstructibleOrConvertibleFromStatusOr<T, U>>>::value,
          int> = 0>
  explicit StatusOr(StatusOr<U>&& other)
      : Base(static_cast<typename StatusOr<U>::Base&&>(other)) {}

  // Conversion copy/move assignment operator, T must be constructible and
  // assignable from U. Only enable if T cannot be directly assigned from
  // StatusOr<U>.
  template <
      typename U,
      absl::enable_if_t<
          absl::conjunction<
              absl::negation<std::is_same<T, U>>,
              std::is_constructible<T, const U&>,
              std::is_assignable<T, const U&>,
              absl::negation<
                  internal_statusor::
                      IsConstructibleOrConvertibleOrAssignableFromStatusOr<
                          T,
                          U>>>::value,
          int> = 0>
  StatusOr& operator=(const StatusOr<U>& other) {
    this->Assign(other);
    return *this;
  }
  template <
      typename U,
      absl::enable_if_t<
          absl::conjunction<
              absl::negation<std::is_same<T, U>>,
              std::is_constructible<T, U&&>,
              std::is_assignable<T, U&&>,
              absl::negation<
                  internal_statusor::
                      IsConstructibleOrConvertibleOrAssignableFromStatusOr<
                          T,
                          U>>>::value,
          int> = 0>
  StatusOr& operator=(StatusOr<U>&& other) {
    this->Assign(std::move(other));
    return *this;
  }

#endif  // SWIG

  // Constructs a new StatusOr with the given value. After calling this
  // constructor, this->ok() will be true and the contained value may be
  // retrieved with value(), operator*(), or operator->().
  //
  // NOTE: Not explicit - we want to use StatusOr<T> as a return type
  // so it is convenient and sensible to be able to do 'return T()'
  // when the return type is StatusOr<T>.
  //
  // REQUIRES: T is copy constructible.
  // TODO(b/113125838): Replace this constructor with a direct-initialization
  // constructor.
  StatusOr(const T& value);

  // Constructs a new StatusOr with the given non-ok status. After calling this
  // constructor, this->ok() will be false and calls to value() will CHECK-fail.
  //
  // NOTE: Not explicit - we want to use StatusOr<T> as a return
  // value, so it is convenient and sensible to be able to do 'return
  // Status()' when the return type is StatusOr<T>.
  //
  // REQUIRES: !status.ok(). This requirement is DCHECKed.
  // In optimized builds, passing util::OkStatus() here will have the effect
  // of passing util::error::INTERNAL as a fallback.
  StatusOr(const absl::Status& status);
  StatusOr& operator=(const absl::Status& status);

#ifndef SWIG
  // Perfect-forwarding value assignment operator.
  // If `*this` contains a `T` value before the call, the contained value is
  // assigned from `std::forward<U>(v)`; Otherwise, it is directly-initialized
  // from `std::forward<U>(v)`.
  // This function does not participate in overload unless:
  // 1. `std::is_constructible_v<T, U>` is true,
  // 2. `std::is_assignable_v<T&, U>` is true.
  // 3. `std::is_same_v<StatusOr<T>, std::remove_cvref_t<U>>` is false.
  // 4. Assigning `U` to `T` is not ambiguous:
  //  If `U` is `StatusOr<V>` and `T` is constructible and assignable from
  //  both `StatusOr<V>` and `V`, the assignment is considered bug-prone and
  //  ambiguous thus will fail to compile. For example:
  //    StatusOr<bool> s1 = true;  // s1.ok() && *s1 == true
  //    StatusOr<bool> s2 = false;  // s2.ok() && *s2 == false
  //    s1 = s2;  // ambiguous, `s1 = *s2` or `s1 = bool(s2)`?
  template <
      typename U = T,
      typename = typename std::enable_if<absl::conjunction<
          std::is_constructible<T, U&&>,
          std::is_assignable<T&, U&&>,
          internal_statusor::IsForwardingAssignmentValid<T, U&&>>::value>::type>
  StatusOr& operator=(U&& v) {
    this->Assign(std::forward<U>(v));
    return *this;
  }

  // Similar to the `const T&` overload.
  //
  // REQUIRES: T is move constructible.
  StatusOr(T&& value);

  // RValue versions of the operations declared above.
  StatusOr(absl::Status&& status);
  StatusOr& operator=(absl::Status&& status);

  // Constructs the inner value T in-place using the provided args, using the
  // T(args...) constructor.
  template <typename... Args>
  explicit StatusOr(absl::in_place_t, Args&&... args);
  template <typename U, typename... Args>
  explicit StatusOr(absl::in_place_t,
                    std::initializer_list<U> ilist,
                    Args&&... args);

  // Constructs the inner value T in-place using the provided args, using the
  // T(U) (direct-initialization) constructor. Only valid if T can be
  // constructed from a U. Can accept move or copy constructors. Explicit if
  // U is not convertible to T. To avoid ambiguity, this is disabled if U is
  // a StatusOr<J>, where J is convertible to T.
  // Style waiver for implicit conversion granted in cl/209187539.
  template <typename U = T,
            absl::enable_if_t<
                absl::conjunction<
                    internal_statusor::IsDirectInitializationValid<T, U&&>,
                    std::is_constructible<T, U&&>,
                    std::is_convertible<U&&, T>>::value,
                int> = 0>
  StatusOr(U&& u)  // NOLINT
      : StatusOr(absl::in_place, std::forward<U>(u)) {}

  template <typename U = T,
            absl::enable_if_t<
                absl::conjunction<
                    internal_statusor::IsDirectInitializationValid<T, U&&>,
                    std::is_constructible<T, U&&>,
                    absl::negation<std::is_convertible<U&&, T>>>::value,
                int> = 0>
  explicit StatusOr(U&& u)  // NOLINT
      : StatusOr(absl::in_place, std::forward<U>(u)) {}

#endif  // SWIG

  // Returns this->status().ok()
  ABSL_MUST_USE_RESULT bool ok() const { return this->status_.ok(); }

  // Returns a reference to our status. If this contains a T, then
  // returns util::OkStatus().
#ifdef SWIG
  const ::util::Status& status() const;
#else   // SWIG
  const absl::Status& status() const&;
  absl::Status status() &&;
#endif  // SWIG

  // Returns a reference to the held value if `this->ok()`. Otherwise, throws
  // `absl::BadStatusOrAccess` if exception is enabled, or `LOG(FATAL)` if
  // exception is disabled.
  // If you have already checked the status using `this->ok()` or
  // `operator bool()`, you probably want to use `operator*()` or `operator->()`
  // to access the value instead of `value`.
  // Note: for value types that are cheap to copy, prefer simple code:
  //
  //   T value = statusor.value();
  //
  // Otherwise, if the value type is expensive to copy, but can be left
  // in the StatusOr, simply assign to a reference:
  //
  //   T& value = statusor.value();  // or `const T&`
  //
  // Otherwise, if the value type supports an efficient move, it can be
  // used as follows:
  //
  //   T value = std::move(statusor).value();
  //
  // The `std::move` on statusor instead of on the whole expression enables
  // warnings about possible uses of the statusor object after the move.
#ifdef SWIG
  const T& value() const;
#else   // SWIG
  const T& value() const&;
  T& value() &;
  const T&& value() const&&;
  T&& value() &&;
#endif  // SWIG

#ifndef SWIG
  // Returns a reference to the current value.
  //
  // REQUIRES: this->ok() == true, otherwise the behavior is undefined.
  //
  // Use this->ok() or `operator bool()` to verify that there is a current
  // value. Alternatively, see value() for a similar API that guarantees
  // CHECK-failing if there is no current value.
  const T& operator*() const&;
  T& operator*() &;
  const T&& operator*() const&&;
  T&& operator*() &&;
#endif  // SWIG

#ifndef SWIG
  // Returns a pointer to the current value.
  //
  // REQUIRES: this->ok() == true, otherwise the behavior is undefined.
  //
  // Use this->ok() or `operator bool()` to verify that there is a current
  // value.
  const T* operator->() const;
  T* operator->();
#endif  // SWIG

#ifndef SWIG
  // Returns a copy of the current value if this->ok() == true. Otherwise
  // returns a default value.
  template <typename U>
  T value_or(U&& default_value) const&;
  template <typename U>
  T value_or(U&& default_value) &&;
#endif  // SWIG

  // Ignores any errors. This method does nothing except potentially suppress
  // complaints from any tools that are checking that errors are not dropped on
  // the floor.
  void IgnoreError() const;

#ifndef SWIG
  // Reconstructs the inner value T in-place using the provided args, using the
  // T(args...) constructor. Returns reference to the reconstructed `T`.
  template <typename... Args>
  T& emplace(Args&&... args) {
    if (ok()) {
      this->Clear();
      this->MakeValue(std::forward<Args>(args)...);
    } else {
      this->MakeValue(std::forward<Args>(args)...);
      this->status_ = absl::OkStatus();
    }
    return this->data_;
  }

  template <
      typename U,
      typename... Args,
      absl::enable_if_t<
          std::is_constructible<T, std::initializer_list<U>&, Args&&...>::value,
          int> = 0>
  T& emplace(std::initializer_list<U> ilist, Args&&... args) {
    if (ok()) {
      this->Clear();
      this->MakeValue(ilist, std::forward<Args>(args)...);
    } else {
      this->MakeValue(ilist, std::forward<Args>(args)...);
      this->status_ = absl::OkStatus();
    }
    return this->data_;
  }
#endif  // SWIG

 private:
#ifndef SWIG
  using internal_statusor::StatusOrData<T>::Assign;
  template <typename U>
  void Assign(const StatusOr<U>& other);
  template <typename U>
  void Assign(StatusOr<U>&& other);
#endif  // SWIG
};

#ifndef SWIG
////////////////////////////////////////////////////////////////////////////////
// Implementation details for StatusOr<T>

template <typename T>
tflite::support::StatusOr<T>::StatusOr()
    : Base(absl::Status(absl::StatusCode::kUnknown, "")) {}

template <typename T>
tflite::support::StatusOr<T>::StatusOr(const T& value) : Base(value) {}

template <typename T>
tflite::support::StatusOr<T>::StatusOr(const absl::Status& status)
    : Base(status) {}

template <typename T>
tflite::support::StatusOr<T>& StatusOr<T>::operator=(
    const absl::Status& status) {
  this->Assign(status);
  return *this;
}

template <typename T>
tflite::support::StatusOr<T>::StatusOr(T&& value) : Base(std::move(value)) {}

template <typename T>
tflite::support::StatusOr<T>::StatusOr(absl::Status&& status)
    : Base(std::move(status)) {}

template <typename T>
tflite::support::StatusOr<T>& StatusOr<T>::operator=(absl::Status&& status) {
  this->Assign(std::move(status));
  return *this;
}

template <typename T>
template <typename U>
inline void StatusOr<T>::Assign(const StatusOr<U>& other) {
  if (other.ok()) {
    this->Assign(other.value());
  } else {
    this->Assign(other.status());
  }
}

template <typename T>
template <typename U>
inline void StatusOr<T>::Assign(StatusOr<U>&& other) {
  if (other.ok()) {
    this->Assign(std::move(other).value());
  } else {
    this->Assign(std::move(other).status());
  }
}
template <typename T>
template <typename... Args>
tflite::support::StatusOr<T>::StatusOr(absl::in_place_t, Args&&... args)
    : Base(absl::in_place, std::forward<Args>(args)...) {}

template <typename T>
template <typename U, typename... Args>
tflite::support::StatusOr<T>::StatusOr(absl::in_place_t,
                                       std::initializer_list<U> ilist,
                                       Args&&... args)
    : Base(absl::in_place, ilist, std::forward<Args>(args)...) {}

template <typename T>
const absl::Status& StatusOr<T>::status() const& {
  return this->status_;
}
template <typename T>
absl::Status StatusOr<T>::status() && {
  return ok() ? absl::OkStatus() : std::move(this->status_);
}

template <typename T>
const T& StatusOr<T>::value() const& {
  if (!this->ok())
    internal_statusor::ThrowBadStatusOrAccess(this->status_);
  return this->data_;
}

template <typename T>
T& StatusOr<T>::value() & {
  if (!this->ok())
    internal_statusor::ThrowBadStatusOrAccess(this->status_);
  return this->data_;
}

template <typename T>
const T&& StatusOr<T>::value() const&& {
  if (!this->ok()) {
    internal_statusor::ThrowBadStatusOrAccess(std::move(this->status_));
  }
  return std::move(this->data_);
}

template <typename T>
T&& StatusOr<T>::value() && {
  if (!this->ok()) {
    internal_statusor::ThrowBadStatusOrAccess(std::move(this->status_));
  }
  return std::move(this->data_);
}

template <typename T>
const T& StatusOr<T>::operator*() const& {
  this->EnsureOk();
  return this->data_;
}

template <typename T>
T& StatusOr<T>::operator*() & {
  this->EnsureOk();
  return this->data_;
}

template <typename T>
const T&& StatusOr<T>::operator*() const&& {
  this->EnsureOk();
  return std::move(this->data_);
}

template <typename T>
T&& StatusOr<T>::operator*() && {
  this->EnsureOk();
  return std::move(this->data_);
}

template <typename T>
const T* StatusOr<T>::operator->() const {
  this->EnsureOk();
  return &this->data_;
}

template <typename T>
T* StatusOr<T>::operator->() {
  this->EnsureOk();
  return &this->data_;
}

template <typename T>
template <typename U>
T StatusOr<T>::value_or(U&& default_value) const& {
  if (ok()) {
    return this->data_;
  }
  return std::forward<U>(default_value);
}

template <typename T>
template <typename U>
T StatusOr<T>::value_or(U&& default_value) && {
  if (ok()) {
    return std::move(this->data_);
  }
  return std::forward<U>(default_value);
}

template <typename T>
void StatusOr<T>::IgnoreError() const {
  // no-op
}

#endif  // SWIG

}  // namespace support
}  // namespace tflite

#endif  // TENSORFLOW_LITE_SUPPORT_CC_PORT_DEFAULT_STATUSOR_H_
