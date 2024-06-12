// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"

#include <cstdint>
#include <memory>
#include <string>

#include "base/ranges/algorithm.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_zaura_output_manager_v2.h"
#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"

namespace ui {

WaylandOutputManager::WaylandOutputManager(WaylandConnection* connection)
    : connection_(connection) {}

WaylandOutputManager::~WaylandOutputManager() = default;

// Output is considered ready when at least one wl_output is fully configured
// (i.e: wl_output::done received), so that WaylandOutputManager is able to
// instantiate a valid WaylandScreen when requested by the upper layer.
bool WaylandOutputManager::IsOutputReady() const {
  for (const auto& it : output_list_) {
    if (it.second->IsReady())
      return true;
  }
  return false;
}

void WaylandOutputManager::AddWaylandOutput(WaylandOutput::Id output_id,
                                            wl_output* output) {
  // Make sure an output with |output_id| has not been added yet. It's very
  // unlikely to happen, unless a compositor has a bug in the numeric names
  // representation of global objects.
  DCHECK(!GetOutput(output_id));
  auto wayland_output =
      std::make_unique<WaylandOutput>(output_id, output, connection_);

  // Even if WaylandScreen has not been created, the output still must be
  // initialized, which results in setting up a wl_listener and getting the
  // geometry and the scaling factor from the Wayland Compositor.
  wayland_output->Initialize(this);

  // If supported, the aura output manager will have have been bound by this
  // client before the any wl_output objects. The aura output manager subsumes
  // the responsibilities of xdg_output and aura_output, so avoid unnecessarily
  // creating the output extensions if present.
  if (!connection_->IsUsingZAuraOutputManager()) {
    if (connection_->xdg_output_manager_v1()) {
      wayland_output->InitializeXdgOutput(connection_->xdg_output_manager_v1());
    }
  }

  // TODO(tluk): Update zaura_output_manager to support the capabilities of
  // zcr_color_management_output.
  if (connection_->zcr_color_manager()) {
    wayland_output->InitializeColorManagementOutput(
        connection_->zcr_color_manager());
  }
  DCHECK(!wayland_output->IsReady());

  output_list_[output_id] = std::move(wayland_output);
}

void WaylandOutputManager::RemoveWaylandOutput(WaylandOutput::Id output_id) {
  // Check the comment in the WaylandConnection::GlobalRemove.
  if (!GetOutput(output_id))
    return;

  // Remove WaylandOutput in following order :
  // 1. from `WaylandSurface::entered_outputs_`
  // 2. from `WaylandScreen::display_list_`
  // 3. from `WaylandZAuraOutputManager::output_metrics_map_`
  // 4. from `WaylandOutputManager::output_list_`
  auto* wayland_window_manager = connection_->window_manager();
  for (auto* window : wayland_window_manager->GetAllWindows())
    window->RemoveEnteredOutput(output_id);

  if (wayland_screen_)
    wayland_screen_->OnOutputRemoved(output_id);
  DCHECK(output_list_.find(output_id) != output_list_.end());

  output_list_.erase(output_id);
}

void WaylandOutputManager::InitializeAllXdgOutputs() {
  DCHECK(connection_->xdg_output_manager_v1());
  for (const auto& output : output_list_)
    output.second->InitializeXdgOutput(connection_->xdg_output_manager_v1());
}

void WaylandOutputManager::InitializeAllColorManagementOutputs() {
  DCHECK(connection_->zcr_color_manager());
  for (const auto& output : output_list_)
    output.second->InitializeColorManagementOutput(
        connection_->zcr_color_manager());
}

std::unique_ptr<WaylandScreen> WaylandOutputManager::CreateWaylandScreen() {
  auto wayland_screen = std::make_unique<WaylandScreen>(connection_);
  wayland_screen_ = wayland_screen->GetWeakPtr();

  return wayland_screen;
}

void WaylandOutputManager::InitWaylandScreen(WaylandScreen* screen) {
  // As long as |wl_output| sends geometry and other events asynchronously (that
  // is, the initial configuration is sent once the interface is bound), we'll
  // have to tell each output to manually inform the delegate about available
  // geometry, scale factor and etc, which will result in feeding the
  // WaylandScreen with the data through OnOutputHandleGeometry and
  // OutOutputHandleScale. All the other hot geometry and scale changes are done
  // automatically, and the |wayland_screen_| is notified immediately about the
  // changes.
  for (const auto& output : output_list_) {
    if (output.second->IsReady()) {
      screen->OnOutputAddedOrUpdated(output.second->GetMetrics());
    }
  }
}

WaylandOutput::Id WaylandOutputManager::GetOutputId(
    wl_output* output_resource) const {
  auto it = base::ranges::find(
      output_list_, output_resource,
      [](const auto& pair) { return pair.second->get_output(); });
  return it == output_list_.end() ? 0 : it->second->output_id();
}

WaylandOutput* WaylandOutputManager::GetOutput(WaylandOutput::Id id) const {
  auto it = output_list_.find(id);
  if (it == output_list_.end())
    return nullptr;

  return it->second.get();
}

WaylandOutput* WaylandOutputManager::GetPrimaryOutput() const {
  if (wayland_screen_) {
    auto output_id = wayland_screen_->GetOutputIdForDisplayId(
        wayland_screen_->GetPrimaryDisplay().id());
    return GetOutput(output_id);
  }
  return nullptr;
}

const WaylandOutputManager::OutputList& WaylandOutputManager::GetAllOutputs()
    const {
  return output_list_;
}

void WaylandOutputManager::DumpState(std::ostream& out) const {
  out << "WaylandOutputManager:" << std::endl;
  if (wayland_screen_) {
    wayland_screen_->DumpState(out);
    out << std::endl;
  }
  for (const auto& output : output_list_) {
    out << "  output[" << output.first << "]:";
    output.second->DumpState(out);
    out << std::endl;
  }
}

void WaylandOutputManager::OnOutputHandleMetrics(
    const WaylandOutput::Metrics& metrics) {
  if (wayland_screen_) {
    wayland_screen_->OnOutputAddedOrUpdated(metrics);
  }

  // Update scale of the windows currently associated with |output_id|. i.e:
  // the ones whose GetPreferredEnteredOutputId() returns |output_id|; or those
  // which have not yet entered any output (i.e: no wl_surface.enter event
  // received for their root surface) and |output_id| is the primary output.
  const bool is_primary =
      wayland_screen_ &&
      metrics.display_id == wayland_screen_->GetPrimaryDisplay().id();
  for (auto* window : connection_->window_manager()->GetAllWindows()) {
    auto entered_output = window->GetPreferredEnteredOutputId();
    if (entered_output == metrics.output_id ||
        (!entered_output && is_primary)) {
      window->OnEnteredOutputScaleChanged();
    }
  }
}

}  // namespace ui
