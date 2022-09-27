// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zcr_color_space_creator.h"

#include <chrome-color-management-client-protocol.h>
#include <cstddef>
#include <memory>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_space.h"

namespace ui {

WaylandZcrColorSpaceCreator::WaylandZcrColorSpaceCreator(
    wl::Object<zcr_color_space_creator_v1> color_space_creator,
    CreatorResultCallback on_creation)
    : zcr_color_space_creator_(std::move(color_space_creator)),
      on_creation_(std::move(on_creation)) {
  DCHECK(zcr_color_space_creator_);
  static const zcr_color_space_creator_v1_listener listener = {
      &WaylandZcrColorSpaceCreator::OnCreated,
      &WaylandZcrColorSpaceCreator::OnError,
  };
  zcr_color_space_creator_v1_add_listener(zcr_color_space_creator_.get(),
                                          &listener, this);
}

WaylandZcrColorSpaceCreator::~WaylandZcrColorSpaceCreator() = default;

// static
void WaylandZcrColorSpaceCreator::OnCreated(
    void* data,
    struct zcr_color_space_creator_v1* css,
    struct zcr_color_space_v1* color_space) {
  WaylandZcrColorSpaceCreator* zcr_color_space_creator =
      static_cast<WaylandZcrColorSpaceCreator*>(data);
  DCHECK(zcr_color_space_creator);
  std::move(zcr_color_space_creator->on_creation_)
      .Run(base::MakeRefCounted<WaylandZcrColorSpace>(color_space), {});
}

// static
void WaylandZcrColorSpaceCreator::OnError(
    void* data,
    struct zcr_color_space_creator_v1* css,
    uint32_t error) {
  WaylandZcrColorSpaceCreator* zcr_color_space_creator =
      static_cast<WaylandZcrColorSpaceCreator*>(data);
  DCHECK(zcr_color_space_creator);
  std::move(zcr_color_space_creator->on_creation_).Run(nullptr, error);
}

}  // namespace ui
