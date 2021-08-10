// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_output.h"

#include "base/logging.h"
#include "ui/display/display.h"
#include "ui/gfx/color_space.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"

namespace ui {

namespace {
constexpr uint32_t kMinWlOutputVersion = 2;
}

// static
void WaylandOutput::Register(WaylandConnection* connection) {
  connection->RegisterGlobalObjectFactory("wl_output",
                                          &WaylandOutput::Instantiate);
}

// static
void WaylandOutput::Instantiate(WaylandConnection* connection,
                                wl_registry* registry,
                                uint32_t name,
                                uint32_t version) {
  if (version < kMinWlOutputVersion) {
    LOG(ERROR)
        << "Unable to bind to the unsupported wl_output object with version= "
        << version << ". Minimum supported version is " << kMinWlOutputVersion;
    return;
  }

  auto output = wl::Bind<wl_output>(registry, name, version);
  if (!output) {
    LOG(ERROR) << "Failed to bind to wl_output global";
    return;
  }

  if (!connection->wayland_output_manager_) {
    connection->wayland_output_manager_ =
        std::make_unique<WaylandOutputManager>(connection);
  }
  connection->wayland_output_manager_->AddWaylandOutput(name, output.release());
}

WaylandOutput::WaylandOutput(uint32_t output_id, wl_output* output)
    : output_id_(output_id), output_(output) {
  wl_output_set_user_data(output_.get(), this);
}

WaylandOutput::~WaylandOutput() {
  wl_output_set_user_data(output_.get(), nullptr);
}

void WaylandOutput::Initialize(Delegate* delegate) {
  DCHECK(!delegate_);
  delegate_ = delegate;
  static constexpr wl_output_listener output_listener = {
      &OutputHandleGeometry,
      &OutputHandleMode,
      &OutputHandleDone,
      &OutputHandleScale,
  };
  wl_output_add_listener(output_.get(), &output_listener, this);
}

float WaylandOutput::GetUIScaleFactor() const {
  return display::Display::HasForceDeviceScaleFactor()
             ? display::Display::GetForcedDeviceScaleFactor()
             : scale_factor();
}

void WaylandOutput::TriggerDelegateNotifications() const {
  DCHECK(!rect_in_physical_pixels_.IsEmpty());
  delegate_->OnOutputHandleMetrics(output_id_, rect_in_physical_pixels_,
                                   scale_factor_, transform_);
}

// static
void WaylandOutput::OutputHandleGeometry(void* data,
                                         wl_output* output,
                                         int32_t x,
                                         int32_t y,
                                         int32_t physical_width,
                                         int32_t physical_height,
                                         int32_t subpixel,
                                         const char* make,
                                         const char* model,
                                         int32_t output_transform) {
  WaylandOutput* wayland_output = static_cast<WaylandOutput*>(data);
  if (wayland_output) {
    wayland_output->rect_in_physical_pixels_.set_origin(gfx::Point(x, y));
    wayland_output->transform_ = output_transform;
  }
}

// static
void WaylandOutput::OutputHandleMode(void* data,
                                     wl_output* wl_output,
                                     uint32_t flags,
                                     int32_t width,
                                     int32_t height,
                                     int32_t refresh) {
  WaylandOutput* wayland_output = static_cast<WaylandOutput*>(data);
  if (wayland_output && (flags & WL_OUTPUT_MODE_CURRENT))
    wayland_output->rect_in_physical_pixels_.set_size(gfx::Size(width, height));
}

// static
void WaylandOutput::OutputHandleDone(void* data, struct wl_output* wl_output) {
  if (auto* output = static_cast<WaylandOutput*>(data))
    output->TriggerDelegateNotifications();
}

// static
void WaylandOutput::OutputHandleScale(void* data,
                                      struct wl_output* wl_output,
                                      int32_t factor) {
  WaylandOutput* wayland_output = static_cast<WaylandOutput*>(data);
  if (wayland_output)
    wayland_output->scale_factor_ = factor;
}

}  // namespace ui
