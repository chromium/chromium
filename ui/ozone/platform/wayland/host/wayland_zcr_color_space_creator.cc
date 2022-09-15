// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zcr_color_space_creator.h"

#include <chrome-color-management-client-protocol.h>
#include <memory>

#include "base/check.h"
#include "base/notreached.h"

namespace ui {

WaylandZcrColorSpaceCreator::WaylandZcrColorSpaceCreator(
    struct zcr_color_space_creator_v1* color_space_creator,
    struct zcr_color_management_surface_v1* management_surface)
    : zcr_color_space_creator_(color_space_creator),
      zcr_color_management_surface_(management_surface) {
  DCHECK(color_space_creator);
  static const zcr_color_space_creator_v1_listener listener = {
      &WaylandZcrColorSpaceCreator::OnCreated,
      &WaylandZcrColorSpaceCreator::OnError,
  };
  DCHECK(zcr_color_management_surface_);
  zcr_color_space_creator_v1_add_listener(zcr_color_space_creator_.get(),
                                          &listener, this);
}

WaylandZcrColorSpaceCreator::~WaylandZcrColorSpaceCreator() = default;

// static
void WaylandZcrColorSpaceCreator::OnCreated(
    void* data,
    struct zcr_color_space_creator_v1* css,
    struct zcr_color_space_v1* color_space) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void WaylandZcrColorSpaceCreator::OnError(
    void* data,
    struct zcr_color_space_creator_v1* css,
    uint32_t error) {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace ui
