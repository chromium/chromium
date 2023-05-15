// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_OUTPUT_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_OUTPUT_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "ui/display/tablet_state.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"

namespace ui {

class WaylandConnection;

// Wraps the zaura_output_manager object. Responsible for receiving
// output state from the server and associating the state with the
// appropriate WaylandOutput.
class WaylandZAuraOutputManager
    : public wl::GlobalObjectRegistrar<WaylandZAuraOutputManager> {
 public:
  static constexpr char kInterfaceName[] = "zaura_output_manager";

  using OutputMetricsMap =
      base::flat_map<WaylandOutput::Id, WaylandOutput::Metrics>;

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  WaylandZAuraOutputManager(zaura_output_manager* output_manager,
                            WaylandConnection* connection);
  WaylandZAuraOutputManager(const WaylandZAuraOutputManager&) = delete;
  WaylandZAuraOutputManager& operator=(const WaylandZAuraOutputManager&) =
      delete;
  ~WaylandZAuraOutputManager();

  // Gets the complete Metrics object for the `output_id`. Returns a valid
  // object only if the zaura_output_manager is supported and has been
  // bound by the client.
  const WaylandOutput::Metrics* GetOutputMetrics(
      WaylandOutput::Id output_id) const;

  // Removes the stored Metrics for the `output_id`. If no output exists the
  // operation will no-op.
  void RemoveOutputMetrics(WaylandOutput::Id output_id);

  zaura_output_manager* wl_object() { return obj_.get(); }

 private:
  WaylandOutput::Id GetId(wl_output* output) const;
  WaylandOutput* GetWaylandOutput(WaylandOutput::Id output_id);

  // Returns true once the output manager has received the done event. At this
  // point all mandatory state for the output has been received.
  bool IsReady(WaylandOutput::Id output_id) const;

  // zaura_output_manager_listeners
  static void OnDone(void* data,
                     zaura_output_manager* output_manager,
                     wl_output* output);
  static void OnDisplayId(void* data,
                          zaura_output_manager* output_manager,
                          wl_output* output,
                          uint32_t display_id_hi,
                          uint32_t display_id_lo);
  static void OnLogicalPosition(void* data,
                                zaura_output_manager* output_manager,
                                wl_output* output,
                                int32_t x,
                                int32_t y);
  static void OnLogicalSize(void* data,
                            zaura_output_manager* output_manager,
                            wl_output* output,
                            int32_t width,
                            int32_t height);
  static void OnPhysicalSize(void* data,
                             zaura_output_manager* output_manager,
                             wl_output* output,
                             int32_t width,
                             int32_t height);
  static void OnInsets(void* data,
                       zaura_output_manager* output_manager,
                       wl_output* output,
                       int32_t top,
                       int32_t left,
                       int32_t bottom,
                       int32_t right);
  static void OnDeviceScaleFactor(void* data,
                                  zaura_output_manager* output_manager,
                                  wl_output* output,
                                  uint32_t scale_as_uint);
  static void OnLogicalTransform(void* data,
                                 zaura_output_manager* output_manager,
                                 wl_output* output,
                                 int32_t transform);
  static void OnPanelTransform(void* data,
                               zaura_output_manager* output_manager,
                               wl_output* output,
                               int32_t transform);
  static void OnName(void* data,
                     zaura_output_manager* output_manager,
                     wl_output* output,
                     const char* name);
  static void OnDescription(void* data,
                            zaura_output_manager* output_manager,
                            wl_output* output,
                            const char* description);
  static void OnActivated(void* data,
                          zaura_output_manager* output_manager,
                          wl_output* output);
  static void OnOverscanInsets(void* data,
                               zaura_output_manager* output_manager,
                               wl_output* output,
                               int32_t top,
                               int32_t left,
                               int32_t bottom,
                               int32_t right);

  // `pending_output_metrics_map_` holds Metrics objects that are updated
  // incrementally as events arrive from the server. These Metrics objects are
  // copied into the `output_metrics_map_` once clients receive the done event.
  // This ensures atomic updates to the client's view of output metrics.
  OutputMetricsMap pending_output_metrics_map_;
  OutputMetricsMap output_metrics_map_;

  wl::Object<zaura_output_manager> obj_;
  const raw_ptr<WaylandConnection> connection_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_OUTPUT_MANAGER_H_
