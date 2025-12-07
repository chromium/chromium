// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WP_COLOR_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WP_COLOR_MANAGER_H_

#include <color-management-v1-client-protocol.h>

#include <cstdint>
#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/lru_cache.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_wp_image_description.h"

namespace ui {

class WaylandConnection;

// Wraps the `wp_color_manager_v1` global object.
class WaylandWpColorManager
    : public wl::GlobalObjectRegistrar<WaylandWpColorManager> {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    virtual void OnHdrEnabledChanged(bool hdr_enabled) = 0;
  };

  static constexpr char kInterfaceName[] = "wp_color_manager_v1";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  WaylandWpColorManager(wp_color_manager_v1* color_manager,
                        WaylandConnection* connection);
  WaylandWpColorManager(const WaylandWpColorManager&) = delete;
  WaylandWpColorManager& operator=(const WaylandWpColorManager&) = delete;
  ~WaylandWpColorManager();

  void GetImageDescription(
      const gfx::ColorSpace& color_space,
      const gfx::HDRMetadata& hdr_metadata,
      WaylandWpImageDescription::CreationCallback callback);

  wl::Object<wp_color_management_output_v1> CreateColorManagementOutput(
      wl_output* output);
  wl::Object<wp_color_management_surface_v1> CreateColorManagementSurface(
      wl_surface* surface);
  wl::Object<wp_color_management_surface_feedback_v1>
  CreateColorManagementFeedbackSurface(wl_surface* surface);

  void OnHdrEnabledChanged(bool hdr_enabled);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  uint32_t version() const {
    return wp_color_manager_v1_get_version(manager_.get());
  }

  bool IsSupportedRenderIntent(wp_color_manager_v1_render_intent intent) const;

  bool IsSupportedFeature(wp_color_manager_v1_feature feature) const;

  bool IsSupportedPrimaries(wp_color_manager_v1_primaries primaries) const;

  bool IsSupportedTransferFunction(
      wp_color_manager_v1_transfer_function transfer_function) const;

  bool ready() const { return ready_; }

  bool hdr_enabled() const { return hdr_enabled_; }

 private:
  using ImageDescription = std::pair<gfx::ColorSpace, gfx::HDRMetadata>;

  // wp_color_manager_v1_listener
  static void OnSupportedIntent(void* data,
                                wp_color_manager_v1* manager,
                                uint32_t render_intent);
  static void OnSupportedFeature(void* data,
                                 wp_color_manager_v1* manager,
                                 uint32_t feature);
  static void OnSupportedTfNamed(void* data,
                                 wp_color_manager_v1* manager,
                                 uint32_t tf);
  static void OnSupportedPrimariesNamed(void* data,
                                        wp_color_manager_v1* manager,
                                        uint32_t primaries);
  static void OnDone(void* data, wp_color_manager_v1* manager);

  void OnImageDescriptionCreated(
      const gfx::ColorSpace& color_space,
      const gfx::HDRMetadata& hdr_metadata,
      scoped_refptr<WaylandWpImageDescription> image_description);

  bool PopulateDescriptionCreator(
      wp_image_description_creator_params_v1* creator,
      const gfx::ColorSpace& color_space,
      const gfx::HDRMetadata& hdr_metadata);

  wl::Object<wp_color_manager_v1> manager_;
  const raw_ptr<WaylandConnection> connection_;

  // Cache of successfully created image descriptions.
  base::LRUCache<ImageDescription, scoped_refptr<WaylandWpImageDescription>>
      image_description_cache_{64};

  // Callbacks for image descriptions that are being created.
  base::flat_map<ImageDescription,
                 std::vector<WaylandWpImageDescription::CreationCallback>>
      pending_callbacks_;

  // Holds the image description objects while their creation is pending.
  base::flat_map<ImageDescription, scoped_refptr<WaylandWpImageDescription>>
      pending_creations_;

  // Feature support, as bitsets.
  uint32_t supported_intents_ = 0;
  uint32_t supported_features_ = 0;
  uint32_t supported_transfers_ = 0;
  uint32_t supported_primaries_ = 0;

  bool ready_ = false;

  bool hdr_enabled_ = false;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<WaylandWpColorManager> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WP_COLOR_MANAGER_H_
