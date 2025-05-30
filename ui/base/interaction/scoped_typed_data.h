// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_SCOPED_TYPED_DATA_H_
#define UI_BASE_INTERACTION_SCOPED_TYPED_DATA_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "ui/base/interaction/typed_data.h"
#include "ui/base/interaction/typed_data_collection.h"
#include "ui/base/interaction/typed_identifier.h"

namespace ui::test {

// Replaces the data in an `UnownedTypedDataCollection` with this value as long
// as it is in scope, then restores the original typed data (if there was any).
template <typename T>
class ScopedTypedData : public TypedData<T> {
 public:
  template <typename... Args>
  ScopedTypedData(UnownedTypedDataCollection& collection,
                  TypedIdentifier<T> id,
                  Args&&... args)
      : TypedData<T>(id, std::forward<Args>(args)...), collection_(collection) {
    const auto it = collection_->lookup_.find(id.identifier());
    if (it != collection_->lookup_.end()) {
      old_value_ = &it->second.get();
      collection_->lookup_.erase(it);
    }
    collection_->lookup_.emplace(id.identifier(), *this);
  }

  ~ScopedTypedData() override {
    const auto id = TypedData<T>::identifier();
    collection_->lookup_.erase(id);
    if (old_value_) {
      collection_->lookup_.emplace(id, *old_value_);
    }
  }

 private:
  raw_ref<UnownedTypedDataCollection> collection_;
  raw_ptr<TypedDataBase> old_value_ = nullptr;
};

}  // namespace ui::test

#endif  // UI_BASE_INTERACTION_SCOPED_TYPED_DATA_H_
