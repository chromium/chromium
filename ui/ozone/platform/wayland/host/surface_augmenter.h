// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_SURFACE_AUGMENTER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_SURFACE_AUGMENTER_H_

#include "ui/ozone/platform/wayland/common/wayland_object.h"

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

  wl::Object<augmented_surface> CreateAugmentedSurface(wl_surface* surface);

 private:
  // Wayland object wrapped by this class.
  wl::Object<surface_augmenter> augmenter_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_SURFACE_AUGMENTER_H_
