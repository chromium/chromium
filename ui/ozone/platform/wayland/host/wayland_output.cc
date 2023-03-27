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
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_zaura_output.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_management_output.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_manager.h"
#include "ui/ozone/platform/wayland/host/xdg_output.h"

namespace ui {

namespace {
constexpr uint32_t kMinVersion = 2;
constexpr uint32_t kMaxVersion = 4;
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

  if (!connection->output_manager_) {
    connection->output_manager_ =
        std::make_unique<WaylandOutputManager>(connection);
  }
  connection->output_manager_->AddWaylandOutput(name, output.release());
}

WaylandOutput::Metrics::Metrics() = default;
WaylandOutput::Metrics::Metrics(Id output_id,
                                int64_t display_id,
                                gfx::Point origin,
                                gfx::Size logical_size,
                                gfx::Size physical_size,
                                gfx::Insets insets,
                                float scale_factor,
                                int32_t panel_transform,
                                int32_t logical_transform,
                                const std::string& description)
    : output_id(output_id),
      display_id(display_id),
      origin(origin),
      logical_size(logical_size),
      physical_size(physical_size),
      insets(insets),
      scale_factor(scale_factor),
      panel_transform(panel_transform),
      logical_transform(logical_transform),
      description(description) {}
WaylandOutput::Metrics::Metrics(const Metrics& copy) = default;
WaylandOutput::Metrics::~Metrics() = default;

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
      this,
      zcr_color_manager->CreateColorManagementOutput(output_.get()).release());
}

void WaylandOutput::Initialize(Delegate* delegate) {
  DCHECK(!delegate_);
  delegate_ = delegate;
  static constexpr wl_output_listener output_listener = {
      &OutputHandleGeometry, &OutputHandleMode, &OutputHandleDone,
      &OutputHandleScale,    &OutputHandleName, &OutputHandleDescription,

  };
  wl_output_add_listener(output_.get(), &output_listener, this);
}

float WaylandOutput::GetUIScaleFactor() const {
  return display::Display::HasForceDeviceScaleFactor()
             ? display::Display::GetForcedDeviceScaleFactor()
             : scale_factor();
}

WaylandOutput::Metrics WaylandOutput::GetMetrics() const {
  // TODO(aluh): Change to designated initializers once C++20 is supported.
  return {output_id(),         display_id(), origin(),       logical_size(),
          physical_size(),     insets(),     scale_factor(), panel_transform(),
          logical_transform(), description()};
}

float WaylandOutput::scale_factor() const {
  if (xdg_output_ && xdg_output_->IsReady()) {
    const gfx::Size output_logical_size = logical_size();
    const bool can_calculate_scale =
        connection_->surface_submission_in_pixel_coordinates() &&
        !output_logical_size.IsEmpty();

    if (can_calculate_scale) {
      const gfx::Size output_physical_size = physical_size();
      DCHECK(!output_physical_size.IsEmpty());
      const float max_physical_side =
          std::max(output_physical_size.width(), output_physical_size.height());
      const float max_logical_side =
          std::max(output_logical_size.width(), output_logical_size.height());
      return max_physical_side / max_logical_side;
    }
  }

  // If xdg output is defined and ready but we still can't calculate the
  // scale factor, fall back to the scale factor sent in wl_output.scale.
  return scale_factor_;
}

int32_t WaylandOutput::panel_transform() const {
  return panel_transform_;
}

int32_t WaylandOutput::logical_transform() const {
  return aura_output_ && aura_output_->IsReady()
             ? aura_output_->logical_transform().value()
             : panel_transform();
}

gfx::Point WaylandOutput::origin() const {
  return xdg_output_ && xdg_output_->IsReady()
             ? xdg_output_->logical_position().value()
             : origin_;
}

gfx::Size WaylandOutput::logical_size() const {
  return xdg_output_ && xdg_output_->IsReady() ? xdg_output_->logical_size()
                                               : gfx::Size();
}

gfx::Size WaylandOutput::physical_size() const {
  return physical_size_;
}

gfx::Insets WaylandOutput::insets() const {
  return aura_output_ && aura_output_->IsReady() ? aura_output_->insets()
                                                 : gfx::Insets();
}

