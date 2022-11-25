// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_OUTPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_OUTPUT_H_

#include <cstdint>

#include "base/gtest_prod_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

// Wraps the zaura_output object.
class WaylandZAuraOutput {
 public:
  explicit WaylandZAuraOutput(zaura_output* aura_output);
  WaylandZAuraOutput(const WaylandZAuraOutput&) = delete;
  WaylandZAuraOutput& operator=(const WaylandZAuraOutput&) = delete;
  ~WaylandZAuraOutput();

  zaura_output* wl_object() { return obj_.get(); }

  const gfx::Insets& insets() const { return insets_; }
  absl::optional<int32_t> logical_transform() const {
    return logical_transform_;
  }
  absl::optional<int64_t> display_id() const { return display_id_; }

  // Tells If the zuara output receives its display id information when
  // supported.
  bool IsReady() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(WaylandZAuraOutputTest, DisplayIdConversions);
  // For unit test use only.
  WaylandZAuraOutput();

  // zaura_output_listeners
  static void OnScale(void* data,
                      struct zaura_output* zaura_output,
                      uint32_t flags,
                      uint32_t scale);
  static void OnConnection(void* data,
                           struct zaura_output* zaura_output,
                           uint32_t connection);
  static void OnDeviceScaleFactor(void* data,
                                  struct zaura_output* zaura_output,
                                  uint32_t scale);
  static void OnInsets(void* data,
                       struct zaura_output* zaura_output,
                       int32_t top,
                       int32_t left,
                       int32_t bottom,
                       int32_t right);
  static void OnLogicalTransform(void* data,
                                 struct zaura_output* zaura_output,
                                 int32_t transform);
  static void OnDisplayId(void* data,
                          struct zaura_output* zaura_output,
                          uint32_t display_id_hi,
                          uint32_t display_id_lo);
  static void OnActivated(void* data, struct zaura_output* zaura_output);

  wl::Object<zaura_output> obj_;
  gfx::Insets insets_;
  absl::optional<int32_t> logical_transform_ = absl::nullopt;
  absl::optional<int64_t> display_id_ = absl::nullopt;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_OUTPUT_H_
