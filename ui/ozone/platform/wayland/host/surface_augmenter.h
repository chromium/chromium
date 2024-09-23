// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_SURFACE_AUGMENTER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_SURFACE_AUGMENTER_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace gfx {
class Size;
}

namespace ui {

class WaylandConnection;

// Wraps the surface-augmenter, which is provided via
// surface_augmenter interface.
class SurfaceAugmenter : public wl::GlobalObjectRegistrar<SurfaceAugmenter> {
 public:
  static constexpr char kInterfaceName[] = "surface_augmenter";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  explicit SurfaceAugmenter(surface_augmenter* surface_augmenter,
                            WaylandConnection* connection);
  SurfaceAugmenter(const SurfaceAugmenter&) = delete;
  SurfaceAugmenter& operator=(const SurfaceAugmenter&) = delete;
  ~SurfaceAugmenter();

  bool SupportsSubpixelAccuratePosition() const;
  // Returns true if augmented_surface_set_clip_rect is supported.
  bool SupportsClipRectOnAugmentedSurface() const;
  bool SupportsTransform() const;
  // Returns true if augmented_surface_set_rounded_corners_clip_bounds handles
  // bounds as its in local surface coordinates space.
  bool NeedsRoundedClipBoundsInLocalSurfaceCoordinates() const;
  bool SupportsCompositingOnlySurface() const;

  uint32_t GetSurfaceAugmentorVersion() const;

  wl::Object<augmented_surface> CreateAugmentedSurface(wl_surface* surface);
  wl::Object<augmented_sub_surface> CreateAugmentedSubSurface(
      wl_subsurface* subsurface);

  wl::Object<wl_buffer> CreateSolidColorBuffer(const SkColor4f& color,
                                               const gfx::Size& size);

 private:
  // Wayland object wrapped by this class.
  wl::Object<surface_augmenter> augmenter_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_SURFACE_AUGMENTER_H_
