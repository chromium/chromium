// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zaura_output_manager.h"

#include <components/exo/wayland/protocol/aura-shell-client-protocol.h>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "ui/base/wayland/wayland_display_util.h"
#include "ui/display/screen.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"

namespace ui {

namespace {
// To support all existing use cases of zaura_output and xdg_output,
// zaura_output_manager must be version 2+.
constexpr uint32_t kMinVersion = 2;
constexpr uint32_t kMaxVersion = 2;
}  // namespace

// static
constexpr char WaylandZAuraOutputManager::kInterfaceName[];

// static
void WaylandZAuraOutputManager::Instantiate(WaylandConnection* connection,
                                            wl_registry* registry,
                                            uint32_t name,
                                            const std::string& interface,
                                            uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->zaura_output_manager_ ||
      !wl::CanBind(interface, version, kMinVersion, kMaxVersion)) {
    return;
  }

  auto output_manager = wl::Bind<struct zaura_output_manager>(
      registry, name, std::min(version, kMaxVersion));
  if (!output_manager) {
    return;
  }
  connection->zaura_output_manager_ =
      std::make_unique<WaylandZAuraOutputManager>(output_manager.release(),
                                                  connection);
}

WaylandZAuraOutputManager::WaylandZAuraOutputManager(
    zaura_output_manager* output_manager,
    WaylandConnection* connection)
    : obj_(output_manager), connection_(connection) {
  DCHECK(obj_);
  DCHECK(connection_);

  static constexpr zaura_output_manager_listener zaura_output_manager_listener =
      {&OnDone,
       &OnDisplayId,
       &OnLogicalPosition,
       &OnLogicalSize,
       &OnPhysicalSize,
       &OnInsets,
       &OnDeviceScaleFactor,
       &OnLogicalTransform,
       &OnPanelTransform,
       &OnName,
       &OnDescription,
       &OnActivated,
       &OnOverscanInsets};
  zaura_output_manager_add_listener(obj_.get(), &zaura_output_manager_listener,
                                    this);
}

WaylandZAuraOutputManager::~WaylandZAuraOutputManager() = default;

const WaylandOutput::Metrics* WaylandZAuraOutputManager::GetOutputMetrics(
    WaylandOutput::Id output_id) const {
  // Only return the output metrics if all state has arrived from the server.
  return IsReady(output_id) ? &output_metrics_map_.at(output_id) : nullptr;
}

void WaylandZAuraOutputManager::RemoveOutputMetrics(
    WaylandOutput::Id output_id) {
  pending_output_metrics_map_.erase(output_id);
  output_metrics_map_.erase(output_id);
}

WaylandOutput::Id WaylandZAuraOutputManager::GetId(wl_output* output) const {
  WaylandOutputManager* output_manager = connection_->wayland_output_manager();
  // The WaylandOutputManager should have been instantiated when the first
  // wl_output was bound.
  DCHECK(output_manager);
  return output_manager->GetOutputId(output);
}

WaylandOutput* WaylandZAuraOutputManager::GetWaylandOutput(
    WaylandOutput::Id output_id) {
  WaylandOutputManager* output_manager = connection_->wayland_output_manager();
  CHECK(output_manager);
  return output_manager->GetOutput(output_id);
}

bool WaylandZAuraOutputManager::IsReady(WaylandOutput::Id output_id) const {
  return base::Contains(output_metrics_map_, output_id);
}

// static
void WaylandZAuraOutputManager::OnDone(void* data,
                                       zaura_output_manager* output_manager,
                                       wl_output* output) {
  auto* self = static_cast<WaylandZAuraOutputManager*>(data);
  const WaylandOutput::Id output_id = self->GetId(output);
  self->pending_output_metrics_map_[output_id].output_id = output_id;

  self->output_metrics_map_[output_id] =
      self->pending_output_metrics_map_[output_id];

  if (auto* wayland_output = self->GetWaylandOutput(output_id)) {
    // Update the metrics on the corresponding WaylandOutput.
    wayland_output->SetMetrics(self->output_metrics_map_[output_id]);

    // TODO(tluk): In the case of multiple outputs we should wait until we
    // receive state updates for all outputs before propagating notifications.
    wayland_output->TriggerDelegateNotifications();
  }
}

// static
void WaylandZAuraOutputManager::OnDisplayId(
    void* data,
    zaura_output_manager* output_manager,
    wl_output* output,
    uint32_t display_id_hi,
    uint32_t display_id_lo) {
  auto* self = static_cast<WaylandZAuraOutputManager*>(data);
  self->pending_output_metrics_map_[self->GetId(output)].display_id =
      ui::wayland::FromWaylandDisplayIdPair({display_id_hi, display_id_lo});
}

