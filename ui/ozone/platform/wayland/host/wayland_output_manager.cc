// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"

#include <algorithm>
#include <cstdint>
#include <memory>

#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
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
    if (it.second->is_ready())
      return true;
  }
  return false;
}

void WaylandOutputManager::AddWaylandOutput(uint32_t output_id,
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
  if (connection_->xdg_output_manager_v1())
    wayland_output->InitializeXdgOutput(connection_->xdg_output_manager_v1());
  if (connection_->zaura_shell()) {
    wayland_output->InitializeZAuraOutput(
        connection_->zaura_shell()->wl_object());
  }
  DCHECK(!wayland_output->is_ready());

  output_list_[output_id] = std::move(wayland_output);
}

void WaylandOutputManager::RemoveWaylandOutput(uint32_t output_id) {
  // Check the comment in the WaylandConnection::GlobalRemove.
  if (!GetOutput(output_id))
    return;

  // Remove WaylandOutput in following order :
  // 1. from `WaylandSurface::entered_outputs_`
  // 2. from `WaylandScreen::display_list_`
  // 3. from `WaylandOutputManager::output_list_`
  auto* wayland_window_manager = connection_->wayland_window_manager();
  for (auto* window : wayland_window_manager->GetAllWindows())
    window->RemoveEnteredOutput(output_id);

  if (wayland_screen_)
    wayland_screen_->OnOutputRemoved(output_id);
  output_list_.erase(output_id);
}

void WaylandOutputManager::InitializeAllXdgOutputs() {
  DCHECK(connection_->xdg_output_manager_v1());
  for (const auto& output : output_list_)
    output.second->InitializeXdgOutput(connection_->xdg_output_manager_v1());
}

void WaylandOutputManager::InitializeAllZAuraOutputs() {
  DCHECK(connection_->zaura_shell());
  for (const auto& output : output_list_) {
    output.second->InitializeZAuraOutput(
        connection_->zaura_shell()->wl_object());
  }
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
    if (output.second->is_ready()) {
      screen->OnOutputAddedOrUpdated(
          output.second->output_id(), output.second->origin(),
          output.second->logical_size(), output.second->physical_size(),
          output.second->insets(), output.second->scale_factor(),
          output.second->panel_transform(), output.second->logical_transform());
    }
  }
}

WaylandOutput* WaylandOutputManager::GetOutput(uint32_t id) const {
  auto it = output_list_.find(id);
  if (it == output_list_.end())
    return nullptr;

  return it->second.get();
}

WaylandOutput* WaylandOutputManager::GetPrimaryOutput() const {
  if (wayland_screen_)
    return GetOutput(wayland_screen_->GetPrimaryDisplay().id());
  return nullptr;
}

void WaylandOutputManager::OnOutputHandleMetrics(uint32_t output_id,
                                                 const gfx::Point& origin,
                                                 const gfx::Size& logical_size,
                                                 const gfx::Size& physical_size,
                                                 const gfx::Insets& insets,
                                                 float scale_factor,
                                                 int32_t panel_transform,
                                                 int32_t logical_transform) {
  if (wayland_screen_) {
    wayland_screen_->OnOutputAddedOrUpdated(output_id, origin, logical_size,
                                            physical_size, insets, scale_factor,
                                            panel_transform, logical_transform);
  }
  auto* wayland_window_manager = connection_->wayland_window_manager();
  for (auto* window : wayland_window_manager->GetWindowsOnOutput(output_id))
    window->UpdateWindowScale(true);
}

}  // namespace ui
