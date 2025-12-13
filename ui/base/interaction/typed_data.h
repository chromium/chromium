// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_TYPED_DATA_H_
#define UI_BASE_INTERACTION_TYPED_DATA_H_

#include <concepts>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/types/is_instantiation.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/typed_identifier.h"

// `TypedData<T>` (along with the classes in typed_data_collection.h) allow
// storing of collections of arbitrary data which can be retrieved via
// `TypedIdentifier<T>`.
//
// Example:
// ```
//   DECLARE_TYPED_IDENTIFIER_VALUE(int, kIntegerProperty);
//   DECLARE_TYPED_IDENTIFIER_VALUE(std::string, kStringProperty);
//
//   // ...
//
//   OwnedTypedDataCollection collection;
//   collection.Emplace(kIntegerProperty, 3);
//   collection.Emplace(kStringProperty, "foo");
//
//   LOG(INFO) << "Integer property: "
//             << collection[kIntegerProperty]
//             << " String property: "
//             << collection[kStringProperty];
// ```
//
// This code would output the following:
// ```
//  Integer property: 3 String property: foo
// ```
//
// You can also add typed data in an owned collection or which is owned by some
// other class or scope to an unowned collection for easy lookup:
//
// ```
//   OwnedTypedDataCollection collection;
//   collection.Emplace(kIntegerProperty, 2);
//
//   TypedData string_data(kStringProperty, "bar");
//
//   UnownedTypedDataCollection lookup;
//   lookup.AddAll(collection);
//   lookup.Add(string_data);
//
//   LOG(INFO) << "Integer property: "
//             << lookup[kIntegerProperty]
//             << " String property: "
//             << lookup[kStringProperty];
// ```
//
// This code would output the following:
// ```
//  Integer property: 2 String property: bar
// ```
//
// The only restriction on unowned collections is that they must be destroyed
// before the data they reference, or ReleaseAllReferences() must be called.
// This avoids UAF violations with raw_ptr/raw_ref.

namespace ui {

template <typename T>
class TypedData;

// Base class for typed data that can be stored in a typed data collection and
// looked up by `TypedIdentifier<T>`. This base class does not actually contain
// the data, but allows for polymorphism.
class COMPONENT_EXPORT(UI_BASE_INTERACTION) TypedDataBase {
 public:
  using Identifier = ElementIdentifier;

  virtual ~TypedDataBase() = default;

  Identifier identifier() const { return identifier_; }

  // Retrieves this object as a typed object. The identifier must match.
  template <typename T>
  TypedData<T>& AsTyped(TypedIdentifier<T> id);

  // Retrieves this object as a typed object. The identifier must match.
  template <typename T>
  const TypedData<T>& AsTyped(TypedIdentifier<T> id) const;

 protected:
  TypedDataBase() = default;
  explicit TypedDataBase(Identifier identifier) : identifier_(identifier) {}
  TypedDataBase(TypedDataBase&& other) noexcept = default;

 private:
  const Identifier identifier_;
};

// Represents typed data that can be put into collections and retrieved via
// `TypedIdentifier<T>`. Use `AsTyped<>()` to retrieve this subclass, then
// `*`, `->`, or `get()` to access the data.
//
// Type is enforced by the use of a unique `TypedIdentifier<T>`, which must be
// supplied both to create and retrieve the corresponding typed data. This
// ensures a `TypedDataBase` object is never cast to the wrong type of
// `TypedData`.
template <typename T>
class TypedData : public TypedDataBase {
 public:
  using Type = T;

  // The payload `data_` is constructed in place in the constructor.
  template <typename... Args>
  explicit TypedData(TypedIdentifier<T> identifier, Args&&... args)
      : TypedDataBase(identifier.identifier()),
        data_(std::forward<Args>(args)...) {}

  TypedData() = default;
  TypedData(TypedData&& other) noexcept = default;
  ~TypedData() override = default;

  T& operator*() { return data_; }
  const T& operator*() const { return data_; }
  T* operator->() { return &data_; }
  const T* operator->() const { return &data_; }
  T* get() { return &data_; }
  const T* get() const { return &data_; }

  TypedIdentifier<T> typed_identifier() const {
    return TypedIdentifier<T>(identifier());
  }

 private:
  T data_;
};

// Template implementation.

template <typename T>
TypedData<T>& TypedDataBase::AsTyped(TypedIdentifier<T> id) {
  CHECK_EQ(id.identifier(), identifier_);
  return *static_cast<TypedData<T>*>(this);
}

template <typename T>
const TypedData<T>& TypedDataBase::AsTyped(TypedIdentifier<T> id) const {
  CHECK_EQ(id.identifier(), identifier_);
  return *static_cast<const TypedData<T>*>(this);
}

}  // namespace ui

#endif  // UI_BASE_INTERACTION_TYPED_DATA_H_
