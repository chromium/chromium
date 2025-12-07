// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/typed_data_collection.h"

#include "base/containers/adapters.h"
#include "base/notreached.h"
#include "ui/base/interaction/element_identifier.h"

namespace ui {

OwnedTypedDataCollection::OwnedTypedDataCollection() = default;

OwnedTypedDataCollection::OwnedTypedDataCollection(
    OwnedTypedDataCollection&& other) noexcept = default;

OwnedTypedDataCollection::~OwnedTypedDataCollection() {
  FreeAll();
}

OwnedTypedDataCollection& OwnedTypedDataCollection::operator=(
    OwnedTypedDataCollection&& other) noexcept {
  if (this != &other) {
    FreeAll();
    Append(std::move(other));
  }
  return *this;
}

void OwnedTypedDataCollection::FreeAll() {
  for (auto& entry : base::Reversed(data_)) {
    entry.reset();
  }
  data_.clear();
}

void OwnedTypedDataCollection::Append(OwnedTypedDataCollection other) {
  for (auto& entry : other.data_) {
    CHECK(!Contains(entry->identifier()));
    data_.push_back(std::move(entry));
  }
  other.data_.clear();
}

bool OwnedTypedDataCollection::Contains(ElementIdentifier id) const {
  return Lookup(id) != nullptr;
}

const TypedDataBase* OwnedTypedDataCollection::Lookup(
    ElementIdentifier id) const {
  for (auto& entry : data_) {
    if (entry->identifier() == id) {
      return entry.get();
    }
  }
  return nullptr;
}

TypedDataBase* OwnedTypedDataCollection::Lookup(ElementIdentifier id) {
  // Share logic with the const version of this call.
  return const_cast<TypedDataBase*>(
      const_cast<const OwnedTypedDataCollection*>(this)->Lookup(id));
}

UnownedTypedDataCollection::UnownedTypedDataCollection() = default;
UnownedTypedDataCollection::UnownedTypedDataCollection(
    UnownedTypedDataCollection&&) noexcept = default;
UnownedTypedDataCollection& UnownedTypedDataCollection::operator=(
    UnownedTypedDataCollection&&) noexcept = default;
UnownedTypedDataCollection::~UnownedTypedDataCollection() = default;

UnownedTypedDataCollection::UnownedTypedDataCollection(
    OwnedTypedDataCollection& source) {
  AddAll(source);
}

void UnownedTypedDataCollection::AddFrom(ElementIdentifier id,
                                         OwnedTypedDataCollection& source) {
  bool found = false;
  for (auto& entry : source.data_) {
    if (entry->identifier() == id) {
      AddImpl(entry->identifier(), *entry);
      found = true;
      break;
    }
  }
  CHECK(found) << "Did not find expected entry in source: " << id;
}

void UnownedTypedDataCollection::AddAll(OwnedTypedDataCollection& source) {
  for (auto& entry : source.data_) {
    AddImpl(entry->identifier(), *entry);
  }
}

void UnownedTypedDataCollection::AddImpl(ElementIdentifier id,
                                         TypedDataBase& data) {
  const auto result = lookup_.emplace(id, data);
  CHECK(result.second || &result.first->second.get() == &data)
      << "Duplicate value in collection: " << id;
}

}  // namespace ui
