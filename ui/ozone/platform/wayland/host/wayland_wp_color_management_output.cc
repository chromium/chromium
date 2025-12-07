// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_wp_color_management_output.h"

#include "base/logging.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"

namespace ui {

WaylandWpColorManagementOutput::WaylandWpColorManagementOutput(
    wp_color_management_output_v1* color_management_output,
    WaylandOutput* wayland_output,
    WaylandConnection* connection)
    : color_management_output_(color_management_output),
      wayland_output_(wayland_output),
      connection_(connection) {
  DCHECK(color_management_output_);
  static constexpr wp_color_management_output_v1_listener kListener = {
      .image_description_changed = &OnImageDescriptionChanged,
  };
  wp_color_management_output_v1_add_listener(color_management_output_.get(),
                                             &kListener, this);

  color_manager_observation_.Observe(connection_->wp_color_manager());

  // Get the initial color space.
  GetCurrentColorSpace();
}

WaylandWpColorManagementOutput::~WaylandWpColorManagementOutput() = default;

const gfx::DisplayColorSpaces*
WaylandWpColorManagementOutput::GetDisplayColorSpaces() const {
  if (!connection_->wp_color_manager()->hdr_enabled()) {
    return nullptr;
  }

  return display_color_spaces_ ? &display_color_spaces_->color_spaces()
                               : nullptr;
}

void WaylandWpColorManagementOutput::OnHdrEnabledChanged(bool hdr_enabled) {
  wayland_output_->TriggerDelegateNotifications();
}

void WaylandWpColorManagementOutput::GetCurrentColorSpace() {
  auto image_description_object = wl::Object<wp_image_description_v1>(
      wp_color_management_output_v1_get_image_description(
          color_management_output_.get()));

  image_description_ = base::MakeRefCounted<WaylandWpImageDescription>(
      std::move(image_description_object), connection_, std::nullopt,
      base::BindOnce(&WaylandWpColorManagementOutput::OnImageDescription,
                     weak_factory_.GetWeakPtr()));
}

void WaylandWpColorManagementOutput::OnImageDescription(
    scoped_refptr<WaylandWpImageDescription> image_description) {
  if (!image_description) {
    LOG(ERROR) << "Failed to get output image description.";
    return;
  }
  CHECK_EQ(image_description, image_description_);

  display_color_spaces_ = image_description->AsDisplayColorSpaces();

  wayland_output_->TriggerDelegateNotifications();
}

// static
void WaylandWpColorManagementOutput::OnImageDescriptionChanged(
    void* data,
    wp_color_management_output_v1* management_output) {
  auto* self = static_cast<WaylandWpColorManagementOutput*>(data);
  DCHECK(self);
  self->GetCurrentColorSpace();
}

}  // namespace ui
