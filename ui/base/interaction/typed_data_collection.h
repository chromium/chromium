// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_TYPED_DATA_COLLECTION_H_
#define UI_BASE_INTERACTION_TYPED_DATA_COLLECTION_H_

#include <concepts>
#include <map>
#include <memory>
#include <type_traits>
#include <vector>

#include "base/check.h"
#include "base/component_export.h"
#include "base/memory/raw_ref.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/typed_data.h"
#include "ui/base/interaction/typed_identifier.h"

namespace ui {

namespace test {
template <typename T>
class ScopedTypedData;
}  // namespace test

// Class which owns typed data, with one element per `TypedIdentifier<T>`.
// The data itself, once added, will be stored at a stable memory address until
// it is freed.
//
// Data is guaranteed to be destroyed in the reverse order it is added.
//
// Lookups are guaranteed to be relatively fast but not necessarily O(1). For
// fast lookups prefer creating an UnownedTypedDataCollection from the typed
// collection.
//
// See usage documentation in typed_data.h for more information.
class COMPONENT_EXPORT(UI_BASE_INTERACTION) OwnedTypedDataCollection final {
 public:
  OwnedTypedDataCollection();
  OwnedTypedDataCollection(OwnedTypedDataCollection&&) noexcept;
  OwnedTypedDataCollection& operator=(OwnedTypedDataCollection&&) noexcept;
  ~OwnedTypedDataCollection();

  bool empty() const { return data_.empty(); }
  auto size() const { return data_.size(); }

  // Frees all data from last added to first.
  void FreeAll();

  // Moves the elements from `other` to the end of this collection.
  void Append(OwnedTypedDataCollection other);

  // Determines if data with untyped identifier `id` is present.

  bool Contains(ElementIdentifier id) const;

  template <typename T>
  bool Contains(TypedIdentifier<T> id) const {
    return Contains(id.identifier());
  }

  // Puts `data` into the collection; the value's ID must not already be there.
  // Returns the newly-added data.
  template <typename T>
  T& Insert(std::unique_ptr<TypedData<T>> data) {
    CHECK(!Contains(data->identifier()))
        << "Duplicate value in collection: " << data->identifier();
    const auto id = data->typed_identifier();
    data_.push_back(std::move(data));
    return *data_.back()->AsTyped(id);
  }

  // Puts `data` into the collection if `T` is movable; the value's ID must not
  // already be there. Returns the newly-added data.
  template <typename T>
    requires std::movable<T>
  T& Insert(TypedData<T> data) {
    return Insert(std::make_unique<TypedData<T>>(std::move(data)));
  }

  // Puts data with `id` and value `value` into the collection if it is not
  // present, or sets the value of the existing data with `id` to `value`.
  //
  // Convenience method; equivalent to:
  // ```
  //  if (collection.Contains(id)) {
  //    *existing = value;
  //  } else {
  //    collection.Emplace(id, value);
  //  }
  //  return collection[id];
  // ```
  //
  // Note that if `id` already exists in the collection, the `TypedData<T>` is
  // not replaced - only its value - and its destruction order is not changed.
  template <typename T, typename U>
  // Note that `std::assignable_from<>` does not work for fundamental types.
    requires(std::assignable_from<T&, U> ||
             (std::is_fundamental_v<T> && !std::is_const_v<T>))
  T& InsertOrAssign(TypedIdentifier<T> id, U&& value) {
    for (auto& entry : data_) {
      if (entry->identifier() == id.identifier()) {
        auto& typed = entry->AsTyped(id);
        *typed = std::forward<U>(value);
        return *typed;
      }
    }
    return Emplace(id, std::forward<U>(value));
  }

  // Constructs a new data item with `id`, in-place, using `args`. Returns the
  // newly-added data.
  template <typename T, typename... Args>
  T& Emplace(TypedIdentifier<T> id, Args&&... args) {
    return Insert(
        std::make_unique<TypedData<T>>(id, std::forward<Args>(args)...));
  }

  // Retrieves the value with identifier `id`, or null if not found.

  template <typename T>
  T* GetIfPresent(TypedIdentifier<T> id) {
    TypedDataBase* const found = Lookup(id.identifier());
    return found ? found->AsTyped(id).get() : nullptr;
  }

  template <typename T>
  const T* GetIfPresent(TypedIdentifier<T> id) const {
    TypedDataBase* const found = Lookup(id.identifier());
    return found ? found->AsTyped(id).get() : nullptr;
  }

