// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/common/wayland.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_wp_color_manager.h"

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

void WaylandOutputManager::AddOutputGlobal(wl_registry* registry,
                                           WaylandOutput::Id output_id,
                                           uint32_t version) {
  DCHECK(!GetOutput(output_id));

  // The compositor does not destroy a removed global before the client
  // acknowledges the removal, so the global can be bound right away.
  if (connection_->SupportsAckGlobalRemove()) {
    BindOutput(registry, output_id, version);
    connection_->Flush();
    return;
  }

  pending_outputs_registry_ = registry;
  pending_outputs_[output_id] = version;

  // Send a new sync request even if one is in flight: the compositor may have
  // handled the earlier request before it removed this global, in which case
  // the earlier callback precedes the global_remove event. Destroying the
  // earlier callback makes libwayland-client drop its done event. Globals
  // dispatched in the same batch still share a single round trip.
  pending_outputs_sync_callback_.reset(connection_->GetSyncCallback());
  static constexpr wl_callback_listener kSyncListener = {
      .done = &OnPendingOutputsSyncDone,
  };
  wl_callback_add_listener(pending_outputs_sync_callback_.get(), &kSyncListener,
                           this);
  connection_->Flush();
}

// static
void WaylandOutputManager::OnPendingOutputsSyncDone(void* data,
                                                    wl_callback* callback,
                                                    uint32_t time) {
  auto* self = static_cast<WaylandOutputManager*>(data);
  CHECK_EQ(self->pending_outputs_sync_callback_.get(), callback);
  self->pending_outputs_sync_callback_.reset();
  self->BindPendingOutputs();
}

void WaylandOutputManager::BindPendingOutputs() {
  auto pending_outputs = std::move(pending_outputs_);
  pending_outputs_.clear();
  for (const auto& [output_id, version] : pending_outputs) {
    BindOutput(pending_outputs_registry_, output_id, version);
  }
  connection_->Flush();
}

void WaylandOutputManager::BindOutput(wl_registry* registry,
                                      WaylandOutput::Id output_id,
                                      uint32_t version) {
  auto output = wl::Bind<wl_output>(registry, output_id, version);
  if (!output) {
    LOG(ERROR) << "Failed to bind to wl_output global";
    return;
  }
  AddWaylandOutput(output_id, output.release());
}

void WaylandOutputManager::AddWaylandOutput(WaylandOutput::Id output_id,
                                            wl_output* output) {
  // Make sure an output with |output_id| has not been added yet. It's very
  // unlikely to happen, unless a compositor has a bug in the numeric names
  // representation of global objects.
  CHECK(!GetOutput(output_id), base::NotFatalUntil::M163);
  auto wayland_output =
      std::make_unique<WaylandOutput>(output_id, output, connection_);

  // Even if WaylandScreen has not been created, the output still must be
  // initialized, which results in setting up a wl_listener and getting the
  // geometry and the scaling factor from the Wayland Compositor.
  wayland_output->Initialize(this);

  if (connection_->xdg_output_manager_v1()) {
    wayland_output->InitializeXdgOutput(connection_->xdg_output_manager_v1());
  }

  if (auto* wp_color_manager = connection_->wp_color_manager();
      wp_color_manager && wp_color_manager->ready()) {
    wayland_output->InitializeWpColorManagementOutput(wp_color_manager);
  }

  CHECK(!wayland_output->IsReady(), base::NotFatalUntil::M163);

  output_list_[output_id] = std::move(wayland_output);
}

void WaylandOutputManager::RemoveWaylandOutput(WaylandOutput::Id output_id) {
  // The global was removed before it was bound. Never bind it, as the
  // compositor may destroy it at any time now.
  if (pending_outputs_.erase(output_id)) {
    return;
  }

  // Check the comment in the WaylandConnection::GlobalRemove.
  if (!GetOutput(output_id))
    return;

  // Remove WaylandOutput in following order :
  // 1. from `WaylandSurface::entered_outputs_`
  // 2. from `WaylandScreen::display_list_`
  // 3. from `WaylandOutputManager::output_list_`
  auto* wayland_window_manager = connection_->window_manager();
  for (auto window : wayland_window_manager->GetAllWindowsAsWeakPtr()) {
    // RemoveEnteredOutput() may RequestState() from window delegate and close
    // its child windows.
    if (window) {
      window->RemoveEnteredOutput(output_id);
    }
  }

  if (wayland_screen_)
    wayland_screen_->OnOutputRemoved(output_id);
  CHECK(output_list_.find(output_id) != output_list_.end(),
        base::NotFatalUntil::M163);

  output_list_.erase(output_id);
}

void WaylandOutputManager::InitializeAllXdgOutputs() {
  CHECK(connection_->xdg_output_manager_v1(), base::NotFatalUntil::M163);
  for (const auto& output : output_list_)
    output.second->InitializeXdgOutput(connection_->xdg_output_manager_v1());
}

void WaylandOutputManager::InitializeAllWpColorManagementOutputs() {
  auto* wp_color_manager = connection_->wp_color_manager();
  CHECK(wp_color_manager);
  CHECK(wp_color_manager->ready());
  for (const auto& output : output_list_) {
    output.second->InitializeWpColorManagementOutput(wp_color_manager);
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
    if (output.second->IsReady()) {
      screen->OnOutputAddedOrUpdated(output.second->GetMetrics());
    }
  }
}

WaylandOutput::Id WaylandOutputManager::GetOutputId(
    wl_output* output_resource) const {
  auto it = std::ranges::find(
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
  for (auto window : connection_->window_manager()->GetAllWindowsAsWeakPtr()) {
    // RemoveEnteredOutput() may RequestState() from window delegate and close
    // its child windows.
    if (!window) {
      continue;
    }
    auto entered_output = window->GetPreferredEnteredOutputId();
    if (entered_output == metrics.output_id ||
        (!entered_output && is_primary)) {
      window->OnEnteredOutputScaleChanged();
    }
  }
}

}  // namespace ui
