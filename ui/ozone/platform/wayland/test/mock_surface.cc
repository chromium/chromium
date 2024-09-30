// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_surface.h"

#include <linux-explicit-synchronization-unstable-v1-client-protocol.h>

#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "ui/ozone/platform/wayland/test/test_region.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/test_zwp_linux_explicit_synchronization.h"

namespace wl {

namespace {

void Attach(wl_client* client,
            wl_resource* resource,
            wl_resource* buffer_resource,
            int32_t x,
            int32_t y) {
  auto* surface = GetUserDataAs<MockSurface>(resource);
  surface->AttachNewBuffer(buffer_resource, x, y);
}

void SetOpaqueRegion(wl_client* client,
                     wl_resource* resource,
                     wl_resource* region) {
  GetUserDataAs<MockSurface>(resource)->SetOpaqueRegionImpl(region);
}

void SetInputRegion(wl_client* client,
                    wl_resource* resource,
                    wl_resource* region) {
  GetUserDataAs<MockSurface>(resource)->SetInputRegionImpl(region);
}

void Damage(wl_client* client,
            wl_resource* resource,
            int32_t x,
            int32_t y,
            int32_t width,
            int32_t height) {
  GetUserDataAs<MockSurface>(resource)->Damage(x, y, width, height);
}

void Frame(struct wl_client* client,
           struct wl_resource* resource,
           uint32_t callback) {
  auto* surface = GetUserDataAs<MockSurface>(resource);

  wl_resource* callback_resource =
      wl_resource_create(client, &wl_callback_interface, 1, callback);
  surface->set_frame_callback(callback_resource);

  surface->Frame(callback);
}

void Commit(wl_client* client, wl_resource* resource) {
  GetUserDataAs<MockSurface>(resource)->Commit();
}

void SetBufferScale(wl_client* client, wl_resource* resource, int32_t scale) {
  auto* mock_surface = GetUserDataAs<MockSurface>(resource);
  mock_surface->SetBufferScale(scale);
  mock_surface->set_buffer_scale(scale);
}

void SetBufferTransform(struct wl_client* client,
                        struct wl_resource* resource,
                        int32_t transform) {
  auto* mock_surface = GetUserDataAs<MockSurface>(resource);
  mock_surface->SetBufferTransform(transform);
}

void DamageBuffer(struct wl_client* client,
                  struct wl_resource* resource,
                  int32_t x,
                  int32_t y,
                  int32_t width,
                  int32_t height) {
  GetUserDataAs<MockSurface>(resource)->DamageBuffer(x, y, width, height);
}

void SetAcquireFence(wl_client* client, wl_resource* resource, int32_t fd) {
  // TODO(crbug.com/40182819): Implement this.
  NOTIMPLEMENTED();
}

void GetRelease(wl_client* client, wl_resource* resource, uint32_t id) {
  auto* linux_buffer_release_resource =
      wl_resource_create(client, &zwp_linux_buffer_release_v1_interface, 1, id);
  auto* linux_surface_synchronization =
      GetUserDataAs<TestLinuxSurfaceSynchronization>(resource);
  auto* surface = GetUserDataAs<MockSurface>(
      linux_surface_synchronization->surface_resource());
  surface->set_linux_buffer_release(surface->attached_buffer(),
                                    linux_buffer_release_resource);
}

}  // namespace

const struct wl_surface_interface kMockSurfaceImpl = {
    DestroyResource,     // destroy
    Attach,              // attach
    Damage,              // damage
    Frame,               // frame
    SetOpaqueRegion,     // set_opaque_region
    SetInputRegion,      // set_input_region
    Commit,              // commit
    SetBufferTransform,  // set_buffer_transform
    SetBufferScale,      // set_buffer_scale
    DamageBuffer,        // damage_buffer
};

const struct zwp_linux_surface_synchronization_v1_interface
    kMockZwpLinuxSurfaceSynchronizationImpl = {
        DestroyResource,
        SetAcquireFence,
        GetRelease,
};

MockSurface::MockSurface(wl_resource* resource) : ServerObject(resource) {}

MockSurface::~MockSurface() {
  if (xdg_surface_ && xdg_surface_->resource())
    wl_resource_destroy(xdg_surface_->resource());
  if (sub_surface_ && sub_surface_->resource())
    wl_resource_destroy(sub_surface_->resource());
  if (viewport_ && viewport_->resource())
    wl_resource_destroy(viewport_->resource());
  if (blending_ && blending_->resource())
    wl_resource_destroy(blending_->resource());
  if (prioritized_surface_ && prioritized_surface_->resource())
    wl_resource_destroy(prioritized_surface_->resource());
  if (augmented_surface_ && augmented_surface_->resource())
    wl_resource_destroy(augmented_surface_->resource());
}

MockSurface* MockSurface::FromResource(wl_resource* resource) {
  if (!ResourceHasImplementation(resource, &wl_surface_interface,
                                 &kMockSurfaceImpl))
    return nullptr;
  return GetUserDataAs<MockSurface>(resource);
}

void MockSurface::ClearBufferReleases() {
  linux_buffer_releases_.clear();
}

void MockSurface::SetOpaqueRegionImpl(wl_resource* region) {
  if (!region) {
    opaque_region_ = gfx::Rect(-1, -1, 0, 0);
    return;
  }
  auto bounds = GetUserDataAs<TestRegion>(region)->getBounds();
  opaque_region_ =
      gfx::Rect(bounds.fLeft, bounds.fTop, bounds.fRight - bounds.fLeft,
                bounds.fBottom - bounds.fTop);

  SetOpaqueRegion(region);
}

void MockSurface::SetInputRegionImpl(wl_resource* region) {
  // It is unsafe to always treat |region| as a valid pointer.
  // According to the protocol about wl_surface::set_input_region
  // "A NULL wl_region cuases the input region to be set to infinite."
  if (!region) {
    input_region_ = gfx::Rect(-1, -1, 0, 0);
    return;
  }
  auto bounds = GetUserDataAs<TestRegion>(region)->getBounds();
  input_region_ =
      gfx::Rect(bounds.fLeft, bounds.fTop, bounds.fRight - bounds.fLeft,
                bounds.fBottom - bounds.fTop);

  SetInputRegion(region);
}

void MockSurface::AttachNewBuffer(wl_resource* buffer_resource,
                                  int32_t x,
                                  int32_t y) {
  if (attached_buffer_)
    prev_attached_buffer_ = attached_buffer_;
  attached_buffer_ = buffer_resource;

  Attach(buffer_resource, x, y);
}

void MockSurface::DestroyPrevAttachedBuffer() {
  DCHECK(prev_attached_buffer_);
  prev_attached_buffer_ = nullptr;
}

void MockSurface::ReleaseBuffer(wl_resource* buffer) {
  // Strictly speaking, Wayland protocol requires that we send both an explicit
  // release and a buffer release if an explicit release has been asked for.
  // But, this makes testing harder, and ozone/wayland should work with
  // just one of these signals (and handle both gracefully).
  // TODO(fangzhoug): Make buffer release mechanism a testing config variation.
  if (linux_buffer_releases_.find(buffer) != linux_buffer_releases_.end()) {
    ReleaseBufferFenced(buffer, {});
    wl_buffer_send_release(buffer);
    wl_client_flush(wl_resource_get_client(buffer));
  }

  DCHECK(buffer);
  wl_buffer_send_release(buffer);
  wl_client_flush(wl_resource_get_client(buffer));

  if (buffer == prev_attached_buffer_)
    prev_attached_buffer_ = nullptr;
  if (buffer == attached_buffer_)
    attached_buffer_ = nullptr;
}

void MockSurface::ReleaseBufferFenced(wl_resource* buffer,
                                      gfx::GpuFenceHandle release_fence) {
  DCHECK(buffer);
  auto iter = linux_buffer_releases_.find(buffer);
  CHECK(iter != linux_buffer_releases_.end(), base::NotFatalUntil::M130);
  auto* linux_buffer_release = iter->second.get();
  if (!release_fence.is_null()) {
    zwp_linux_buffer_release_v1_send_fenced_release(linux_buffer_release,
                                                    release_fence.Peek());
  } else {
    zwp_linux_buffer_release_v1_send_immediate_release(linux_buffer_release);
  }
  wl_client_flush(wl_resource_get_client(linux_buffer_release));
  linux_buffer_releases_.erase(iter);
  if (buffer == prev_attached_buffer_)
    prev_attached_buffer_ = nullptr;
  if (buffer == attached_buffer_)
    attached_buffer_ = nullptr;
}

void MockSurface::SendFrameCallback() {
  if (!frame_callback_)
    return;

  wl_callback_send_done(
      frame_callback_,
      0 /* trequest-specific data for the callback. not used */);
  wl_client_flush(wl_resource_get_client(frame_callback_));
  wl_resource_destroy(frame_callback_);
  frame_callback_ = nullptr;
}

}  // namespace wl
