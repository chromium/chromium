// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_OUTPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_OUTPUT_H_

#include <cstdint>
#include <optional>

#include "base/gtest_prod_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"

namespace ui {

// Wraps the zaura_output object.
class WaylandZAuraOutput {
 public:
  explicit WaylandZAuraOutput(zaura_output* aura_output);
  WaylandZAuraOutput(const WaylandZAuraOutput&) = delete;
  WaylandZAuraOutput& operator=(const WaylandZAuraOutput&) = delete;
  ~WaylandZAuraOutput();

  zaura_output* wl_object() { return obj_.get(); }

  // Returns true if all state defined by this extension necessary to correctly
  // represent the Display has successfully arrived from the server.
  bool IsReady() const;

  // Called after wl_output.done event has been received for this output.
  void OnDone();

  // Called after processing the wl_output.done event. Translates the received
  // state into the metrics object as part of a chained atomic update.
  void UpdateMetrics(WaylandOutput::Metrics& metrics);

 private:
  FRIEND_TEST_ALL_PREFIXES(WaylandZAuraOutputTest, DisplayIdConversions);
  // For unit test use only.
  WaylandZAuraOutput();

  // zaura_output_listener callbacks:
  static void OnScale(void* data,
                      zaura_output* output,
                      uint32_t flags,
                      uint32_t scale);
  static void OnConnection(void* data,
                           zaura_output* output,
                           uint32_t connection);
  static void OnDeviceScaleFactor(void* data,
                                  zaura_output* output,
                                  uint32_t scale);
  static void OnInsets(void* data,
                       zaura_output* output,
                       int32_t top,
                       int32_t left,
                       int32_t bottom,
                       int32_t right);
  static void OnLogicalTransform(void* data,
                                 zaura_output* output,
                                 int32_t transform);
  static void OnDisplayId(void* data,
                          zaura_output* output,
                          uint32_t display_id_hi,
                          uint32_t display_id_lo);
  static void OnActivated(void* data, zaura_output* output);

  // Tracks whether this zaura_output is considered "ready". I.e. it has
  // received all of its relevant Display state from the server followed by a
  // wl_output.done event.
  bool is_ready_ = false;

  wl::Object<zaura_output> obj_;
  gfx::Insets insets_;
  std::optional<int32_t> logical_transform_ = std::nullopt;
  std::optional<int64_t> display_id_ = std::nullopt;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_OUTPUT_H_
