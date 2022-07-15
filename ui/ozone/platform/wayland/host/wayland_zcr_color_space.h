// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_COLOR_SPACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_COLOR_SPACE_H_

#include <memory>
#include <utility>

#include "base/callback.h"
#include "ui/gfx/color_space.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

// ZcrColorSpace is used to send color space information over wayland protocol.
// its requests and events are specified in chrome-color-management.xml.
// The ui::gfx::ColorSpace equivalent of ZcrColorSpace can be gotten with
// gfx_color_space().
class WaylandZcrColorSpace {
 public:
  using WaylandZcrColorSpaceDoneCallback =
      base::OnceCallback<void(const gfx::ColorSpace&)>;
  explicit WaylandZcrColorSpace(zcr_color_space_v1* color_space);
  WaylandZcrColorSpace(const WaylandZcrColorSpace&) = delete;
  WaylandZcrColorSpace& operator=(const WaylandZcrColorSpace&) = delete;
  ~WaylandZcrColorSpace();

  zcr_color_space_v1* zcr_color_space() const { return zcr_color_space_.get(); }
  bool HasColorSpaceDoneCallback() const {
    return !color_space_done_callback_.is_null();
  }
  void SetColorSpaceDoneCallback(WaylandZcrColorSpaceDoneCallback callback) {
    color_space_done_callback_ = std::move(callback);
  }

 private:
  // zcr_color_space_v1_listener
  static void OnIccFile(void* data,
                        struct zcr_color_space_v1* cs,
                        int32_t icc,
                        uint32_t icc_size);
  static void OnNames(void* data,
                      struct zcr_color_space_v1* cs,
                      uint32_t eotf,
                      uint32_t chromaticity,
                      uint32_t whitepoint);
  static void OnDone(void* data, struct zcr_color_space_v1* cs);
  static void OnParams(void* data,
                       struct zcr_color_space_v1* cs,
                       uint32_t eotf,
                       uint32_t primary_r_x,
                       uint32_t primary_r_y,
                       uint32_t primary_g_x,
                       uint32_t primary_g_y,
                       uint32_t primary_b_x,
                       uint32_t primary_b_y,
                       uint32_t whitepoint_x,
                       uint32_t whitepoint_y);

  wl::Object<zcr_color_space_v1> zcr_color_space_;
  WaylandZcrColorSpaceDoneCallback color_space_done_callback_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_COLOR_SPACE_H_
