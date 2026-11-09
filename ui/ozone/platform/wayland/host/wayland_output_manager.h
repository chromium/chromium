// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_OUTPUT_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_OUTPUT_MANAGER_H_

#include <memory>
#include <ostream>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_screen.h"

struct wl_callback;
struct wl_output;
struct wl_registry;

namespace ui {

class WaylandConnection;
class WaylandOutput;

class WaylandOutputManager : public WaylandOutput::Delegate {
 public:
  using OutputList =
      base::flat_map<WaylandOutput::Id, std::unique_ptr<WaylandOutput>>;

  explicit WaylandOutputManager(WaylandConnection* connection);

  WaylandOutputManager(const WaylandOutputManager&) = delete;
  WaylandOutputManager& operator=(const WaylandOutputManager&) = delete;

  ~WaylandOutputManager() override;

  // Says if at least one output has already been announced by a Wayland
  // compositor.
  bool IsOutputReady() const;

  // Binds the wl_output global `output_id`, which the compositor has just
  // announced.
  //
  // The compositor may destroy an output global shortly after removing it
  // (mutter does so after 10 seconds), so binding it as soon as the
  // wl_registry.global event is dispatched fails with a fatal protocol error
  // if that event was dispatched late. Unless the compositor supports
  // wl_fixes.ack_global_remove (and so keeps removed globals alive until the
  // client acknowledges the removal), the bind is deferred until a
  // wl_display.sync round trip completes. Any wl_registry.global_remove that
  // the compositor sent before handling the sync request is dispatched before
  // the sync callback, so the global is never bound once it is known to be
  // removed.
  //
  // This narrows, but does not close, the race without
  // wl_fixes.ack_global_remove: if the compositor removes the global after it
  // handles the sync request, and the client stalls for longer than the
  // compositor's destroy delay before dispatching the sync callback, the
  // global is bound before the global_remove event that follows the callback
  // is dispatched.
  void AddOutputGlobal(wl_registry* registry,
                       WaylandOutput::Id output_id,
                       uint32_t version);

  void RemoveWaylandOutput(WaylandOutput::Id output_id);

  void InitializeAllXdgOutputs();
  void InitializeAllColorManagementOutputs();
  void InitializeAllWpColorManagementOutputs();

  // Creates a platform screen.
  std::unique_ptr<WaylandScreen> CreateWaylandScreen();

  // Feeds a new platform screen with existing outputs.
  void InitWaylandScreen(WaylandScreen* screen);

  // Returns the output_id (i.e. the output's assigned global name).
  WaylandOutput::Id GetOutputId(wl_output* output_resource) const;
  WaylandOutput* GetOutput(WaylandOutput::Id id) const;
  WaylandOutput* GetPrimaryOutput() const;
  const OutputList& GetAllOutputs() const;

  WaylandScreen* wayland_screen() const { return wayland_screen_.get(); }

  void DumpState(std::ostream& out) const;

 private:
  // WaylandOutput::Delegate:
  void OnOutputHandleMetrics(const WaylandOutput::Metrics& metrics) override;

  // wl_callback_listener:
  static void OnPendingOutputsSyncDone(void* data,
                                       wl_callback* callback,
                                       uint32_t time);

  // Binds the output globals in `pending_outputs_`.
  void BindPendingOutputs();

  // Binds the wl_output global `output_id` and adds it to `output_list_`.
  void BindOutput(wl_registry* registry,
                  WaylandOutput::Id output_id,
                  uint32_t version);
  void AddWaylandOutput(WaylandOutput::Id output_id, wl_output* output);

  OutputList output_list_;

  // Output globals announced by the compositor, but not bound yet, mapped to
  // the version to bind. See AddOutputGlobal().
  base::flat_map<WaylandOutput::Id, uint32_t> pending_outputs_;
  raw_ptr<wl_registry> pending_outputs_registry_ = nullptr;
  wl::Object<wl_callback> pending_outputs_sync_callback_;

  const raw_ptr<WaylandConnection> connection_;

  // Non-owned wayland screen instance.
  base::WeakPtr<WaylandScreen> wayland_screen_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_OUTPUT_MANAGER_H_
