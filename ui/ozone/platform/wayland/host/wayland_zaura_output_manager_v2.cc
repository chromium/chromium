// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zaura_output_manager_v2.h"

#include <components/exo/wayland/protocol/aura-output-management-client-protocol.h>

#include <algorithm>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "ui/base/wayland/wayland_display_util.h"
#include "ui/display/screen.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"

namespace ui {

namespace {
constexpr uint32_t kMinVersion = 1;
constexpr uint32_t kMaxVersion = 1;
}  // namespace

// static
constexpr char WaylandZAuraOutputManagerV2::kInterfaceName[];

// static
void WaylandZAuraOutputManagerV2::Instantiate(WaylandConnection* connection,
                                              wl_registry* registry,
                                              uint32_t name,
                                              const std::string& interface,
                                              uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->zaura_output_manager_v2_ ||
      !wl::CanBind(interface, version, kMinVersion, kMaxVersion)) {
    return;
  }

  auto output_manager = wl::Bind<struct zaura_output_manager_v2>(
      registry, name, std::min(version, kMaxVersion));
  if (!output_manager) {
    return;
  }
  connection->zaura_output_manager_v2_ =
      std::make_unique<WaylandZAuraOutputManagerV2>(output_manager.release(),
                                                    connection);
}

WaylandZAuraOutputManagerV2::WaylandZAuraOutputManagerV2(
    zaura_output_manager_v2* output_manager,
    WaylandConnection* connection)
    : obj_(output_manager), connection_(connection) {
  DCHECK(obj_);
  DCHECK(connection_);
  static constexpr zaura_output_manager_v2_listener kAuraOutputManagerListener =
      {.done = &OnDone,
       .display_id = &OnDisplayId,
       .logical_position = &OnLogicalPosition,
       .logical_size = &OnLogicalSize,
       .physical_size = &OnPhysicalSize,
       .work_area_insets = &OnWorkAreaInsets,
       .device_scale_factor = &OnDeviceScaleFactor,
       .logical_transform = &OnLogicalTransform,
       .panel_transform = &OnPanelTransform,
       .name = &OnName,
       .description = &OnDescription,
       .overscan_insets = &OnOverscanInsets,
       .activated = &OnActivated};
  zaura_output_manager_v2_add_listener(obj_.get(), &kAuraOutputManagerListener,
                                       this);
}

WaylandZAuraOutputManagerV2::~WaylandZAuraOutputManagerV2() = default;

void WaylandZAuraOutputManagerV2::ScheduleRemoveWaylandOutput(
    WaylandOutput::Id output_id) {
  pending_removed_outputs_.push_back(output_id);
}

void WaylandZAuraOutputManagerV2::DumpState(std::ostream& out) const {
  out << "AuraOutputManagerv2:" << std::endl;
  int i = 0;
  for (const auto& pair : pending_output_metrics_map_) {
    out << "  pending output metrics[" << i++ << "]:";
    pair.second.DumpState(out);
    out << std::endl;
  }
  i = 0;
  for (const auto& pair : output_metrics_map_) {
    out << "  output metrics[" << i++ << "]:";
    pair.second.DumpState(out);
    out << std::endl;
  }

  std::vector<std::string> values;
  std::transform(pending_outputs_.begin(), pending_outputs_.end(),
                 values.begin(),
                 static_cast<std::string (*)(int)>(base::NumberToString));
  out << "  pending_changed outputs: [" << base::JoinString(values, ",") << "]"
      << std::endl;

  values.clear();
  std::transform(pending_removed_outputs_.begin(),
                 pending_removed_outputs_.end(), values.begin(),
                 static_cast<std::string (*)(int)>(base::NumberToString));
  out << "  pending_removed outputs: [" << base::JoinString(values, ",") << "]"
      << std::endl;
}

WaylandOutput* WaylandZAuraOutputManagerV2::GetWaylandOutput(
    WaylandOutput::Id output_id) {
  WaylandOutputManager* output_manager = connection_->wayland_output_manager();
  CHECK(output_manager);
  return output_manager->GetOutput(output_id);
}

