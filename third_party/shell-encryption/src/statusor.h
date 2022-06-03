/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef RLWE_STATUSOR_H_
#define RLWE_STATUSOR_H_

#include <cassert>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/types/optional.h"

namespace rlwe {

template <typename T>
class StatusOr {
 public:
  // Construct a new StatusOr with Status::UNKNOWN status
  StatusOr();

  // Construct a new StatusOr with the given non-ok status. After calling
  // this constructor, calls to value() will CHECK-fail.
  //
  // NOTE: Not explicit - we want to use StatusOr<T> as a return
  // value, so it is convenient and sensible to be able to do 'return
  // Status()' when the return type is StatusOr<T>.
  //
  // REQUIRES: status != Status::OK. This requirement is DCHECKed.
  // In optimized builds, passing Status::OK here will have the effect
  // of passing PosixErrorSpace::EINVAL as a fallback.
  StatusOr(const absl::Status& status);

  // Construct a new StatusOr with the given value. If T is a plain pointer,
  // value must not be NULL. After calling this constructor, calls to
  // value() will succeed, and calls to status() will return OK.
  //
  // NOTE: Not explicit - we want to use StatusOr<T> as a return type
  // so it is convenient and sensible to be able to do 'return T()'
  // when the return type is StatusOr<T>.
  //
  // REQUIRES: if T is a plain pointer, value != NULL. This requirement is
  // DCHECKed. In optimized builds, passing a NULL pointer here will have
  // the effect of passing absl::StatusCode::kInternal as a fallback.
  StatusOr(const T& value);

  // Copy constructor.
  StatusOr(const StatusOr& other);

  // Assignment operator.
  StatusOr& operator=(const StatusOr& other);

  // Move constructor and move-assignment operator.
  StatusOr(StatusOr&& other) = default;
  StatusOr& operator=(StatusOr&& other) = default;

  // Rvalue-reference overloads of the other constructors and assignment
  // operators, to support move-only types and avoid unnecessary copying.
  StatusOr(T&& value);

  // Returns a reference to our status. If this contains a T, then
  // returns Status::OK.
  const absl::Status& status() const;

  // Returns this->status().ok()
  bool ok() const;

  // Returns a reference to our current value, or CHECK-fails if !this->ok().
  const T& ValueOrDie() const&;
  T& ValueOrDie() &;
  const T&& ValueOrDie() const&&;
  T&& ValueOrDie() &&;

  // Returns a reference to our current value, or CHECK-fails if !this->ok().
  const T& value() const&;
  T& value() &;
  const T&& value() const&&;
  T&& value() &&;

  // Ignores any errors. This method does nothing except potentially suppress
  // complaints from any tools that are checking that errors are not dropped on
  // the floor.
  void IgnoreError() const {}

  operator absl::Status() const { return status(); }

  template <template <typename> class OtherStatusOrType>
  operator OtherStatusOrType<T>() {
    if (value_) {
      return OtherStatusOrType<T>(std::move(value_.value()));
    } else {
      return OtherStatusOrType<T>(status());
    }
  }

 private:
  absl::Status status_;
  absl::optional<T> value_;
};

namespace internal {

class StatusOrHelper {
 public:
  // Move type-agnostic error handling to the .cc.
  static absl::Status HandleInvalidStatusCtorArg();
  static absl::Status HandleNullObjectCtorArg();
  static void Crash(const absl::Status& status);

  // Customized behavior for StatusOr<T> vs. StatusOr<T*>
  template <typename T>
  struct Specialize;
};

template <typename T>
struct StatusOrHelper::Specialize {
  // For non-pointer T, a reference can never be NULL.
  static inline bool IsValueNull(const T& t) { return false; }
};

template <typename T>
struct StatusOrHelper::Specialize<T*> {
  static inline bool IsValueNull(const T* t) { return t == nullptr; }
};

}  // namespace internal

template <typename T>
inline StatusOr<T>::StatusOr()
    : status_(absl::UnknownError("")), value_(absl::nullopt) {}

template <typename T>
inline StatusOr<T>::StatusOr(const absl::Status& status)
    : status_(status), value_(absl::nullopt) {
  if (status.ok()) {
    status_ = internal::StatusOrHelper::HandleInvalidStatusCtorArg();
  }
}

template <typename T>
inline StatusOr<T>::StatusOr(const T& value)
    : status_(absl::OkStatus()), value_(value) {
  if (internal::StatusOrHelper::Specialize<T>::IsValueNull(value)) {
    status_ = internal::StatusOrHelper::HandleNullObjectCtorArg();
  }
}

template <typename T>
inline StatusOr<T>::StatusOr(const StatusOr& other)
    : status_(other.status_), value_(other.value_) {}

template <typename T>
inline StatusOr<T>& StatusOr<T>::operator=(const StatusOr<T>& other) {
  status_ = other.status_;
  value_.reset(other.value_);
  return *this;
}

template <typename T>
inline StatusOr<T>::StatusOr(T&& value)
    : status_(absl::OkStatus()), value_(std::forward<T>(value)) {
  if (internal::StatusOrHelper::Specialize<T>::IsValueNull(value_.value())) {
    status_ = internal::StatusOrHelper::HandleNullObjectCtorArg();
  }
}

template <typename T>
inline const absl::Status& StatusOr<T>::status() const {
  return status_;
}

template <typename T>
inline bool StatusOr<T>::ok() const {
  return status_.ok();
}

template <typename T>
inline const T& StatusOr<T>::ValueOrDie() const& {
  if (!value_) {
    internal::StatusOrHelper::Crash(status());
  }
  return value_.value();
}

template <typename T>
inline T& StatusOr<T>::ValueOrDie() & {
  if (!value_) {
    internal::StatusOrHelper::Crash(status());
  }
  return value_.value();
}

template <typename T>
inline const T&& StatusOr<T>::ValueOrDie() const&& {
  if (!value_) {
    internal::StatusOrHelper::Crash(status());
  }
  return std::move(value_.value());
}

template <typename T>
inline T&& StatusOr<T>::ValueOrDie() && {
  if (!value_) {
    internal::StatusOrHelper::Crash(status());
  }
  return std::move(value_.value());
}

template <typename T>
inline const T& StatusOr<T>::value() const& {
  if (!value_) {
    internal::StatusOrHelper::Crash(status());
  }
  return value_.value();
}

template <typename T>
inline T& StatusOr<T>::value() & {
  if (!value_) {
    internal::StatusOrHelper::Crash(status());
  }
  return value_.value();
}

template <typename T>
inline const T&& StatusOr<T>::value() const&& {
  if (!value_) {
    internal::StatusOrHelper::Crash(status());
  }
  return std::move(value_.value());
}

template <typename T>
inline T&& StatusOr<T>::value() && {
  if (!value_) {
    internal::StatusOrHelper::Crash(status());
  }
  return std::move(value_.value());
}

}  // namespace rlwe

#endif  // RLWE_STATUSOR_H_
