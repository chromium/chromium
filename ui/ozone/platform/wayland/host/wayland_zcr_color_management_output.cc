// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zcr_color_management_output.h"

#include <chrome-color-management-client-protocol.h>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"

namespace ui {

WaylandZcrColorManagementOutput::WaylandZcrColorManagementOutput(
    WaylandOutput* wayland_output,
    zcr_color_management_output_v1* color_management_output)
    : wayland_output_(wayland_output),
      zcr_color_management_output_(color_management_output) {
  DCHECK(color_management_output);
  static constexpr zcr_color_management_output_v1_listener
      kColorManagementOutputListener = {
          .color_space_changed = &OnColorSpaceChanged,
          .extended_dynamic_range = &OnExtendedDynamicRange,
      };
  zcr_color_management_output_v1_add_listener(
      zcr_color_management_output_.get(), &kColorManagementOutputListener,
      this);
}

WaylandZcrColorManagementOutput::~WaylandZcrColorManagementOutput() = default;

// static
void WaylandZcrColorManagementOutput::OnColorSpaceChanged(
    void* data,
    zcr_color_management_output_v1* cmo) {
  auto* self = static_cast<WaylandZcrColorManagementOutput*>(data);
  DCHECK(self);

  // request new color space
  self->color_space_ = base::MakeRefCounted<WaylandZcrColorSpace>(
      zcr_color_management_output_v1_get_color_space(
          self->zcr_color_management_output_.get()));

  self->color_space_->SetColorSpaceDoneCallback(
      base::BindOnce(&WaylandZcrColorManagementOutput::OnColorSpaceDone,
                     self->weak_factory_.GetWeakPtr()));
}
// static
void WaylandZcrColorManagementOutput::OnExtendedDynamicRange(
    void* data,
    zcr_color_management_output_v1* cmo,
    uint32_t value) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandZcrColorManagementOutput::OnColorSpaceDone(
    const gfx::ColorSpace& color_space) {
  gfx_color_space_ = std::make_unique<gfx::ColorSpace>(color_space);
  // Notify WaylandScreen that the output has been updated, so it will check for
  // the new ColorSpace.
  wayland_output_->TriggerDelegateNotifications();
}

}  // namespace ui
