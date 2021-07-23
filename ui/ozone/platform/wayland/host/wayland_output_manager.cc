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

namespace ui {

WaylandOutputManager::WaylandOutputManager(WaylandConnection* connection)
    : connection_(connection) {}

WaylandOutputManager::~WaylandOutputManager() = default;

// Output is considered ready when at least one wl_output is fully configured
// (i.e: wl_output::done received), so that WaylandOutputManager is able to
// instantiate a valid WaylandScreen when requested by the upper layer.
bool WaylandOutputManager::IsOutputReady() const {
  return std::find_if(output_list_.begin(), output_list_.end(),
                      [](const auto& output) { return output->is_ready(); }) !=
         output_list_.end();
}

void WaylandOutputManager::AddWaylandOutput(uint32_t output_id,
                                            wl_output* output) {
  // Make sure an output with |output_id| has not been added yet. It's very
  // unlikely to happen, unless a compositor has a bug in the numeric names
  // representation of global objects.
  auto output_it = GetOutputItById(output_id);
  DCHECK(output_it == output_list_.end());
  auto wayland_output = std::make_unique<WaylandOutput>(output_id, output);

  // Even if WaylandScreen has not been created, the output still must be
  // initialized, which results in setting up a wl_listener and getting the
  // geometry and the scaling factor from the Wayland Compositor.
  wayland_output->Initialize(this);
  DCHECK(!wayland_output->is_ready());

  output_list_.push_back(std::move(wayland_output));
}

void WaylandOutputManager::RemoveWaylandOutput(uint32_t output_id) {
  auto output_it = GetOutputItById(output_id);

  // Check the comment in the WaylandConnetion::GlobalRemove.
  if (output_it == output_list_.end())
    return;

  if (wayland_screen_)
    wayland_screen_->OnOutputRemoved(output_id);
  output_list_.erase(output_it);
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
    if (output->is_ready()) {
      screen->OnOutputAddedOrUpdated(output->output_id(), output->bounds(),
                                     output->scale_factor(),
                                     output->transform());
    }
  }
}

WaylandOutput* WaylandOutputManager::GetOutput(uint32_t id) const {
  auto output_it = GetOutputItById(id);
  // This is unlikely to happen, but better to be explicit here.
  DCHECK(output_it != output_list_.end());
  return output_it->get();
}

WaylandOutput* WaylandOutputManager::GetPrimaryOutput() const {
  if (wayland_screen_)
    return GetOutput(wayland_screen_->GetPrimaryDisplay().id());
  return nullptr;
}

void WaylandOutputManager::OnOutputHandleMetrics(uint32_t output_id,
                                                 const gfx::Rect& new_bounds,
                                                 int32_t scale_factor,
                                                 int32_t transform) {
  if (wayland_screen_) {
    wayland_screen_->OnOutputAddedOrUpdated(output_id, new_bounds,
                                            scale_factor, transform);
  }
  auto* wayland_window_manager = connection_->wayland_window_manager();
  for (auto* window : wayland_window_manager->GetWindowsOnOutput(output_id))
    window->UpdateWindowScale(true);
}

WaylandOutputManager::OutputList::const_iterator
WaylandOutputManager::GetOutputItById(uint32_t id) const {
  return std::find_if(
      output_list_.begin(), output_list_.end(),
      [id](const auto& item) { return item->output_id() == id; });
}

}  // namespace ui
