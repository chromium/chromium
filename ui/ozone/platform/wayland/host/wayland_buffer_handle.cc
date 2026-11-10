// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_handle.h"

#include <ostream>

#include "ui/ozone/platform/wayland/host/wayland_buffer_backing.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_syncobj_timeline.h"

namespace ui {
namespace {
WaylandBufferHandle::SyncMethod determine_sync_method(
    WaylandBufferBacking* backing) {
  switch (backing->type()) {
    case WaylandBufferBacking::BufferBackingType::kSinglePixel:
      return WaylandBufferHandle::SyncMethod::kNone;
    case WaylandBufferBacking::BufferBackingType::kShm:
      return WaylandBufferHandle::SyncMethod::kImplicit;
    case WaylandBufferBacking::BufferBackingType::kDmabuf:
      if (backing->connection()->SupportsExplicitSync()) {
        return WaylandBufferHandle::SyncMethod::kSyncobj;
      } else if (backing->connection()->UseImplicitSyncInterop()) {
        return WaylandBufferHandle::SyncMethod::kDMAFence;
      } else {
        return WaylandBufferHandle::SyncMethod::kImplicit;
      }
    default:
      NOTREACHED();
  }
}
}  // namespace

WaylandBufferHandle::WaylandBufferHandle(WaylandBufferBacking* backing)
    : connection_(backing->connection()),
      id_(backing->id()),
      size_(backing->size()),
      sync_method_(determine_sync_method(backing)),
      weak_factory_(this) {}

WaylandBufferHandle::~WaylandBufferHandle() {
  for (auto& cb : released_callbacks_)
    std::move(cb.second).Run(wl_buffer_.get(), /*is_destruct=*/true);
}

void WaylandBufferHandle::OnWlBufferCreated(wl::Object<wl_buffer> wl_buffer) {
  CHECK(wl_buffer) << "Failed to create wl_buffer object.";

  wl_buffer_ = std::move(wl_buffer);

  // Setup buffer release listener callbacks.
  static constexpr wl_buffer_listener kBufferListener = {
      .release = &OnRelease,
  };
  if (sync_method_ == SyncMethod::kSyncobj) {
    release_timeline_ = WaylandSyncobjReleaseTimeline::Create(connection_);
  }
  // kSyncobj buffers also need the listener: until a surface receives its
  // first acquire fence, they are attached without explicit sync and released
  // with wl_buffer.release.
  if (sync_method_ != SyncMethod::kNone) {
    wl_buffer_add_listener(wl_buffer_.get(), &kBufferListener, this);
  }

  if (!created_callback_.is_null())
    std::move(created_callback_).Run();
}

void WaylandBufferHandle::OnExplicitRelease(WaylandSurface* requestor) {
  auto it = released_callbacks_.find(requestor);
  CHECK(it != released_callbacks_.end());
  released_callbacks_.erase(it);
}

void WaylandBufferHandle::OnWlBufferReleased(wl_buffer* wl_buff) {
  CHECK_EQ(wl_buff, wl_buffer_.get(), base::NotFatalUntil::M163);
  CHECK_NE(sync_method_, SyncMethod::kNone, base::NotFatalUntil::M163);
  if (sync_method_ == SyncMethod::kSyncobj && attached_with_explicit_sync_) {
    // The delivery of wl_buffer.release is undefined while explicit sync is
    // used; the release point signals the release instead.
    return;
  }
  CHECK_LE(released_callbacks_.size(), 1u, base::NotFatalUntil::M163);

  if (!released_callbacks_.empty()) {
    std::move(released_callbacks_.begin()->second).Run(wl_buff, false);
  }
  released_callbacks_.clear();
}

base::WeakPtr<WaylandBufferHandle> WaylandBufferHandle::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// static
void WaylandBufferHandle::OnRelease(void* data, wl_buffer* buffer) {
  auto* self = static_cast<WaylandBufferHandle*>(data);
  CHECK(self, base::NotFatalUntil::M163);
  self->OnWlBufferReleased(buffer);
}

}  // namespace ui
