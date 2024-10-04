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
#include "ui/ozone/platform/wayland/host/dump_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_management_output.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_manager.h"
#include "ui/ozone/platform/wayland/host/xdg_output.h"

namespace ui {

namespace {
constexpr uint32_t kMinVersion = 2;
constexpr uint32_t kMaxVersion = 4;
}  // namespace

using Metrics = WaylandOutput::Metrics;

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

Metrics::Metrics() = default;
Metrics::Metrics(Id output_id,
                 int64_t display_id,
                 gfx::Point origin,
                 gfx::Size logical_size,
                 gfx::Size physical_size,
                 gfx::Insets insets,
                 gfx::Insets physical_overscan_insets,
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
      physical_overscan_insets(physical_overscan_insets),
      scale_factor(scale_factor),
      panel_transform(panel_transform),
      logical_transform(logical_transform),
      description(description) {}
Metrics::Metrics(const Metrics&) = default;
Metrics& Metrics::operator=(const Metrics&) = default;
Metrics::Metrics(Metrics&&) = default;
Metrics& Metrics::operator=(Metrics&&) = default;
Metrics::~Metrics() = default;

void Metrics::DumpState(std::ostream& out) const {
  constexpr auto kTransformMap = base::MakeFixedFlatMap<int32_t, const char*>({
      {WL_OUTPUT_TRANSFORM_NORMAL, "normal"},
      {WL_OUTPUT_TRANSFORM_90, "90"},
      {WL_OUTPUT_TRANSFORM_180, "180"},
      {WL_OUTPUT_TRANSFORM_270, "270"},
      {WL_OUTPUT_TRANSFORM_FLIPPED, "flipped"},
      {WL_OUTPUT_TRANSFORM_FLIPPED_90, "flipped 90"},
      {WL_OUTPUT_TRANSFORM_FLIPPED_180, "flipped 180"},
      {WL_OUTPUT_TRANSFORM_FLIPPED_270, "flipped 270"},
  });

  out << "output_id=" << output_id << ", display_id=" << display_id
      << ", description=" << description << ", origin=" << origin.ToString()
      << ", logical_size=" << logical_size.ToString()
      << ", physicacl_size=" << physical_size.ToString()
      << ", scale_factor=" << scale_factor
      << ", work_area_insets=" << insets.ToString()
      << ", overscacn_insets=" << physical_overscan_insets.ToString()
      << ", panel_transform="
      << GetMapValueOrDefault(kTransformMap, panel_transform)
      << ", logical_transform="
      << GetMapValueOrDefault(kTransformMap, logical_transform);
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
  static constexpr wl_output_listener kOutputListener = {
      .geometry = &OnGeometry,
      .mode = &OnMode,
      .done = &OnDone,
      .scale = &OnScale,
      .name = &OnName,
      .description = &OnDescription,
  };
  wl_output_add_listener(output_.get(), &kOutputListener, this);
}

const Metrics& WaylandOutput::GetMetrics() const {
  return metrics_;
}

void WaylandOutput::SetMetrics(const Metrics& metrics) {
  metrics_ = metrics;
}

float WaylandOutput::scale_factor() const {
  return metrics_.scale_factor;
}

bool WaylandOutput::IsReady() const {
  // zaura_output_manager is guaranteed to have received all relevant output
  // metrics before the first wl_output.done event. zaura_output_manager is
  // responsible for updating `metrics_` in an atomic and consistent way as soon
  // as it receives all its necessary output metrics events.
  if (connection_->IsUsingZAuraOutputManager()) {
    // WaylandOutput should be considered ready after the first atomic update to
    // `metrics_`.
    return metrics_.output_id == output_id_;
  }

  return is_ready_;
}

void WaylandOutput::SetScaleFactorForTesting(float scale_factor) {
  metrics_.scale_factor = scale_factor;
}

void WaylandOutput::TriggerDelegateNotifications() {
  // Wait until the all outputs receives enough information to generate display
  // information.
  if (!IsReady())
    return;

  delegate_->OnOutputHandleMetrics(GetMetrics());
}

void WaylandOutput::DumpState(std::ostream& out) const {
  metrics_.DumpState(out);
}

void WaylandOutput::UpdateMetrics() {
  metrics_.output_id = output_id_;
  // For the non-aura case we map the global output "name" to the display_id.
  metrics_.display_id = output_id_;
  metrics_.origin = origin_;
  metrics_.physical_size = physical_size_;
  metrics_.scale_factor = scale_factor_;
  metrics_.panel_transform = panel_transform_;
  metrics_.logical_transform = panel_transform_;
  metrics_.name = name_;
  metrics_.description = description_;

  if (xdg_output_) {
    xdg_output_->UpdateMetrics(
        connection_->supports_viewporter_surface_scaling(), metrics_);
  }
}

// static
void WaylandOutput::OnGeometry(void* data,
                               wl_output* output,
                               int32_t x,
                               int32_t y,
                               int32_t physical_width,
                               int32_t physical_height,
                               int32_t subpixel,
                               const char* make,
                               const char* model,
                               int32_t output_transform) {
  if (auto* self = static_cast<WaylandOutput*>(data)) {
    // It looks like there is a bug in libffi - only the 8th arg is affected.
    // Possibly it is not following the calling convention of the ABI? Eg. the
    // lib has some off-by-1-error where it's supposed to pass 8 args in regs
    // and the rest on the stack but instead it's passing 7 in regs. This is
    // out of our control. Given the output_transform is always correct,
    // unpoison the value to make MSAN happy.
    MSAN_UNPOISON(&output_transform, sizeof(int32_t));
    self->origin_ = gfx::Point(x, y);
    self->panel_transform_ = output_transform;
  }
}

// static
void WaylandOutput::OnMode(void* data,
                           wl_output* wl_output,
                           uint32_t flags,
                           int32_t width,
                           int32_t height,
                           int32_t refresh) {
  auto* self = static_cast<WaylandOutput*>(data);
  if (self && (flags & WL_OUTPUT_MODE_CURRENT)) {
    self->physical_size_ = gfx::Size(width, height);
  }
}

// static
void WaylandOutput::OnDone(void* data, wl_output* output) {
  auto* self = static_cast<WaylandOutput*>(data);

  // zaura_output_manager takes responsibility of keeping `metrics_` up to date
  // and triggering delegate notifications.
  if (!self || self->connection_->IsUsingZAuraOutputManager()) {
    return;
  }

  self->is_ready_ = true;

  if (auto& xdg_output = self->xdg_output_) {
    xdg_output->HandleDone();
  }

  // Once all metrics have been received perform an atomic update on this
  // output's `metrics_`.
  self->UpdateMetrics();

  self->TriggerDelegateNotifications();
}

// static
void WaylandOutput::OnScale(void* data, wl_output* output, int32_t factor) {
  if (auto* self = static_cast<WaylandOutput*>(data)) {
    self->scale_factor_ = factor;
  }
}

// static
void WaylandOutput::OnName(void* data, wl_output* output, const char* name) {
  if (auto* self = static_cast<WaylandOutput*>(data)) {
    self->name_ = name ? std::string(name) : std::string{};
  }
}

// static
void WaylandOutput::OnDescription(void* data,
                                  wl_output* output,
                                  const char* description) {
  if (auto* self = static_cast<WaylandOutput*>(data)) {
    self->description_ = description ? std::string(description) : std::string{};
  }
}

}  // namespace ui