  // Retrieves the value with identifier `id`, which must be present; generates
  // an error if not found.

  template <typename T>
  T& operator[](TypedIdentifier<T> id) {
    T* result = GetIfPresent(id);
    CHECK(result) << "Expected collection to contain element " << id;
    return *result;
  }

  template <typename T>
  const T& operator[](TypedIdentifier<T> id) const {
    T* result = GetIfPresent(id);
    CHECK(result) << "Expected collection to contain element " << id;
    return *result;
  }

 private:
  friend class UnownedTypedDataCollection;

  TypedDataBase* Lookup(ElementIdentifier id);
  const TypedDataBase* Lookup(ElementIdentifier id) const;

  std::vector<std::unique_ptr<TypedDataBase>> data_;
};

// Provides a lookup for typed data objects that some other object or scope
// owns.
//
// `TypedData<T>` can be added individually or from an
// `OwnedTypedDataCollection`. This object must be destroyed or have its
// `ReleaseAllReferences()` method called before any of the data it
// references is destroyed in order to avoid UAF/raw_ptr failure. (That is to
// say, its scope must end sooner than the data it references.)
//
// To inject data into the collection for testing purposes, use
// `test::ScopedTypedData`.
//
// See usage documentation in typed_data.h for more information.
class COMPONENT_EXPORT(UI_BASE_INTERACTION) UnownedTypedDataCollection {
 public:
  UnownedTypedDataCollection();
  UnownedTypedDataCollection(UnownedTypedDataCollection&&) noexcept;
  UnownedTypedDataCollection& operator=(UnownedTypedDataCollection&&) noexcept;
  explicit UnownedTypedDataCollection(OwnedTypedDataCollection& source);
  ~UnownedTypedDataCollection();

  bool empty() const { return lookup_.empty(); }
  size_t size() const { return lookup_.size(); }
  bool contains(ElementIdentifier id) { return lookup_.contains(id); }
  template <typename T>
  bool contains(TypedIdentifier<T> id) {
    return contains(id.identifier());
  }

  // Clears the lookup, releasing any references. Not called "clear()" to be
  // explicit about releasing all of its smart refs.
  void ReleaseAllReferences() { lookup_.clear(); }

  // Adds an entry; there must be no duplicate ids. Adding a reference that
  // already exists in the collection is a no-op.
  template <typename T>
  void Add(TypedData<T>& data) {
    AddImpl(data.identifier(), data);
  }

  // Adds the entry with `id` from `source`; the entry must be present and there
  // must be no duplicate ids. Adding a reference that already exists in the
  // collection is a no-op.

  void AddFrom(ElementIdentifier id, OwnedTypedDataCollection& source);

  template <typename T>
  void AddFrom(TypedIdentifier<T> id, OwnedTypedDataCollection& source) {
    AddFrom(id.identifier(), source);
  }

  // Adds all the entries in `source`; there must be no duplicate IDs.
  void AddAll(OwnedTypedDataCollection& source);

  // Retrieves the data from the lookup if present, returns null otherwise.

  template <typename T>
  T* GetIfPresent(TypedIdentifier<T> id) {
    const auto it = lookup_.find(id.identifier());
    return it == lookup_.end() ? nullptr : it->second->AsTyped(id).get();
  }

  template <typename T>
  const T* GetIfPresent(TypedIdentifier<T> id) const {
    const auto it = lookup_.find(id.identifier());
    return it == lookup_.end() ? nullptr : it->second->AsTyped(id).get();
  }

  // Retrieves the data from the lookup; it must be present.

  template <typename T>
  T& operator[](TypedIdentifier<T> id) {
    auto* const result = GetIfPresent(id);
    CHECK(result) << "Expected collection to contain element " << id;
    return *result;
  }

  template <typename T>
  const T& operator[](TypedIdentifier<T> id) const {
    auto* const result = GetIfPresent(id);
    CHECK(result) << "Expected collection to contain element " << id;
    return *result;
  }

 private:
  template <typename T>
  friend class test::ScopedTypedData;

  void AddImpl(ElementIdentifier id, TypedDataBase& data);

  std::map<ElementIdentifier, raw_ref<TypedDataBase>> lookup_;
};

}  // namespace ui

#endif  // UI_BASE_INTERACTION_TYPED_DATA_COLLECTION_H_