bool WaylandZAuraOutputManagerV2::IsReady(WaylandOutput::Id output_id) const {
  return base::Contains(output_metrics_map_, output_id);
}

void WaylandZAuraOutputManagerV2::RemoveOutput(WaylandOutput::Id output_id) {
  pending_output_metrics_map_.erase(output_id);
  output_metrics_map_.erase(output_id);
}

// static
void WaylandZAuraOutputManagerV2::OnDone(
    void* data,
    zaura_output_manager_v2* output_manager) {
  auto* self = static_cast<WaylandZAuraOutputManagerV2*>(data);

  // Copy over updated metrics for all outputs involved in the current
  // transaction.
  for (const WaylandOutput::Id pending_output_id : self->pending_outputs_) {
    self->output_metrics_map_[pending_output_id] =
        self->pending_output_metrics_map_.at(pending_output_id);

    auto* wayland_output = self->GetWaylandOutput(pending_output_id);
    CHECK(wayland_output);
    wayland_output->SetMetrics(self->output_metrics_map_[pending_output_id]);
  }

  // Once all added / updated output state has been applied propagate the
  // relevant delegate notifications.
  for (const WaylandOutput::Id pending_output_id : self->pending_outputs_) {
    auto* wayland_output = self->GetWaylandOutput(pending_output_id);
    CHECK(wayland_output);
    wayland_output->TriggerDelegateNotifications();
  }

  // Process removed outputs as the last step to ensure the >1 display invariant
  // is maintained.
  WaylandOutputManager* wayland_output_manager =
      self->connection_->wayland_output_manager();
  CHECK(wayland_output_manager);
  for (const WaylandOutput::Id removed_output_id :
       self->pending_removed_outputs_) {
    wayland_output_manager->RemoveWaylandOutput(removed_output_id);
    self->RemoveOutput(removed_output_id);
  }

  self->pending_outputs_.clear();
  self->pending_removed_outputs_.clear();
}

// static
void WaylandZAuraOutputManagerV2::OnDisplayId(
    void* data,
    zaura_output_manager_v2* output_manager,
    uint32_t output_id,
    uint32_t display_id_hi,
    uint32_t display_id_lo) {
  auto* self = static_cast<WaylandZAuraOutputManagerV2*>(data);
  auto& pending_metrics = self->pending_output_metrics_map_[output_id];
  pending_metrics.output_id = output_id;
  pending_metrics.display_id =
      ui::wayland::FromWaylandDisplayIdPair({display_id_hi, display_id_lo});
  self->pending_outputs_.insert(output_id);
}

// static
void WaylandZAuraOutputManagerV2::OnLogicalPosition(
    void* data,
    zaura_output_manager_v2* output_manager,
    uint32_t output_id,
    int32_t x,
    int32_t y) {
  auto* self = static_cast<WaylandZAuraOutputManagerV2*>(data);
  auto& pending_metrics = self->pending_output_metrics_map_[output_id];
  pending_metrics.output_id = output_id;
  pending_metrics.origin.SetPoint(x, y);
  self->pending_outputs_.insert(output_id);
}

// static
void WaylandZAuraOutputManagerV2::OnLogicalSize(
    void* data,
    zaura_output_manager_v2* output_manager,
    uint32_t output_id,
    int32_t width,
    int32_t height) {
  auto* self = static_cast<WaylandZAuraOutputManagerV2*>(data);
  auto& pending_metrics = self->pending_output_metrics_map_[output_id];
  pending_metrics.output_id = output_id;
  pending_metrics.logical_size.SetSize(width, height);
  self->pending_outputs_.insert(output_id);
}

// static
void WaylandZAuraOutputManagerV2::OnPhysicalSize(
    void* data,
    zaura_output_manager_v2* output_manager,
    uint32_t output_id,
    int32_t width,
    int32_t height) {
  auto* self = static_cast<WaylandZAuraOutputManagerV2*>(data);
  auto& pending_metrics = self->pending_output_metrics_map_[output_id];
  pending_metrics.output_id = output_id;
  pending_metrics.physical_size.SetSize(width, height);
  self->pending_outputs_.insert(output_id);
}

