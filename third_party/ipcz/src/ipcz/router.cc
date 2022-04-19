// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/router.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "third_party/abseil-cpp/absl/synchronization/mutex.h"

namespace ipcz {

Router::Router() = default;

Router::~Router() = default;

void Router::QueryStatus(IpczPortalStatus& status) {
  absl::MutexLock lock(&mutex_);
  const size_t size = std::min(status.size, status_.size);
  memcpy(&status, &status_, size);
  status.size = size;
}

void Router::CloseRoute() {
  absl::MutexLock lock(&mutex_);
  outward_link_.reset();
}

void Router::SetOutwardLink(Ref<RouterLink> link) {
  absl::MutexLock lock(&mutex_);
  ABSL_ASSERT(!outward_link_);
  outward_link_ = std::move(link);
}

}  // namespace ipcz
