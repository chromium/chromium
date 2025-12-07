// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/unowned_user_data/user_data_factory.h"

#include <utility>

#include "base/check_op.h"

namespace ui {

UserDataFactory::ScopedOverride::ScopedOverride() = default;
UserDataFactory::ScopedOverride::~ScopedOverride() {
  Release();
}

UserDataFactory::ScopedOverride::ScopedOverride(ScopedOverride&& other) noexcept
    : factory_(std::exchange(other.factory_, nullptr)),
      key_(std::exchange(other.key_, UntypedKey())) {}

UserDataFactory::ScopedOverride& UserDataFactory::ScopedOverride::operator=(
    ScopedOverride&& other) noexcept {
  if (this != &other) {
    Release();
    factory_ = std::exchange(other.factory_, nullptr);
    key_ = std::exchange(other.key_, UntypedKey());
  }
  return *this;
}

UserDataFactory::ScopedOverride::ScopedOverride(UserDataFactory& factory,
                                                UntypedKey key)
    : factory_(&factory), key_(key) {
  CHECK(factory_);
  CHECK(key_);
}

void UserDataFactory::ScopedOverride::Release() {
  if (factory_) {
    const auto result = factory_->entries_.erase(key_);
    CHECK_EQ(result, 1U) << "Expected to remove one entry; got " << result;
    factory_ = nullptr;
    key_ = UntypedKey();
  }
}

UserDataFactory::UserDataFactory() = default;
UserDataFactory::~UserDataFactory() {
  CHECK(entries_.empty())
      << "All scoped overrides must end before this object is freed.";
}

void UserDataFactory::AddEntry(UntypedKey key, std::unique_ptr<Entry> entry) {
  const auto result = entries_.emplace(key, std::move(entry));
  CHECK(result.second)
      << "Attempted to add duplicate factory override for user data type "
      << key;
}

}  // namespace ui
