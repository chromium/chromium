// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mac/mtl_shared_event_fence.h"

#include "base/check.h"
#include "base/containers/flat_map.h"

namespace gfx {

struct MTLSharedEventFence::ObjCStorage {
  explicit ObjCStorage(id<MTLSharedEvent> shared_event)
      : shared_event(shared_event) {}

  id<MTLSharedEvent> __strong shared_event;
};

MTLSharedEventFence::MTLSharedEventFence() = default;
MTLSharedEventFence::~MTLSharedEventFence() = default;

MTLSharedEventFence::MTLSharedEventFence(MTLSharedEventFence&&) = default;
MTLSharedEventFence& MTLSharedEventFence::operator=(
    MTLSharedEventFence&&) = default;

MTLSharedEventFence::MTLSharedEventFence(id<MTLSharedEvent> shared_event,
                                             uint64_t fence_value)
    : objc_storage_(std::make_unique<ObjCStorage>(shared_event)),
      fence_value_(fence_value) {}

std::vector<MTLSharedEventFence> MTLSharedEventFence::Reduce(
    std::vector<MTLSharedEventFence> fences) {
  std::vector<MTLSharedEventFence> reduced;
  base::flat_map<id<MTLSharedEvent>, uint64_t> shared_events;
  for (const auto& fence : fences) {
    if (id<MTLSharedEvent> shared_event = fence.GetSharedEvent()) {
      shared_events[shared_event] =
          std::max(shared_events[shared_event], fence.fence_value());
    }
  }
  for (const auto& [shared_event, fence_value] : shared_events) {
    reduced.emplace_back(shared_event, fence_value);
  }
  return reduced;
}

id<MTLSharedEvent> MTLSharedEventFence::GetSharedEvent() const {
  CHECK(objc_storage_);
  return objc_storage_->shared_event;
}

bool MTLSharedEventFence::HasSignaled() const {
  if (id<MTLSharedEvent> shared_event = GetSharedEvent()) {
    return shared_event.signaledValue >= fence_value_;
  }
  return true;
}

}  // namespace gfx
