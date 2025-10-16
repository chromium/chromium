// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MAC_MTL_SHARED_EVENT_FENCE_H_
#define UI_GFX_MAC_MTL_SHARED_EVENT_FENCE_H_

#include <memory>
#include <vector>

#if defined(__OBJC__)
#import <Metal/Metal.h>
#endif

#include "base/component_export.h"

namespace gfx {

// A simple wrapper around a MTLSharedEvent and a fence value that's expected to
// be signaled at some point. Used primarily to export shared event fences from
// gpu::IOSurfaceImageBacking to ui::CALayerTreeCoordinator for backpressure.
class COMPONENT_EXPORT(GFX) MTLSharedEventFence {
 public:
  MTLSharedEventFence();
  ~MTLSharedEventFence();

  MTLSharedEventFence(MTLSharedEventFence&&);
  MTLSharedEventFence& operator=(MTLSharedEventFence&&);

  MTLSharedEventFence(const MTLSharedEventFence&) = delete;
  MTLSharedEventFence& operator=(const MTLSharedEventFence&) = delete;

  // Reduce given `fences` to a minimal set by deduplicating shared events and
  // associating each shared event with its highest fence value from the input.
  static std::vector<MTLSharedEventFence> Reduce(
      std::vector<MTLSharedEventFence> fences);

#if defined(__OBJC__)
  MTLSharedEventFence(id<MTLSharedEvent> shared_event, uint64_t fence_value);

  id<MTLSharedEvent> GetSharedEvent() const;
#endif

  uint64_t fence_value() const { return fence_value_; }

  // Returns true if the MTLSharedEvent `signaledValue` is past `fence_value_`.
  bool HasSignaled() const;

 private:
  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> objc_storage_;

  uint64_t fence_value_ = 0;
};

}  // namespace gfx

#endif  // UI_GFX_MAC_MTL_SHARED_EVENT_FENCE_H_