const std::string& WaylandOutput::description() const {
  // Description is an optional xdg_output event.
  return xdg_output_ && xdg_output_->IsReady() &&
                 !xdg_output_->description().empty()
             ? xdg_output_->description()
             : description_;
}

int64_t WaylandOutput::display_id() const {
  // For the non-aura case we map the global output "name" to the display_id.
  return aura_output_ && aura_output_->IsReady()
             ? aura_output_->display_id().value()
             : output_id_;
}

const std::string& WaylandOutput::name() const {
  // Name is an optional xdg_output event.
  return xdg_output_ && xdg_output_->IsReady() && !xdg_output_->name().empty()
             ? xdg_output_->name()
             : name_;
}

bool WaylandOutput::IsReady() const {
  // The aura output requires both the logical size and the display ID
  // to become ready. If a client that uses xdg_output but not aura_output
  // needs different condition for readiness, this needs to be updated.
  return is_ready_ &&
         (!aura_output_ ||
          (xdg_output_ && xdg_output_->IsReady() && aura_output_->IsReady()));
}

zaura_output* WaylandOutput::get_zaura_output() {
  return aura_output_ ? aura_output_->wl_object() : nullptr;
}

void WaylandOutput::SetScaleFactorForTesting(float scale_factor) {
  scale_factor_ = scale_factor;
}

void WaylandOutput::TriggerDelegateNotifications() {
  // Wait until the all outputs receives enough information to generate display
  // information.
  if (!IsReady())
    return;

  delegate_->OnOutputHandleMetrics(GetMetrics());
}

// static
void WaylandOutput::OutputHandleGeometry(void* data,
                                         wl_output* obj,
                                         int32_t x,
                                         int32_t y,
                                         int32_t physical_width,
                                         int32_t physical_height,
                                         int32_t subpixel,
                                         const char* make,
                                         const char* model,
                                         int32_t output_transform) {
  if (auto* output = static_cast<WaylandOutput*>(data)) {
    // It looks like there is a bug in libffi - only the 8th arg is affected.
    // Possibly it is not following the calling convention of the ABI? Eg. the
    // lib has some off-by-1-error where it's supposed to pass 8 args in regs
    // and the rest on the stack but instead it's passing 7 in regs. This is
    // out of our control. Given the output_transform is always correct,
    // unpoison the value to make MSAN happy.
    MSAN_UNPOISON(&output_transform, sizeof(int32_t));
    output->origin_ = gfx::Point(x, y);
    output->panel_transform_ = output_transform;
  }
}

// static
void WaylandOutput::OutputHandleMode(void* data,
                                     wl_output* wl_output,
                                     uint32_t flags,
                                     int32_t width,
                                     int32_t height,
                                     int32_t refresh) {
  auto* output = static_cast<WaylandOutput*>(data);
  if (output && (flags & WL_OUTPUT_MODE_CURRENT))
    output->physical_size_ = gfx::Size(width, height);
}

// static
void WaylandOutput::OutputHandleDone(void* data, struct wl_output* wl_output) {
  if (auto* output = static_cast<WaylandOutput*>(data)) {
    output->is_ready_ = true;

    if (auto& xdg_output = output->xdg_output_) {
      xdg_output->OnDone();
    }

    if (auto& aura_output = output->aura_output_) {
      aura_output->OnDone();
    }

    output->TriggerDelegateNotifications();
  }
}

// static
void WaylandOutput::OutputHandleScale(void* data,
                                      struct wl_output* wl_output,
                                      int32_t factor) {
  if (auto* output = static_cast<WaylandOutput*>(data))
    output->scale_factor_ = factor;
}

// static
void WaylandOutput::OutputHandleName(void* data,
                                     struct wl_output* wl_output,
                                     const char* name) {
  if (auto* output = static_cast<WaylandOutput*>(data))
    output->name_ = name ? std::string(name) : std::string{};
}

// static
void WaylandOutput::OutputHandleDescription(void* data,
                                            struct wl_output* wl_output,
                                            const char* description) {
  if (auto* output = static_cast<WaylandOutput*>(data)) {
    output->description_ =
        description ? std::string(description) : std::string{};
  }
}

}  // namespace ui
