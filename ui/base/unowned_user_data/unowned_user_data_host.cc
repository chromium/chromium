// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/unowned_user_data/unowned_user_data_host.h"

#include "base/check.h"

namespace ui {

UnownedUserDataHost::UnownedUserDataHost() = default;

UnownedUserDataHost::~UnownedUserDataHost() {
  // All UnownedUserData should be removed before the host is destroyed;
  // otherwise, there could be a UAF when they try to remove themselves as they
  // are destroyed.
  // If any remain, print out the first entry's key. There should never be any,
  // so this should be sufficient to help folks debug.
  CHECK(map_.empty()) << "All UnownedUserData must be removed before the "
                      << "corresponding UnownedUserDataHost is destroyed. "
                      << "First remaining key: " << map_.begin()->first;
}

void UnownedUserDataHost::SetImpl(UntypedKey key, void* data) {
  CHECK(data) << "Assigning bad data for key: " << key;
  const bool inserted = map_.insert_or_assign(key, data).second;
  // Ensure a new value was inserted into the map unless the key was explicitly
  // marked as being used for testing (in which case, we allow it to be
  // overwritten).
  CHECK(inserted) << "Attempted to reinsert data for key: " << key;
}

void UnownedUserDataHost::EraseImpl(UntypedKey key) {
  const bool erased = map_.erase(key);
  // The value should have been erased unless the key was marked as being used
  // in testing. In that case, the previous testing instance may have erased the
  // entry in the map, and we don't expect a second erasure.
  CHECK(erased) << "Erasing invalid data for key: " << key;
}

void* UnownedUserDataHost::GetImpl(UntypedKey key) {
  auto iter = map_.find(key);
  return iter == map_.end() ? nullptr : iter->second;
}

}  // namespace ui
