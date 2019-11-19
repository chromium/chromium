// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"

#include <memory>

#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"

namespace ui {

WaylandOutputManager::WaylandOutputManager() = default;

WaylandOutputManager::~WaylandOutputManager() = default;

bool WaylandOutputManager::IsOutputReady() const {
  if (output_list_.empty())
    return false;
  return output_list_.front()->is_ready();
}

void WaylandOutputManager::AddWaylandOutput(const uint32_t output_id,
                                            wl_output* output) {
  // Make sure an output with |output_id| has not been added yet. It's very
  // unlikely to happen, unless a compositor has a bug in the numeric names
  // representation of global objects.
  auto output_it = GetOutputItById(output_id);
  DCHECK(output_it == output_list_.end());
  auto wayland_output = std::make_unique<WaylandOutput>(output_id, output);
  WaylandOutput* wayland_output_ptr = wayland_output.get();
  output_list_.push_back(std::move(wayland_output));

  OnWaylandOutputAdded(output_id);

  // Even if WaylandScreen has not been created, the output still must be
  // initialized, which results in setting up a wl_listener and getting the
  // geometry and the scaling factor from the Wayland Compositor.
  wayland_output_ptr->Initialize(this);
}

void WaylandOutputManager::RemoveWaylandOutput(const uint32_t output_id) {
  auto output_it = GetOutputItById(output_id);

  // Check the comment in the WaylandConnetion::GlobalRemove.
  if (output_it == output_list_.end())
    return;

  output_list_.erase(output_it);
  OnWaylandOutputRemoved(output_id);
}

std::unique_ptr<WaylandScreen> WaylandOutputManager::CreateWaylandScreen(
    WaylandConnection* connection) {
  auto wayland_screen = std::make_unique<WaylandScreen>(connection);
  wayland_screen_ = wayland_screen->GetWeakPtr();

  // As long as |wl_output| sends geometry and other events asynchronously (that
  // is, the initial configuration is sent once the interface is bound), we'll
  // have to tell each output to manually inform the delegate about available
  // geometry, scale factor and etc, which will result in feeding the
  // WaylandScreen with the data through OnOutputHandleGeometry and
  // OutOutputHandleScale. All the other hot geometry and scale changes are done
  // automatically, and the |wayland_screen_| is notified immediately about the
  // changes.
  if (!output_list_.empty()) {
    for (auto& output : output_list_) {
      OnWaylandOutputAdded(output->output_id());
      output->TriggerDelegateNotification();
    }
  }

  return wayland_screen;
}

uint32_t WaylandOutputManager::GetIdForOutput(wl_output* output) const {
  auto output_it = std::find_if(
      output_list_.begin(), output_list_.end(),
      [output](const auto& item) { return item->has_output(output); });
  // This is unlikely to happen, but better to be explicit here.
  DCHECK(output_it != output_list_.end());
  return output_it->get()->output_id();
}

WaylandOutput* WaylandOutputManager::GetOutput(uint32_t id) const {
  auto output_it = GetOutputItById(id);
  // This is unlikely to happen, but better to be explicit here.
  DCHECK(output_it != output_list_.end());
  return output_it->get();
}

void WaylandOutputManager::OnWaylandOutputAdded(uint32_t output_id) {
  if (wayland_screen_)
    wayland_screen_->OnOutputAdded(output_id);
}

void WaylandOutputManager::OnWaylandOutputRemoved(uint32_t output_id) {
  if (wayland_screen_)
    wayland_screen_->OnOutputRemoved(output_id);
}

void WaylandOutputManager::OnOutputHandleMetrics(uint32_t output_id,
                                                 const gfx::Rect& new_bounds,
                                                 int32_t scale_factor) {
  if (wayland_screen_)
    wayland_screen_->OnOutputMetricsChanged(output_id, new_bounds,
                                            scale_factor);
}

WaylandOutputManager::OutputList::const_iterator
WaylandOutputManager::GetOutputItById(uint32_t id) const {
  return std::find_if(
      output_list_.begin(), output_list_.end(),
      [id](const auto& item) { return item->output_id() == id; });
}

}  // namespace ui
