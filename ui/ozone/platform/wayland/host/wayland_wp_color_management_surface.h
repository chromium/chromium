// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WP_COLOR_MANAGEMENT_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WP_COLOR_MANAGEMENT_SURFACE_H_

#include <color-management-v1-client-protocol.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_wp_color_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_wp_image_description.h"

namespace ui {

class WaylandConnection;
class WaylandSurface;

// Wraps `wp_color_management_surface_v1` and
// `wp_color_management_surface_feedback_v1`.
class WaylandWpColorManagementSurface : public WaylandWpColorManager::Observer {
 public:
  WaylandWpColorManagementSurface(
      WaylandSurface* wayland_surface,
      WaylandConnection* connection,
      wl::Object<wp_color_management_surface_v1> management_surface,
      wl::Object<wp_color_management_surface_feedback_v1> feedback_surface);
  WaylandWpColorManagementSurface(const WaylandWpColorManagementSurface&) =
      delete;
  WaylandWpColorManagementSurface& operator=(
      const WaylandWpColorManagementSurface&) = delete;
  ~WaylandWpColorManagementSurface() override;

  void SetColorSpace(const gfx::ColorSpace& color_space,
                     const gfx::HDRMetadata& hdr_metadata);

 private:
  // wp_color_management_surface_feedback_v1_listener
  static void OnPreferredChanged(
      void* data,
      wp_color_management_surface_feedback_v1* feedback_surface,
      uint32_t identity);

  // WaylandWpColorManager::Observer:
  void OnHdrEnabledChanged(bool hdr_enabled) override;

  void OnSetColorSpace(
      scoped_refptr<WaylandWpImageDescription> image_description);

  void OnImageDescription(
      scoped_refptr<WaylandWpImageDescription> image_description);

  const raw_ptr<WaylandSurface> wayland_surface_;
  const raw_ptr<WaylandConnection> connection_;
  wl::Object<wp_color_management_surface_v1> management_surface_;
  wl::Object<wp_color_management_surface_feedback_v1> feedback_surface_;

  scoped_refptr<WaylandWpImageDescription> image_description_;
  scoped_refptr<gfx::DisplayColorSpacesRef> display_color_spaces_;

  base::ScopedObservation<WaylandWpColorManager,
                          WaylandWpColorManager::Observer>
      color_manager_observation_{this};

  base::WeakPtrFactory<WaylandWpColorManagementSurface> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WP_COLOR_MANAGEMENT_SURFACE_H_
