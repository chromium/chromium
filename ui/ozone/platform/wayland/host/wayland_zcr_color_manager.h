// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_COLOR_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_COLOR_MANAGER_H_

#include <chrome-color-management-client-protocol.h>

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/containers/lru_cache.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/color_space.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_space.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_space_creator.h"

struct zcr_color_manager_v1;

namespace ui {

class WaylandConnection;

// Wrapper around |zcr_color_manager_v1| Wayland factory
class WaylandZcrColorManager
    : public wl::GlobalObjectRegistrar<WaylandZcrColorManager> {
 public:
  static constexpr char kInterfaceName[] = "zcr_color_manager_v1";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  WaylandZcrColorManager(zcr_color_manager_v1* zcr_color_manager_,
                         WaylandConnection* connection);

  WaylandZcrColorManager(const WaylandZcrColorManager&) = delete;
  WaylandZcrColorManager& operator=(const WaylandZcrColorManager&) = delete;

  ~WaylandZcrColorManager();

  wl::Object<zcr_color_management_output_v1> CreateColorManagementOutput(
      wl_output* output);
  wl::Object<zcr_color_management_surface_v1> CreateColorManagementSurface(
      wl_surface* surface);

  scoped_refptr<WaylandZcrColorSpace> GetColorSpace(
      const gfx::ColorSpace& color_space);

  uint32_t GetVersion() { return version_; }

 private:
  void OnColorSpaceCreated(gfx::ColorSpace color_space,
                           scoped_refptr<WaylandZcrColorSpace> zcr_color_space,
                           std::optional<uint32_t> error);
  void PreloadCommonColorSpaces();
  wl::Object<zcr_color_space_creator_v1> CreateZcrColorSpaceCreator(
      const gfx::ColorSpace& color_space);
  // in flight
  base::flat_map<gfx::ColorSpace, std::unique_ptr<WaylandZcrColorSpaceCreator>>
      pending_color_spaces_;

  // cache
  base::LRUCache<gfx::ColorSpace, scoped_refptr<WaylandZcrColorSpace>>
      saved_color_spaces_{100};

  // Holds pointer to the zcr_color_manager_v1 Wayland factory.
  const wl::Object<zcr_color_manager_v1> zcr_color_manager_;

  // Non-owned.
  const raw_ptr<WaylandConnection> connection_;

  uint32_t version_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_COLOR_MANAGER_H_
