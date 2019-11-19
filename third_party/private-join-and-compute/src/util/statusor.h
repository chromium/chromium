/*
 * Copyright 2019 Google Inc.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// StatusOr<T> is the union (w/o using Union) of a Status object and a T object.
// StatusOr models the concept of an object that is either a usable value, or an
// error Status explaining why such a value is not present. To this end,
// StatusOr<T> does not allow its Status value to be Status::OK. Furthermore,
// the value of a StatusOr<T*> must not be null. This is enforced by a debug
// check in most cases, but even when it is not, clients must not set the value
// to null.
//
// The primary use-case for StatusOr<T> is as the return value of a function
// which may fail.
//
// Example client usage for a StatusOr<T>, where T is not a pointer:
//
//  StatusOr<float> result = DoBigCalculationThatCouldFail();
//  if (result.ok()) {
//    float answer = result.ValueOrDie();
//    printf("Big calculation yielded: %f", answer);
//  } else {
//    LOG(ERROR) << result.status();
//  }
//
// Example client usage for a StatusOr<T*>:
//
//  StatusOr<Foo*> result = FooFactory::MakeNewFoo(arg);
//  if (result.ok()) {
//    std::unique_ptr<Foo> foo(result.ValueOrDie());
//    foo->DoSomethingCool();
//  } else {
//    LOG(ERROR) << result.status();
//  }
//
// Example client usage for a StatusOr<std::unique_ptr<T>>:
//
//  StatusOr<std::unique_ptr<Foo>> result = FooFactory::MakeNewFoo(arg);
//  if (result.ok()) {
//    std::unique_ptr<Foo> foo = std::move(result.ValueOrDie());
//    foo->DoSomethingCool();
//  } else {
//    LOG(ERROR) << result.status();
//  }
//
// Example factory implementation returning StatusOr<T*>:
//
//  StatusOr<Foo*> FooFactory::MakeNewFoo(int arg) {
//    if (arg <= 0) {
//      return Status(::private_join_and_compute::StatusCode::kInvalidArgument,
//                            "Arg must be positive");
//    } else {
//      return new Foo(arg);
//    }
//  }
//

#ifndef UTIL_STATUSOR_H_
#define UTIL_STATUSOR_H_

#include <memory>
#include <new>
#include <utility>

#include "third_party/private-join-and-compute/src/util/status.h"  // IWYU pragma: export  // for Status

namespace private_join_and_compute {

  template <typename T>
  class StatusOr {
 public:
  // Construct a new StatusOr with Status::UNKNOWN status
  StatusOr();

  // Construct a new StatusOr with the given non-ok status. After calling
  // this constructor, calls to ValueOrDie() will CHECK-fail.
  //
  // NOTE: Not explicit - we want to use StatusOr<T> as a return
  // value, so it is convenient and sensible to be able to do 'return
  // Status()' when the return type is StatusOr<T>.
  //
  // REQUIRES: status != Status::OK. This requirement is DCHECKed.
  // In optimized builds, passing Status::OK here will have the effect
  // of passing PosixErrorSpace::EINVAL as a fallback.
  StatusOr(const Status& status);  // NOLINT - no explicit

  // Construct a new StatusOr with the given value. If T is a plain pointer,
  // value must not be nullptr. After calling this constructor, calls to
  // ValueOrDie() will succeed, and calls to status() will return OK.
  //
  // NOTE: Not explicit - we want to use StatusOr<T> as a return type
  // so it is convenient and sensible to be able to do 'return T()'
  // when the return type is StatusOr<T>.
  //
  // REQUIRES: if T is a plain pointer, value != nullptr. This requirement is
  // DCHECKed. In optimized builds, passing a nullptr pointer here will have
  // the effect of passing ::private_join_and_compute::StatusCode::kInternal as a fallback.
  StatusOr(const T& value);  // NOLINT - no explicit

  // Copy constructor.
  StatusOr(const StatusOr& other);

  // Assignment operator.
  StatusOr& operator=(const StatusOr& other);

  // Move constructor and move-assignment operator.
  StatusOr(StatusOr&& other) = default;
  StatusOr& operator=(StatusOr&& other) = default;

  // Rvalue-reference overloads of the other constructors and assignment
  // operators, to support move-only types and avoid unnecessary copying.
  StatusOr(T&& value);  // NOLINT - no explicit

  // Returns a reference to our status. If this contains a T, then
  // returns Status::OK.
  const Status& status() const;

  // Returns this->status().ok()
  bool ok() const;

  // Returns a reference to our current value, or CHECK-fails if !this->ok().
  const T& ValueOrDie() const&;
  T& ValueOrDie() &;
  const T&& ValueOrDie() const&&;
  T&& ValueOrDie() &&;

  // Ignores any errors. This method does nothing except potentially suppress
  // complaints from any tools that are checking that errors are not dropped on
  // the floor.
  void IgnoreError() const {}

 private:
  // absl::variant<Status, T> variant_;
  Status status_;
  std::unique_ptr<T> value_;
};

////////////////////////////////////////////////////////////////////////////////
// Implementation details for StatusOr<T>

namespace internal {

class StatusOrHelper {
 public:
  // Move type-agnostic error handling to the .cc.
  static Status HandleInvalidStatusCtorArg();
  static Status HandleNullObjectCtorArg();
  static void Crash(const Status& status);

  // Customized behavior for StatusOr<T> vs. StatusOr<T*>
  template <typename T>
  struct Specialize;
};

template <typename T>
struct StatusOrHelper::Specialize {
  // For non-pointer T, a reference can never be nullptr.
  static inline bool IsValueNull(const T& t) { return false; }
};

template <typename T>
struct StatusOrHelper::Specialize<T*> {
  static inline bool IsValueNull(const T* t) { return t == nullptr; }
};

}  // namespace internal

template <typename T>
inline StatusOr<T>::StatusOr() : status_(Status::UNKNOWN()), value_(nullptr) {}

template <typename T>
inline StatusOr<T>::StatusOr(const Status& status)
    : status_(status), value_(nullptr) {
  if (status.ok()) {
    status_ = internal::StatusOrHelper::HandleInvalidStatusCtorArg();
  }
}

template <typename T>
inline StatusOr<T>::StatusOr(const T& value)
    : status_(Status::OK()), value_(new T(value)) {
  if (internal::StatusOrHelper::Specialize<T>::IsValueNull(*value_)) {
    status_ = internal::StatusOrHelper::HandleNullObjectCtorArg();
  }
}

template <typename T>
inline StatusOr<T>::StatusOr(const StatusOr& other)
    : status_(other.status_), value_(new T(*other.value_)) {}

template <typename T>
inline StatusOr<T>& StatusOr<T>::operator=(const StatusOr<T>& other) {
  status_ = other.status_;
  value_.reset(new T(*other.value_));
  return *this;
}


template <typename T>
inline StatusOr<T>::StatusOr(T&& value)
    : status_(Status::OK()), value_(new T(std::forward<T>(value))) {
  if (internal::StatusOrHelper::Specialize<T>::IsValueNull(*value_)) {
    status_ = internal::StatusOrHelper::HandleNullObjectCtorArg();
  }
}

template <typename T>
inline const Status& StatusOr<T>::status() const {
  return status_;
}

template <typename T>
inline bool StatusOr<T>::ok() const {
  return status_.ok();
}

template <typename T>
inline const T& StatusOr<T>::ValueOrDie() const& {
  if (value_ == nullptr) {
    internal::StatusOrHelper::Crash(status());
  }
  return *value_;
}

template <typename T>
inline T& StatusOr<T>::ValueOrDie() & {
  if (value_ == nullptr) {
    internal::StatusOrHelper::Crash(status());
  }
  return *value_;
}

template <typename T>
inline const T&& StatusOr<T>::ValueOrDie() const&& {
  if (value_ == nullptr) {
    internal::StatusOrHelper::Crash(status());
  }
  return std::move(*value_);
}

template <typename T>
inline T&& StatusOr<T>::ValueOrDie() && {
  if (value_ == nullptr) {
    internal::StatusOrHelper::Crash(status());
  }
  return std::move(*value_);
}

}  // namespace private_join_and_compute

#endif  // UTIL_STATUSOR_H_
