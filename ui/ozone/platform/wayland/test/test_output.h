// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_OUTPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_OUTPUT_H_

#include <wayland-server-protocol.h>

#include <cstdint>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/test/global_object.h"
#include "ui/ozone/platform/wayland/test/test_output_metrics.h"
#include "ui/ozone/platform/wayland/test/test_zxdg_output.h"

struct wl_client;
struct wl_resource;

namespace wl {

// Handles the server-side representation of the wl_output. Values stored in
// `metrics_` are propagated to clients when `Flush()` is called. This occurs
// when the client first binds the output and output extensions are set by
// default.
class TestOutput : public GlobalObject {
 public:
  TestOutput();
  explicit TestOutput(TestOutputMetrics metrics);
  TestOutput(const TestOutput&) = delete;
  TestOutput& operator=(const TestOutput&) = delete;
  ~TestOutput() override;

  static TestOutput* FromResource(wl_resource* resource);

  // Gets the name of the associated wl_output global.
  uint64_t GetOutputName(wl_client* client) const;

  //////////////////////////////////////////////////////////////////////////////
  // Output metrics helpers.

  // Sets the physical and logical bounds of the output to `bounds`. This is
  // helpful for the default case.
  void SetPhysicalAndLogicalBounds(const gfx::Rect& bounds);

  // Applies a transpose operation on the logical size.
  void ApplyLogicalTranspose();

  void SetOrigin(const gfx::Point& wl_origin);
  void SetScale(int32_t wl_scale);
  void SetLogicalSize(const gfx::Size& xdg_logical_size);
  void SetLogicalOrigin(const gfx::Point& xdg_logical_origin);
  void SetPanelTransform(wl_output_transform wl_panel_transform);

  const gfx::Size& GetPhysicalSize() const;
  const gfx::Point& GetOrigin() const;
  int32_t GetScale() const;
  int64_t GetDisplayId() const;
  //////////////////////////////////////////////////////////////////////////////

  // Flushes `metrics_` for this output and all available extensions.
  void Flush();

  void SetXdgOutput(TestZXdgOutput* xdg_output);
  TestZXdgOutput* xdg_output() { return xdg_output_; }

  void set_suppress_implicit_flush(bool suppress_implicit_flush) {
    suppress_implicit_flush_ = suppress_implicit_flush;
  }

  // GlobalObject:
  void OnBind() override;

 private:
  // Disable sending metrics to clients implicitly (i.e. when the output is
  // bound or when output extensions are created). If this is set `Flush()` must
  // be explicitly called to propagate pending metrics.
  bool suppress_implicit_flush_ = false;

  TestOutputMetrics metrics_;

  raw_ptr<TestZXdgOutput> xdg_output_ = nullptr;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_OUTPUT_H_