// static
void WaylandZAuraOutputManagerV2::OnWorkAreaInsets(
    void* data,
    zaura_output_manager_v2* output_manager,
    uint32_t output_id,
    int32_t top,
    int32_t left,
    int32_t bottom,
    int32_t right) {
  auto* self = static_cast<WaylandZAuraOutputManagerV2*>(data);
  auto& pending_metrics = self->pending_output_metrics_map_[output_id];
  pending_metrics.output_id = output_id;
  pending_metrics.insets = gfx::Insets::TLBR(top, left, bottom, right);
  self->pending_outputs_.insert(output_id);
}

// static
void WaylandZAuraOutputManagerV2::OnDeviceScaleFactor(
    void* data,
    zaura_output_manager_v2* output_manager,
    uint32_t output_id,
    uint32_t scale_as_uint) {
  auto* self = static_cast<WaylandZAuraOutputManagerV2*>(data);
  auto& pending_metrics = self->pending_output_metrics_map_[output_id];
  pending_metrics.output_id = output_id;
  pending_metrics.scale_factor = base::bit_cast<float>(scale_as_uint);
  self->pending_outputs_.insert(output_id);
}

// static
void WaylandZAuraOutputManagerV2::OnLogicalTransform(
    void* data,
    zaura_output_manager_v2* output_manager,
    uint32_t output_id,
    int32_t transform) {
  auto* self = static_cast<WaylandZAuraOutputManagerV2*>(data);
  auto& pending_metrics = self->pending_output_metrics_map_[output_id];
  pending_metrics.output_id = output_id;
  pending_metrics.logical_transform = transform;
  self->pending_outputs_.insert(output_id);
}

// static
void WaylandZAuraOutputManagerV2::OnPanelTransform(
    void* data,
    zaura_output_manager_v2* output_manager,
    uint32_t output_id,
    int32_t transform) {
  auto* self = static_cast<WaylandZAuraOutputManagerV2*>(data);
  auto& pending_metrics = self->pending_output_metrics_map_[output_id];
  pending_metrics.output_id = output_id;
  pending_metrics.panel_transform = transform;
  self->pending_outputs_.insert(output_id);
}

// static
void WaylandZAuraOutputManagerV2::OnName(
    void* data,
    zaura_output_manager_v2* output_manager,
    uint32_t output_id,
    const char* name) {
  auto* self = static_cast<WaylandZAuraOutputManagerV2*>(data);
  auto& pending_metrics = self->pending_output_metrics_map_[output_id];
  pending_metrics.output_id = output_id;
  pending_metrics.name = name;
  self->pending_outputs_.insert(output_id);
}

// static
void WaylandZAuraOutputManagerV2::OnDescription(
    void* data,
    zaura_output_manager_v2* output_manager,
    uint32_t output_id,
    const char* description) {
  auto* self = static_cast<WaylandZAuraOutputManagerV2*>(data);
  auto& pending_metrics = self->pending_output_metrics_map_[output_id];
  pending_metrics.output_id = output_id;
  pending_metrics.description = description;
  self->pending_outputs_.insert(output_id);
}

// static
void WaylandZAuraOutputManagerV2::OnOverscanInsets(
    void* data,
    zaura_output_manager_v2* output_manager,
    uint32_t output_id,
    int32_t top,
    int32_t left,
    int32_t bottom,
    int32_t right) {
  auto* self = static_cast<WaylandZAuraOutputManagerV2*>(data);
  auto& pending_metrics = self->pending_output_metrics_map_[output_id];
  pending_metrics.output_id = output_id;
  pending_metrics.physical_overscan_insets =
      gfx::Insets::TLBR(top, left, bottom, right);
  self->pending_outputs_.insert(output_id);
}

// static
void WaylandZAuraOutputManagerV2::OnActivated(
    void* data,
    zaura_output_manager_v2* output_manager,
    uint32_t output_id) {
  const auto* self = static_cast<WaylandZAuraOutputManagerV2*>(data);
  display::Screen::GetScreen()->SetDisplayForNewWindows(
      self->output_metrics_map_.at(output_id).display_id);
}

}  // namespace ui
