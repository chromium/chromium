// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_OUTPUT_MANAGER_V2_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_OUTPUT_MANAGER_V2_H_

#include <unordered_set>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"

namespace ui {

class WaylandConnection;

// Wraps the zaura_output_manager_v2 global object. Handles output configuration
// state transactions and various output operations (e.g. activation). This
// subsumes the responsibilities of other output extensions such as xdg_output
// and aura_output.
class WaylandZAuraOutputManagerV2
    : public wl::GlobalObjectRegistrar<WaylandZAuraOutputManagerV2> {
 public:
  using OutputMetricsMap =
      base::flat_map<WaylandOutput::Id, WaylandOutput::Metrics>;

  static constexpr char kInterfaceName[] = "zaura_output_manager_v2";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  WaylandZAuraOutputManagerV2(zaura_output_manager_v2* output_manager,
                              WaylandConnection* connection);
  WaylandZAuraOutputManagerV2(const WaylandZAuraOutputManagerV2&) = delete;
  WaylandZAuraOutputManagerV2& operator=(const WaylandZAuraOutputManagerV2&) =
      delete;
  ~WaylandZAuraOutputManagerV2();

  // Output removals are communicated as wl_registry.global_remove events. This
  // is called when this event is received for a given `output_id` and schedules
  // the destruction of the output for the end of the configuration-change
  // transaction.
  void ScheduleRemoveWaylandOutput(WaylandOutput::Id output_id);

  const OutputMetricsMap& output_metrics_map_for_testing() const {
    return output_metrics_map_;
  }
  const OutputMetricsMap& pending_output_metrics_map_for_testing() const {
    return pending_output_metrics_map_;
  }

  void DumpState(std::ostream& out) const;

 private:
  WaylandOutput* GetWaylandOutput(WaylandOutput::Id output_id);

  // Returns true the manager has processed an output configuration change
  // transaction for `output_id`.
  bool IsReady(WaylandOutput::Id output_id) const;

  // Removes any state related to `output_id`.
  void RemoveOutput(WaylandOutput::Id output_id);

  // zaura_output_manager_v2_listener callbacks:
  static void OnDone(void* data, zaura_output_manager_v2* output_manager);
  static void OnDisplayId(void* data,
                          zaura_output_manager_v2* output_manager,
                          uint32_t output_id,
                          uint32_t display_id_hi,
                          uint32_t display_id_lo);
  static void OnLogicalPosition(void* data,
                                zaura_output_manager_v2* output_manager,
                                uint32_t output_id,
                                int32_t x,
                                int32_t y);
  static void OnLogicalSize(void* data,
                            zaura_output_manager_v2* output_manager,
                            uint32_t output_id,
                            int32_t width,
                            int32_t height);
  static void OnPhysicalSize(void* data,
                             zaura_output_manager_v2* output_manager,
                             uint32_t output_id,
                             int32_t width,
                             int32_t height);
  static void OnWorkAreaInsets(void* data,
                               zaura_output_manager_v2* output_manager,
                               uint32_t output_id,
                               int32_t top,
                               int32_t left,
                               int32_t bottom,
                               int32_t right);
  static void OnDeviceScaleFactor(void* data,
                                  zaura_output_manager_v2* output_manager,
                                  uint32_t output_id,
                                  uint32_t scale_as_uint);
  static void OnLogicalTransform(void* data,
                                 zaura_output_manager_v2* output_manager,
                                 uint32_t output_id,
                                 int32_t transform);
  static void OnPanelTransform(void* data,
                               zaura_output_manager_v2* output_manager,
                               uint32_t output_id,
                               int32_t transform);
  static void OnName(void* data,
                     zaura_output_manager_v2* output_manager,
                     uint32_t output_id,
                     const char* name);
  static void OnDescription(void* data,
                            zaura_output_manager_v2* output_manager,
                            uint32_t output_id,
                            const char* description);
  static void OnOverscanInsets(void* data,
                               zaura_output_manager_v2* output_manager,
                               uint32_t output_id,
                               int32_t top,
                               int32_t left,
                               int32_t bottom,
                               int32_t right);
  static void OnActivated(void* data,
                          zaura_output_manager_v2* output_manager,
                          uint32_t output_id);

  // `pending_output_metrics_map_` holds Metrics objects that are updated
  // incrementally as events arrive from the server as part of the configuration
  // change transaction. These Metrics objects are copied into the
  // `output_metrics_map_` once the done event is received.
  OutputMetricsMap pending_output_metrics_map_;
  OutputMetricsMap output_metrics_map_;

  // Holds the outputs that have been added or changed as part of the current
  // transaction.
  std::unordered_set<WaylandOutput::Id> pending_outputs_;

  // Holds the outputs that have been removed as part of this display change
  // configuration. These are destroyed by the manager at the end of the
  // transaction.
  std::vector<WaylandOutput::Id> pending_removed_outputs_;

  wl::Object<zaura_output_manager_v2> obj_;
  const raw_ptr<WaylandConnection> connection_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_OUTPUT_MANAGER_V2_H_
