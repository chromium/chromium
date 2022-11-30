// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_output.h"

#include <aura-shell-client-protocol.h>
#include <chrome-color-management-client-protocol.h>
#include <xdg-output-unstable-v1-client-protocol.h>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "ui/display/display.h"
#include "ui/gfx/color_space.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_zaura_output.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_management_output.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_manager.h"
#include "ui/ozone/platform/wayland/host/xdg_output.h"

namespace ui {

namespace {
constexpr uint32_t kMinVersion = 2;
#if CHROME_WAYLAND_CHECK_VERSION(1, 20, 0)
constexpr uint32_t kMaxVersion = 4;
#else
constexpr uint32_t kMaxVersion = 2;
#endif
}  // namespace

// static
constexpr char WaylandOutput::kInterfaceName[];

// static
void WaylandOutput::Instantiate(WaylandConnection* connection,
                                wl_registry* registry,
                                uint32_t name,
                                const std::string& interface,
                                uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (!wl::CanBind(interface, version, kMinVersion, kMaxVersion)) {
    return;
  }

  auto output =
      wl::Bind<wl_output>(registry, name, std::min(version, kMaxVersion));
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

WaylandOutput::WaylandOutput(Id output_id,
                             wl_output* output,
                             WaylandConnection* connection)
    : output_id_(output_id), output_(output), connection_(connection) {
  wl_output_set_user_data(output_.get(), this);
}

WaylandOutput::~WaylandOutput() {
  wl_output_set_user_data(output_.get(), nullptr);
}

void WaylandOutput::InitializeXdgOutput(
    zxdg_output_manager_v1* xdg_output_manager) {
  DCHECK(!xdg_output_);
  xdg_output_ = std::make_unique<XDGOutput>(
      zxdg_output_manager_v1_get_xdg_output(xdg_output_manager, output_.get()));
}

void WaylandOutput::InitializeZAuraOutput(zaura_shell* aura_shell) {
  DCHECK(!aura_output_);
  aura_output_ = std::make_unique<WaylandZAuraOutput>(
      zaura_shell_get_aura_output(aura_shell, output_.get()));
}

void WaylandOutput::InitializeColorManagementOutput(
    WaylandZcrColorManager* zcr_color_manager) {
  DCHECK(!color_management_output_);
  color_management_output_ = std::make_unique<WaylandZcrColorManagementOutput>(
      zcr_color_manager->CreateColorManagementOutput(output_.get()).release());
}

void WaylandOutput::Initialize(Delegate* delegate) {
  DCHECK(!delegate_);
  delegate_ = delegate;
  static constexpr wl_output_listener output_listener = {
    &OutputHandleGeometry,
    &OutputHandleMode,
    &OutputHandleDone,
    &OutputHandleScale,
#if CHROME_WAYLAND_CHECK_VERSION(1, 20, 0)
    // since protocol version 4 and Wayland version 1.20
    &OutputHandleName,
    &OutputHandleDescription,
#endif
  };
  wl_output_add_listener(output_.get(), &output_listener, this);
}

float WaylandOutput::GetUIScaleFactor() const {
  return display::Display::HasForceDeviceScaleFactor()
             ? display::Display::GetForcedDeviceScaleFactor()
             : scale_factor();
}

int32_t WaylandOutput::logical_transform() const {
  if (aura_output_ && aura_output_->logical_transform()) {
    return *aura_output_->logical_transform();
  }
  return panel_transform();
}

gfx::Point WaylandOutput::origin() const {
  if (xdg_output_ && xdg_output_->logical_position()) {
    return *xdg_output_->logical_position();
  }
  return origin_;
}

gfx::Size WaylandOutput::logical_size() const {
  return xdg_output_ ? xdg_output_->logical_size() : gfx::Size();
}

gfx::Insets WaylandOutput::insets() const {
  return aura_output_ ? aura_output_->insets() : gfx::Insets();
}

const std::string& WaylandOutput::description() const {
  return xdg_output_ ? xdg_output_->description() : description_;
}

const std::string& WaylandOutput::name() const {
  return xdg_output_ ? xdg_output_->name() : name_;
}

zaura_output* WaylandOutput::get_zaura_output() {
  return aura_output_ ? aura_output_->wl_object() : nullptr;
}

void WaylandOutput::SetScaleFactorForTesting(float scale_factor) {
  scale_factor_ = scale_factor;
}

void WaylandOutput::TriggerDelegateNotifications() {
  if (xdg_output_ && connection_->surface_submission_in_pixel_coordinates()) {
    DCHECK(!physical_size_.IsEmpty());
    const gfx::Size logical_size = xdg_output_->logical_size();
    if (!logical_size.IsEmpty()) {
      // We calculate the fractional scale factor from the long sides of the
      // physical and logical sizes, since their orientations may be different.
      const float max_physical_side =
          std::max(physical_size_.width(), physical_size_.height());
      const float max_logical_side =
          std::max(logical_size.width(), logical_size.height());
      scale_factor_ = max_physical_side / max_logical_side;
    }
  }
  delegate_->OnOutputHandleMetrics(
      output_id_, origin(), logical_size(), physical_size_, insets(),
      scale_factor_, panel_transform_, logical_transform(), description());
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
    // It looks like there is a bug in libffi - only the 8th arg is affected.
    // Possibly it is not following the calling convention of the ABI? Eg. the
    // lib has some off-by-1-error where it's supposed to pass 8 args in regs
    // and the rest on the stack but instead it's passing 7 in regs. This is
    // out of our control. Given the output_transform is always correct,
    // unpoison the value to make MSAN happy.
    MSAN_UNPOISON(&output_transform, sizeof(int32_t));
    wayland_output->origin_ = gfx::Point(x, y);
    wayland_output->panel_transform_ = output_transform;
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
    wayland_output->physical_size_ = gfx::Size(width, height);
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

#if CHROME_WAYLAND_CHECK_VERSION(1, 20, 0)

// static
void WaylandOutput::OutputHandleName(void* data,
                                     struct wl_output* wl_output,
                                     const char* name) {
  if (WaylandOutput* wayland_output = static_cast<WaylandOutput*>(data))
    wayland_output->name_ = name ? std::string(name) : std::string{};
}

// static
void WaylandOutput::OutputHandleDescription(void* data,
                                            struct wl_output* wl_output,
                                            const char* description) {
  if (WaylandOutput* wayland_output = static_cast<WaylandOutput*>(data)) {
    wayland_output->description_ =
        description ? std::string(description) : std::string{};
  }
}

#endif

}  // namespace ui
