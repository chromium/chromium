// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_surface.h"

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
  GetUserDataAs<MockSurface>(resource)->SetOpaqueRegion(region);
}

void SetInputRegion(wl_client* client,
                    wl_resource* resource,
                    wl_resource* region) {
  GetUserDataAs<MockSurface>(resource)->SetInputRegion(region);
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
  GetUserDataAs<MockSurface>(resource)->SetBufferScale(scale);
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
    DestroyResource,  // destroy
    Attach,           // attach
    Damage,           // damage
    Frame,            // frame
    SetOpaqueRegion,  // set_opaque_region
    SetInputRegion,   // set_input_region
    Commit,           // commit
    nullptr,          // set_buffer_transform
    SetBufferScale,   // set_buffer_scale
    DamageBuffer,     // damage_buffer
};

MockSurface::MockSurface(wl_resource* resource) : ServerObject(resource) {}

MockSurface::~MockSurface() {
  if (xdg_surface_ && xdg_surface_->resource())
    wl_resource_destroy(xdg_surface_->resource());
}

MockSurface* MockSurface::FromResource(wl_resource* resource) {
  if (!ResourceHasImplementation(resource, &wl_surface_interface,
                                 &kMockSurfaceImpl))
    return nullptr;
  return GetUserDataAs<MockSurface>(resource);
}

void MockSurface::AttachNewBuffer(wl_resource* buffer_resource,
                                  int32_t x,
                                  int32_t y) {
  if (attached_buffer_) {
    DCHECK(!prev_attached_buffer_);
    prev_attached_buffer_ = attached_buffer_;
  }
  attached_buffer_ = buffer_resource;

  Attach(buffer_resource, x, y);
}

void MockSurface::ReleasePrevAttachedBuffer() {
  if (!prev_attached_buffer_)
    return;

  wl_buffer_send_release(prev_attached_buffer_);
  wl_client_flush(wl_resource_get_client(prev_attached_buffer_));
  prev_attached_buffer_ = nullptr;
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
