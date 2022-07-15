// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zcr_color_space.h"

#include <chrome-color-management-client-protocol.h>

#include "base/notreached.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

WaylandZcrColorSpace::WaylandZcrColorSpace(
    struct zcr_color_space_v1* color_space)
    : zcr_color_space_(color_space) {
  DCHECK(color_space);
  static const zcr_color_space_v1_listener listener = {
      &WaylandZcrColorSpace::OnIccFile,
      &WaylandZcrColorSpace::OnNames,
      &WaylandZcrColorSpace::OnParams,
      &WaylandZcrColorSpace::OnDone,
  };

  zcr_color_space_v1_add_listener(zcr_color_space_.get(), &listener, this);
  zcr_color_space_v1_get_information(zcr_color_space_.get());
}

WaylandZcrColorSpace::~WaylandZcrColorSpace() = default;

// static
void WaylandZcrColorSpace::OnIccFile(void* data,
                                     struct zcr_color_space_v1* cs,
                                     int32_t icc,
                                     uint32_t icc_size) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void WaylandZcrColorSpace::OnNames(void* data,
                                   struct zcr_color_space_v1* cs,
                                   uint32_t eotf,
                                   uint32_t chromaticity,
                                   uint32_t whitepoint) {}

// static
void WaylandZcrColorSpace::OnParams(void* data,
                                    struct zcr_color_space_v1* cs,
                                    uint32_t eotf,
                                    uint32_t primary_r_x,
                                    uint32_t primary_r_y,
                                    uint32_t primary_g_x,
                                    uint32_t primary_g_y,
                                    uint32_t primary_b_x,
                                    uint32_t primary_b_y,
                                    uint32_t whitepoint_x,
                                    uint32_t whitepoint_y) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void WaylandZcrColorSpace::OnDone(void* data, struct zcr_color_space_v1* cs) {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace ui
