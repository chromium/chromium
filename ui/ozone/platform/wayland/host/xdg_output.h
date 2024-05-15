// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_OUTPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_OUTPUT_H_

#include <cstdint>
#include <string>

#include "base/gtest_prod_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"

namespace ui {

class XDGOutput {
 public:
  explicit XDGOutput(zxdg_output_v1* xdg_output);
  XDGOutput(const XDGOutput&) = delete;
  XDGOutput& operator=(const XDGOutput&) = delete;
  ~XDGOutput();

  // Returns true if all state defined by this extension necessary to correctly
  // represent the Display has successfully arrived from the server.
  bool IsReady() const;

  // Called after wl_output.done event has been received for this output.
  void HandleDone();

  // Called after processing the wl_output.done event. Translates the received
  // state into the metrics object as part of a chained atomic update.
  void UpdateMetrics(bool compute_scale_from_size,
                     WaylandOutput::Metrics& metrics);

 private:
  FRIEND_TEST_ALL_PREFIXES(WaylandOutputTest, NameAndDescriptionFallback);
  FRIEND_TEST_ALL_PREFIXES(WaylandOutputTest, ScaleFactorCalculation);
  FRIEND_TEST_ALL_PREFIXES(WaylandOutputTest, ScaleFactorFallback);
  FRIEND_TEST_ALL_PREFIXES(WaylandOutputTest, ScaleFactorCalculationNoop);

  // zxdg_output_v1_listener callbacks:
  static void OnLogicalPosition(void* data,
                                zxdg_output_v1* output,
                                int32_t x,
                                int32_t y);
  static void OnLogicalSize(void* data,
                            zxdg_output_v1* output,
                            int32_t width,
                            int32_t height);
  static void OnDone(void* data, zxdg_output_v1* output);
  static void OnName(void* data, zxdg_output_v1* output, const char* name);
  static void OnDescription(void* data,
                            zxdg_output_v1* output,
                            const char* description);

  // Tracks whether this xdg_output is considered "ready". I.e. it has received
  // all of its relevant Display state from the server followed by a
  // wl_output.done event.
  bool is_ready_ = false;

  wl::Object<zxdg_output_v1> xdg_output_;
  gfx::Point logical_position_;
  gfx::Size logical_size_;
  std::string description_;
  std::string name_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_OUTPUT_H_
