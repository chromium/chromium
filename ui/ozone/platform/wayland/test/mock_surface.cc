// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_surface.h"

#include "base/notreached.h"
#include "ui/ozone/platform/wayland/test/test_region.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"

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
}

MockSurface* MockSurface::FromResource(wl_resource* resource) {
  if (!ResourceHasImplementation(resource, &wl_surface_interface,
                                 &kMockSurfaceImpl))
    return nullptr;
  return GetUserDataAs<MockSurface>(resource);
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
  DCHECK(buffer);
  wl_buffer_send_release(buffer);
  wl_client_flush(wl_resource_get_client(buffer));

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
  wl_resource_destroy(frame_callback_.ExtractAsDangling());
  frame_callback_ = nullptr;
}

}  // namespace wl
