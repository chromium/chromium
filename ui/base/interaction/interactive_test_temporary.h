// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_INTERACTIVE_TEST_TEMPORARY_H_
#define UI_BASE_INTERACTION_INTERACTIVE_TEST_TEMPORARY_H_

#include <map>
#include <memory>
#include <type_traits>

#include "base/check_op.h"
#include "ui/base/identifier/typed_identifier.h"
#include "ui/base/identifier/unique_identifier.h"

// This file provides machinery for creating and storing temporary values during
// tests. Temporary values may exist only for a few steps, and are often used in
// custom verbs to carry a value between steps without having to explicitly
// allocate ref-counted memory (as any local variable captured by reference
// would disappear when the steps are returned from the custom verb).
//
// Example:
// ```
//   // You can't do this.
//   auto MyVerbWillCrash() {
//     int value;
//     return Steps(Do([&value]{ value = ... }),
//                  Check([&value]{ /* Some expression involving value */}));
//     // value disappears here, steps will UAF when run
//   }
//
//   // You can do this, but you don't want to.
//   auto MyVerbUgly() {
//     // Look how clunky this is.
//     shared_ptr<RefCountedData<int>> holder =
//         base::MakeRefCounted<RefCountedData<int>>();
//     // Have to remember to capture everything the right way, or bad things
//     // happen.
//     return Steps(Do([holder]{ holder.data = ... }),
//                  Check([value]{ /* Expression involving holder.data */}));
//     // holder is freed when the last step that references it completes.
//   }
//
//   // This is what temporary values allow:
//   auto MyVerbGood() {
//     INTERACTIVE_TEST_TEMPORARY_VALUE(int, kValue);
//     // Value is allocated when this step is run and freed on teardown.
//     return Steps(Do([this, kValue]{ SetTemporaryValue(kValue, ...); }),
//                  Check([this, kValue]{ ... GetTemporaryValue(kValue) ... }));
//   }
// ```
//
// Temporaries are identified by an `InteractiveTestTemporary<T>` (where `T` is
// the type of value being held); these are declared with
// `INTERACTIVE_TEST_TEMPORARY_VALUE(type, name)`.
//
// You can then set, get, and clear these values during interactive tests and
// their values will be preserved across steps.
//
// For more information, see:
//  - `InteractiveTestApi::SetTemporaryValue()`
//  - `InteractiveTestApi::GetTemporaryValue()`
//  - `InteractiveTestApi::ClearTemporaryValue()`

namespace ui::test {

// Define an identifier to be used to identify temporaries.

// This is the underlying untyped identifier used by the store object to set and
// get values (see below).
namespace internal {
DECLARE_UNIQUE_IDENTIFIER_TYPE(InteractiveTestTemporaryIdentifier);
}

// This is the typed identifier that links type information to a unique
// identifier, allowing temporary values to be set and retrieved in a type-safe
// manner. These are the keys that are used to set and retrieve temporary values
// in a test. They are declared with `INTERACTIVE_TEST_TEMPORARY_VALUE()`.
template <typename T>
using InteractiveTestTemporary =
    ui::TypedIdentifier<internal::InteractiveTestTemporaryIdentifier, T>;

namespace internal {

// Polymorphic base class held in `InteractiveTestTemporaryStorage`.
class InteractiveTestTemporaryData {
 public:
  InteractiveTestTemporaryData() = default;
  InteractiveTestTemporaryData(const InteractiveTestTemporaryData&) = delete;
  void operator=(const InteractiveTestTemporaryData&) = delete;
  virtual ~InteractiveTestTemporaryData() = default;
};

// Implementation of the data holder for a particular type `T`.
//
// Templated so it can derive from the base class but also easily support
// perfect forwarding.
template <typename T>
class InteractiveTestTemporaryDataT final
    : public InteractiveTestTemporaryData {
 public:
  template <typename U>
  explicit InteractiveTestTemporaryDataT(U&& value)
      : value_(std::forward<U>(value)) {}
  ~InteractiveTestTemporaryDataT() override = default;

  const T& value() const { return value_; }

  template <typename U>
  void set_value(U&& value) {
    value_ = std::forward<U>(value);
  }

 private:
  T value_ = T();
};

// Simple, type-safe collection of temporary data keyed by
// `InteractiveTestTemporary` identifiers. Used internally by
// `InteractiveTestApi`.
class InteractiveTestTemporaryStorage {
 public:
  InteractiveTestTemporaryStorage();
  InteractiveTestTemporaryStorage(const InteractiveTestTemporaryStorage&) =
      delete;
  void operator=(const InteractiveTestTemporaryStorage&) = delete;
  ~InteractiveTestTemporaryStorage();

  // Adds or replaces the value of `var` with `value`.
  // Returns a reference to the value set.
  template <typename T, typename U>
  const T& AddOrSet(InteractiveTestTemporary<T> var, U&& value) {
    auto it = data_.find(var.identifier());
    if (it != data_.end()) {
      static_cast<InteractiveTestTemporaryDataT<T>&>(*it->second)
          .set_value(std::forward<U>(value));
    } else {
      it = data_
               .emplace(var.identifier(),
                        std::make_unique<InteractiveTestTemporaryDataT<T>>(
                            std::forward<U>(value)))
               .first;
    }
    return static_cast<const InteractiveTestTemporaryDataT<T>*>(
               it->second.get())
        ->value();
  }

  // Returns the value for `var`, which much have been set.
  template <typename T>
  const T& Get(InteractiveTestTemporary<T> var) const {
    const auto it = data_.find(var.identifier());
    CHECK(it != data_.end()) << "No value for variable " << var;
    return static_cast<InteractiveTestTemporaryDataT<T>&>(*it->second).value();
  }

  // Removes the value for `var`, freeing its data.
  template <typename T>
  void Remove(InteractiveTestTemporary<T> var) {
    data_.erase(var.identifier());
  }

  // Clears the entire collection, freeing all data.
  void clear() { data_.clear(); }

 private:
  std::map<InteractiveTestTemporaryIdentifier,
           std::unique_ptr<InteractiveTestTemporaryData>>
      data_;
};

}  // namespace internal

}  // namespace ui::test

// Macro used to declare a temporary value with name `Name` and type `Type`.
// Must be used locally (i.e. inside a function body or .cc file).
#define INTERACTIVE_TEST_TEMPORARY_VALUE(Type, Name) \
  DEFINE_MACRO_LOCAL_TYPED_IDENTIFIER_VALUE(         \
      __FILE__, __LINE__,                            \
      ::ui::test::internal::InteractiveTestTemporaryIdentifier, Type, Name)

#endif  // UI_BASE_INTERACTION_INTERACTIVE_TEST_TEMPORARY_H_
