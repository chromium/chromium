// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WP_COLOR_MANAGEMENT_OUTPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WP_COLOR_MANAGEMENT_OUTPUT_H_

#include <color-management-v1-client-protocol.h>

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_wp_color_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_wp_image_description.h"

namespace ui {

class WaylandOutput;
class WaylandConnection;

// Wraps the `wp_color_management_output_v1` object.
class WaylandWpColorManagementOutput : public WaylandWpColorManager::Observer {
 public:
  WaylandWpColorManagementOutput(
      wp_color_management_output_v1* color_management_output,
      WaylandOutput* wayland_output,
      WaylandConnection* connection);
  WaylandWpColorManagementOutput(const WaylandWpColorManagementOutput&) =
      delete;
  WaylandWpColorManagementOutput& operator=(
      const WaylandWpColorManagementOutput&) = delete;
  ~WaylandWpColorManagementOutput() override;

  const gfx::DisplayColorSpaces* GetDisplayColorSpaces() const;

 private:
  // wp_color_management_output_v1_listener
  static void OnImageDescriptionChanged(
      void* data,
      wp_color_management_output_v1* management_output);

  // WaylandWpColorManager::Observer:
  void OnHdrEnabledChanged(bool hdr_enabled) override;

  void GetCurrentColorSpace();
  void OnImageDescription(
      scoped_refptr<WaylandWpImageDescription> image_description);

  wl::Object<wp_color_management_output_v1> color_management_output_;
  const raw_ptr<WaylandOutput> wayland_output_;
  const raw_ptr<WaylandConnection> connection_;

  scoped_refptr<gfx::DisplayColorSpacesRef> display_color_spaces_;
  scoped_refptr<WaylandWpImageDescription> image_description_;

  base::ScopedObservation<WaylandWpColorManager,
                          WaylandWpColorManager::Observer>
      color_manager_observation_{this};

  base::WeakPtrFactory<WaylandWpColorManagementOutput> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WP_COLOR_MANAGEMENT_OUTPUT_H_
