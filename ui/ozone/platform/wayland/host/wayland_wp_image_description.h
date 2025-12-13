// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WP_IMAGE_DESCRIPTION_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WP_IMAGE_DESCRIPTION_H_

#include <color-management-v1-client-protocol.h>

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "third_party/skia/modules/skcms/skcms.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class WaylandConnection;

// Wraps `wp_image_description_v1`
class WaylandWpImageDescription
    : public base::RefCounted<WaylandWpImageDescription> {
 public:
  using CreationCallback = base::OnceCallback<void(
      scoped_refptr<WaylandWpImageDescription> image_description)>;

  WaylandWpImageDescription(
      wl::Object<wp_image_description_v1> image_description,
      WaylandConnection* connection,
      std::optional<gfx::ColorSpace> color_space,
      CreationCallback callback);
  WaylandWpImageDescription(const WaylandWpImageDescription&) = delete;
  WaylandWpImageDescription& operator=(const WaylandWpImageDescription&) =
      delete;

  scoped_refptr<gfx::DisplayColorSpacesRef> AsDisplayColorSpaces() const;

  wp_image_description_v1* object() const { return image_description_.get(); }

 private:
  friend class base::RefCounted<WaylandWpImageDescription>;
  ~WaylandWpImageDescription();

  // wp_image_description_v1_listener
  static void OnFailed(void* data,
                       wp_image_description_v1* image_description,
                       uint32_t cause,
                       const char* msg);
  static void OnReady(void* data,
                      wp_image_description_v1* image_description,
                      uint32_t identity);

  // wp_image_description_info_v1_listener
  static void OnInfoDone(void* data, wp_image_description_info_v1* image_info);
  static void OnInfoIccFile(void* data,
                            wp_image_description_info_v1* image_info,
                            int32_t icc,
                            uint32_t icc_size);
  static void OnInfoPrimaries(void* data,
                              wp_image_description_info_v1* image_info,
                              int32_t r_x,
                              int32_t r_y,
                              int32_t g_x,
                              int32_t g_y,
                              int32_t b_x,
                              int32_t b_y,
                              int32_t w_x,
                              int32_t w_y);
  static void OnInfoTfPower(void* data,
                            wp_image_description_info_v1* info,
                            uint32_t eexp);
  static void OnInfoLuminances(void* data,
                               wp_image_description_info_v1* info,
                               uint32_t min_lum,
                               uint32_t max_lum,
                               uint32_t reference_lum);
  static void OnInfoTargetPrimaries(void* data,
                                    wp_image_description_info_v1* info,
                                    int32_t r_x,
                                    int32_t r_y,
                                    int32_t g_x,
                                    int32_t g_y,
                                    int32_t b_x,
                                    int32_t b_y,
                                    int32_t w_x,
                                    int32_t w_y);
  static void OnInfoTargetLuminance(void* data,
                                    wp_image_description_info_v1* info,
                                    uint32_t min_lum,
                                    uint32_t max_lum);
  static void OnInfoTargetMaxCll(void* data,
                                 wp_image_description_info_v1* info,
                                 uint32_t max_cll);
  static void OnInfoTargetMaxFall(void* data,
                                  wp_image_description_info_v1* info,
                                  uint32_t max_fall);
  static void OnInfoTfNamed(void* data,
                            wp_image_description_info_v1* image_info,
                            uint32_t tf);
  static void OnInfoPrimariesNamed(void* data,
                                   wp_image_description_info_v1* image_info,
                                   uint32_t primaries);

  void HandleReady();

  gfx::ColorSpace CreateColorSpaceFromPendingInfo(bool is_hdr) const;

  wl::Object<wp_image_description_v1> image_description_;
  const raw_ptr<WaylandConnection> connection_;

  gfx::ColorSpace color_space_;
  float sdr_max_luminance_nits_ = gfx::ColorSpace::kDefaultSDRWhiteLevel;
  float hdr_max_luminance_relative_ = 1.f;
  CreationCallback creation_callback_;

  // Intermediate state for parsing information events.
  std::optional<skcms_Matrix3x3> pending_custom_primaries_;
  std::optional<skcms_TransferFunction> pending_custom_transfer_fn_;
  std::optional<gfx::ColorSpace::PrimaryID> pending_primary_id_;
  std::optional<gfx::ColorSpace::TransferID> pending_transfer_id_;
  std::optional<uint32_t> pending_reference_lum_;
  std::optional<uint32_t> pending_target_max_lum_;

  // Holds the info object while its information is being parsed.
  wl::Object<wp_image_description_info_v1> info_;

  base::WeakPtrFactory<WaylandWpImageDescription> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WP_IMAGE_DESCRIPTION_H_
