// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_output_manager.h"

#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_output.h"

namespace ui {

WaylandOutputManager::WaylandOutputManager() = default;

WaylandOutputManager::~WaylandOutputManager() = default;

bool WaylandOutputManager::IsPrimaryOutputReady() const {
  if (output_list_.empty())
    return false;

  // The very first output in the list is always treated as a primary output.
  const auto& primary_output = output_list_.front();
  return primary_output->is_ready();
}

void WaylandOutputManager::AddWaylandOutput(const uint32_t output_id,
                                            wl_output* output) {
  // Make sure an output with |output_id| has not been added yet. It's very
  // unlikely to happen, unless a compositor has a bug in the numeric names
  // representation of global objects.
  auto output_it = std::find_if(output_list_.begin(), output_list_.end(),
                                [output_id](const auto& output) {
                                  return output->output_id() == output_id;
                                });
  DCHECK(output_it == output_list_.end());
  auto wayland_output = std::make_unique<WaylandOutput>(output_id, output);
  WaylandOutput* wayland_output_ptr = wayland_output.get();
  output_list_.push_back(std::move(wayland_output));

  OnWaylandOutputAdded(output_id);

  // If WaylandScreen has already been created, the output can be initialized,
  // which results in setting up a wl_listener and getting the geometry and the
  // scaling factor from the Wayland Compositor.
  wayland_output_ptr->Initialize(this);
}

void WaylandOutputManager::RemoveWaylandOutput(const uint32_t output_id) {
  auto output_it = std::find_if(output_list_.begin(), output_list_.end(),
                                [output_id](const auto& output) {
                                  return output->output_id() == output_id;
                                });

  // Check the comment in the WaylandConnetion::GlobalRemove.
  if (output_it == output_list_.end())
    return;

  bool was_primary_output = IsPrimaryOutput(output_id);
  output_list_.erase(output_it);

  // If it was a primary output removed, make sure the second output, which
  // became a primary one, announces that to observers.
  if (was_primary_output && !output_list_.empty())
    output_list_.front()->TriggerDelegateNotification();

  OnWaylandOutputRemoved(output_id);
}

std::unique_ptr<WaylandScreen> WaylandOutputManager::CreateWaylandScreen() {
  auto wayland_screen = std::make_unique<WaylandScreen>();
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

void WaylandOutputManager::OnWaylandOutputAdded(uint32_t output_id) {
  if (wayland_screen_)
    wayland_screen_->OnOutputAdded(output_id, IsPrimaryOutput(output_id));
}

void WaylandOutputManager::OnWaylandOutputRemoved(uint32_t output_id) {
  if (wayland_screen_)
    wayland_screen_->OnOutputRemoved(output_id);
}

bool WaylandOutputManager::IsPrimaryOutput(uint32_t output_id) const {
  DCHECK(!output_list_.empty());
  // The very first object in the |output_list_| is always treated as a primary
  // output.
  const auto& primary_output = output_list_.front();
  return primary_output->output_id() == output_id;
}

void WaylandOutputManager::OnOutputHandleMetrics(uint32_t output_id,
                                                 const gfx::Rect& new_bounds,
                                                 int32_t scale_factor) {
  if (wayland_screen_) {
    wayland_screen_->OnOutputMetricsChanged(output_id, new_bounds, scale_factor,
                                            IsPrimaryOutput(output_id));
  }
}

}  // namespace ui
