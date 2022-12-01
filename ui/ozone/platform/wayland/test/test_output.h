// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_OUTPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_OUTPUT_H_

#include <wayland-server-protocol.h>
#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/test/global_object.h"
#include "ui/ozone/platform/wayland/test/test_zaura_output.h"
#include "ui/ozone/platform/wayland/test/test_zxdg_output.h"

namespace wl {

// Handle wl_output object.
class TestOutput : public GlobalObject {
 public:
  TestOutput();

  TestOutput(const TestOutput&) = delete;
  TestOutput& operator=(const TestOutput&) = delete;

  ~TestOutput() override;

  static TestOutput* FromResource(wl_resource* resource);

  // Useful only when zaura_shell is supported.
  void set_aura_shell_enabled() { aura_shell_enabled_ = true; }
  bool aura_shell_enabled() { return aura_shell_enabled_; }

  const gfx::Rect GetRect() { return rect_; }
  void SetRect(const gfx::Rect& rect);
  int32_t GetScale() const { return scale_; }
  void SetScale(int32_t factor);
  void SetTransform(wl_output_transform transform);

  void Flush();

  void SetAuraOutput(TestZAuraOutput* aura_output);
  TestZAuraOutput* GetAuraOutput();

  void SetXdgOutput(TestZXdgOutput* aura_output);
  TestZXdgOutput* xdg_output() { return xdg_output_; }

 protected:
  void OnBind() override;

 private:
  bool aura_shell_enabled_ = false;
  gfx::Rect rect_;
  int32_t scale_;
  wl_output_transform transform_{WL_OUTPUT_TRANSFORM_NORMAL};

  absl::optional<gfx::Rect> pending_rect_ = absl::nullopt;
  absl::optional<int32_t> pending_scale_ = absl::nullopt;
  absl::optional<wl_output_transform> pending_transform_ = absl::nullopt;

  raw_ptr<TestZAuraOutput> aura_output_ = nullptr;
  raw_ptr<TestZXdgOutput> xdg_output_ = nullptr;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_OUTPUT_H_
