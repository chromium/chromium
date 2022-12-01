// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_OUTPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_OUTPUT_H_

#include <stdint.h>

#include "base/gtest_prod_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class XDGOutput {
 public:
  explicit XDGOutput(struct zxdg_output_v1* xdg_output);
  XDGOutput(const XDGOutput&) = delete;
  XDGOutput& operator=(const XDGOutput&) = delete;
  ~XDGOutput();

  absl::optional<gfx::Point> logical_position() const {
    return logical_position_;
  }
  gfx::Size logical_size() const { return logical_size_; }
  const std::string& description() const { return description_; }
  const std::string& name() const { return name_; }

  // Tells if the output has already received necessary screen information to
  // generate Display.
  bool IsReady() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(WaylandOutputTest, NameAndDescriptionFallback);

  static void OutputHandleLogicalPosition(void* data,
                                          struct zxdg_output_v1* zxdg_output_v1,
                                          int32_t x,
                                          int32_t y);
  static void OutputHandleLogicalSize(void* data,
                                      struct zxdg_output_v1* zxdg_output_v1,
                                      int32_t width,
                                      int32_t height);
  static void OutputHandleDone(void* data,
                               struct zxdg_output_v1* zxdg_output_v1);
  static void OutputHandleName(void* data,
                               struct zxdg_output_v1* zxdg_output_v1,
                               const char* name);
  static void OutputHandleDescription(void* data,
                                      struct zxdg_output_v1* zxdg_output_v1,
                                      const char* description);

  wl::Object<zxdg_output_v1> xdg_output_;
  absl::optional<gfx::Point> logical_position_;
  gfx::Size logical_size_;
  std::string description_;
  std::string name_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_OUTPUT_H_
