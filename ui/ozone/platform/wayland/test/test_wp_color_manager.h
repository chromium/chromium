// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WP_COLOR_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WP_COLOR_MANAGER_H_

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "ui/ozone/platform/wayland/test/global_object.h"

struct wl_resource;

namespace wl {

// The parameters set on a wp_image_description_creator_params_v1 object.
struct TestImageDescriptionParams {
  struct Luminances {
    uint32_t min = 0;
    uint32_t max = 0;
    uint32_t reference = 0;
  };
  struct MasteringLuminance {
    uint32_t min = 0;
    uint32_t max = 0;
  };

  TestImageDescriptionParams();
  TestImageDescriptionParams(const TestImageDescriptionParams&);
  TestImageDescriptionParams& operator=(const TestImageDescriptionParams&);
  ~TestImageDescriptionParams();

  std::optional<uint32_t> tf_named;
  std::optional<uint32_t> tf_power;
  std::optional<uint32_t> primaries_named;
  std::optional<std::array<int32_t, 8>> primaries;
  std::optional<Luminances> luminances;
  std::optional<std::array<int32_t, 8>> mastering_display_primaries;
  std::optional<MasteringLuminance> mastering_luminance;
  std::optional<uint32_t> max_cll;
  std::optional<uint32_t> max_fall;
};

// Manages the wp_color_manager_v1 global. Image descriptions created from
// parametric creators are recorded so tests can inspect what the client sent.
// Image descriptions for outputs and surface feedback describe an SDR sRGB
// display.
class TestWpColorManager : public GlobalObject {
 public:
  TestWpColorManager();
  TestWpColorManager(const TestWpColorManager&) = delete;
  TestWpColorManager& operator=(const TestWpColorManager&) = delete;
  ~TestWpColorManager() override;

  // GlobalObject:
  void OnBind() override;

  // Sets the features advertised to clients on bind. Must be called before the
  // client binds the global.
  void set_supported_features(std::vector<uint32_t> features) {
    supported_features_ = std::move(features);
  }

  // All image descriptions created via parametric creators, in order.
  const std::vector<TestImageDescriptionParams>& created_descriptions() const {
    return created_descriptions_;
  }

  // Returns the parameters of the parametric image description most recently
  // set on `surface` (a wl_surface resource), if any.
  std::optional<TestImageDescriptionParams> GetSurfaceImageDescription(
      wl_resource* surface) const;

  void OnDescriptionCreated(const TestImageDescriptionParams& params);
  void OnSurfaceImageDescriptionSet(
      uint32_t surface_id,
      std::optional<TestImageDescriptionParams> params);

  // Returns a new unique identity for an image description.
  uint32_t NextIdentity() { return ++last_identity_; }

 private:
  uint32_t last_identity_ = 0;
  std::vector<uint32_t> supported_features_;
  std::vector<TestImageDescriptionParams> created_descriptions_;
  // Keyed by wl_surface resource id.
  std::map<uint32_t, std::optional<TestImageDescriptionParams>>
      surface_descriptions_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WP_COLOR_MANAGER_H_
