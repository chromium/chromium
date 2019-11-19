// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_OUTPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_OUTPUT_H_

#include <stdint.h>

#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

// WaylandOutput objects keep track of the current output of display
// that are available to the application.
class WaylandOutput {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual void OnOutputHandleMetrics(uint32_t output_id,
                                       const gfx::Rect& new_bounds,
                                       int32_t scale_factor) = 0;
  };

  WaylandOutput(const uint32_t output_id, wl_output* output);
  ~WaylandOutput();

  void Initialize(Delegate* delegate);

  void TriggerDelegateNotification() const;

  uint32_t output_id() const { return output_id_; }
  bool has_output(wl_output* output) const { return output_.get() == output; }
  int32_t scale_factor() const { return scale_factor_; }

  // Tells if the output has already received physical screen dimensions in the
  // global compositor space.
  bool is_ready() const { return !rect_in_physical_pixels_.IsEmpty(); }

 private:
  static constexpr int32_t kDefaultScaleFactor = 1;

  // Callback functions used for setting geometric properties of the output
  // and available modes.
  static void OutputHandleGeometry(void* data,
                                   wl_output* output,
                                   int32_t x,
                                   int32_t y,
                                   int32_t physical_width,
                                   int32_t physical_height,
                                   int32_t subpixel,
                                   const char* make,
                                   const char* model,
                                   int32_t output_transform);

  static void OutputHandleMode(void* data,
                               wl_output* wl_output,
                               uint32_t flags,
                               int32_t width,
                               int32_t height,
                               int32_t refresh);
  static void OutputHandleDone(void* data, struct wl_output* wl_output);
  static void OutputHandleScale(void* data,
                                struct wl_output* wl_output,
                                int32_t factor);

  const uint32_t output_id_ = 0;
  wl::Object<wl_output> output_;
  int32_t scale_factor_ = kDefaultScaleFactor;
  gfx::Rect rect_in_physical_pixels_;

  Delegate* delegate_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WaylandOutput);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SCREEN_H_