// static
void WaylandZAuraOutputManager::OnLogicalPosition(
    void* data,
    zaura_output_manager* output_manager,
    wl_output* output,
    int32_t x,
    int32_t y) {
  auto* self = static_cast<WaylandZAuraOutputManager*>(data);
  self->pending_output_metrics_map_[self->GetId(output)].origin.SetPoint(x, y);
}

// static
void WaylandZAuraOutputManager::OnLogicalSize(
    void* data,
    zaura_output_manager* output_manager,
    wl_output* output,
    int32_t width,
    int32_t height) {
  auto* self = static_cast<WaylandZAuraOutputManager*>(data);
  self->pending_output_metrics_map_[self->GetId(output)].logical_size.SetSize(
      width, height);
}

// static
void WaylandZAuraOutputManager::OnPhysicalSize(
    void* data,
    zaura_output_manager* output_manager,
    wl_output* output,
    int32_t width,
    int32_t height) {
  auto* self = static_cast<WaylandZAuraOutputManager*>(data);
  self->pending_output_metrics_map_[self->GetId(output)].physical_size.SetSize(
      width, height);
}

// static
void WaylandZAuraOutputManager::OnInsets(void* data,
                                         zaura_output_manager* output_manager,
                                         wl_output* output,
                                         int32_t top,
                                         int32_t left,
                                         int32_t bottom,
                                         int32_t right) {
  auto* self = static_cast<WaylandZAuraOutputManager*>(data);
  self->pending_output_metrics_map_[self->GetId(output)].insets =
      gfx::Insets::TLBR(top, left, bottom, right);
}

// static
void WaylandZAuraOutputManager::OnDeviceScaleFactor(
    void* data,
    zaura_output_manager* output_manager,
    wl_output* output,
    uint32_t scale_as_uint) {
  auto* self = static_cast<WaylandZAuraOutputManager*>(data);
  self->pending_output_metrics_map_[self->GetId(output)].scale_factor =
      base::bit_cast<float>(scale_as_uint);
}

// static
void WaylandZAuraOutputManager::OnLogicalTransform(
    void* data,
    zaura_output_manager* output_manager,
    wl_output* output,
    int32_t transform) {
  auto* self = static_cast<WaylandZAuraOutputManager*>(data);
  self->pending_output_metrics_map_[self->GetId(output)].logical_transform =
      transform;
}

// static
void WaylandZAuraOutputManager::OnPanelTransform(
    void* data,
    zaura_output_manager* output_manager,
    wl_output* output,
    int32_t transform) {
  auto* self = static_cast<WaylandZAuraOutputManager*>(data);
  self->pending_output_metrics_map_[self->GetId(output)].panel_transform =
      transform;
}

// static
void WaylandZAuraOutputManager::OnName(void* data,
                                       zaura_output_manager* output_manager,
                                       wl_output* output,
                                       const char* name) {
  auto* self = static_cast<WaylandZAuraOutputManager*>(data);
  self->pending_output_metrics_map_[self->GetId(output)].name = name;
}

// static
void WaylandZAuraOutputManager::OnDescription(
    void* data,
    zaura_output_manager* output_manager,
    wl_output* output,
    const char* description) {
  auto* self = static_cast<WaylandZAuraOutputManager*>(data);
  self->pending_output_metrics_map_[self->GetId(output)].description =
      description;
}

// static
void WaylandZAuraOutputManager::OnActivated(
    void* data,
    zaura_output_manager* output_manager,
    wl_output* output) {
  CHECK(display::Screen::GetScreen());

  const auto* self = static_cast<WaylandZAuraOutputManager*>(data);
  const WaylandOutput::Id output_id = self->GetId(output);
  display::Screen::GetScreen()->SetDisplayForNewWindows(
      self->GetOutputMetrics(output_id)->display_id);
}

// static
void WaylandZAuraOutputManager::OnOverscanInsets(
    void* data,
    zaura_output_manager* output_manager,
    wl_output* output,
    int32_t top,
    int32_t left,
    int32_t bottom,
    int32_t right) {
  auto* self = static_cast<WaylandZAuraOutputManager*>(data);
  self->pending_output_metrics_map_[self->GetId(output)]
      .physical_overscan_insets = gfx::Insets::TLBR(top, left, bottom, right);
}

}  // namespace ui
