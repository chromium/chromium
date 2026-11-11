// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WP_COLOR_MANAGEMENT_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WP_COLOR_MANAGEMENT_SURFACE_H_

#include <color-management-v1-client-protocol.h>

#include <cstdint>
#include <optional>
#include <utility>

#include "base/functional/callback_forward.h"
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

  // Image descriptions are created asynchronously. Returns true if
  // SetColorSpace() with these arguments would set the image description
  // synchronously, so that it is latched by the next surface commit. Otherwise
  // starts creating the description, returns false and runs `on_ready` once
  // it's created (or failed to be created).
  bool EnsureImageDescription(const gfx::ColorSpace& color_space,
                              const gfx::HDRMetadata& hdr_metadata,
                              base::OnceClosure on_ready);

 private:
  using ImageDescriptionKey = std::pair<gfx::ColorSpace, gfx::HDRMetadata>;
  // wp_color_management_surface_feedback_v1_listener
  static void OnPreferredChanged(
      void* data,
      wp_color_management_surface_feedback_v1* feedback_surface,
      uint32_t identity);

  // WaylandWpColorManager::Observer:
  void OnHdrEnabledChanged(bool hdr_enabled) override;
  void OnColorManagerDestroyed() override;
  void OnColorManagerReady() override;

  void OnSetColorSpace(
      uint64_t sequence,
      scoped_refptr<WaylandWpImageDescription> image_description);

  // Returns the color space to describe buffers in `color_space` with.
  gfx::ColorSpace GetEffectiveColorSpace(
      const gfx::ColorSpace& color_space) const;

  static void OnImageDescriptionEnsured(
      base::WeakPtr<WaylandWpColorManagementSurface> self,
      ImageDescriptionKey key,
      base::OnceClosure on_ready,
      scoped_refptr<WaylandWpImageDescription> image_description);

  void OnImageDescription(
      scoped_refptr<WaylandWpImageDescription> image_description);

  // Issues a `get_preferred` request and (re)creates `image_description_`
  // from the result. Called once at construction time and again whenever the
  // compositor sends a `preferred_changed` event.
  void FetchPreferredImageDescription();

  const raw_ptr<WaylandSurface> wayland_surface_;
  const raw_ptr<WaylandConnection> connection_;
  // Never null: surfaces are only created when the compositor supports the
  // protocol, and the color manager outlives them (see
  // OnColorManagerDestroyed()).
  const raw_ptr<WaylandWpColorManager> color_manager_;
  wl::Object<wp_color_management_surface_v1> management_surface_;
  wl::Object<wp_color_management_surface_feedback_v1> feedback_surface_;

  scoped_refptr<WaylandWpImageDescription> image_description_;
  scoped_refptr<gfx::DisplayColorSpacesRef> display_color_spaces_;
  gfx::ColorSpace color_space_;
  gfx::HDRMetadata hdr_metadata_;
  uint64_t color_space_sequence_ = 0;
  bool srgb_is_gamma22_ = false;

  // The last description that failed to be created in
  // EnsureImageDescription(), which is not waited for again.
  std::optional<ImageDescriptionKey> failed_image_description_;

  base::ScopedObservation<WaylandWpColorManager,
                          WaylandWpColorManager::Observer>
      color_manager_observation_{this};

  base::WeakPtrFactory<WaylandWpColorManagementSurface> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WP_COLOR_MANAGEMENT_SURFACE_H_
