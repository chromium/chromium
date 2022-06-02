// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_output.h"

#include <wayland-server-protocol.h>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace wl {

namespace {
constexpr uint32_t kOutputVersion = 2;
}

TestOutput::TestOutput()
    : GlobalObject(&wl_output_interface, nullptr, kOutputVersion) {}

TestOutput::~TestOutput() = default;

void TestOutput::SetRect(const gfx::Rect& rect) {
  pending_rect_ = rect;
}

void TestOutput::SetScale(int32_t factor) {
  pending_scale_ = factor;
}

void TestOutput::SetTransform(wl_output_transform transform) {
  pending_transform_ = transform;
}

void TestOutput::Flush() {
  constexpr char kUnknownMake[] = "unknown_make";
  constexpr char kUnknownModel[] = "unknown_model";

  if (!pending_rect_ && !pending_scale_)
    return;

  if (pending_rect_ || pending_transform_) {
    if (pending_rect_)
      rect_ = std::move(pending_rect_.value());
    if (pending_transform_)
      transform_ = std::move(pending_transform_.value());

    wl_output_send_geometry(resource(), rect_.x(), rect_.y(),
                            0 /* physical_width */, 0 /* physical_height */,
                            0 /* subpixel */, kUnknownMake, kUnknownModel,
                            transform_);
    wl_output_send_mode(resource(), WL_OUTPUT_MODE_CURRENT, rect_.width(),
                        rect_.height(), 0);
  }

  if (pending_scale_) {
    scale_ = std::move(pending_scale_.value());
    wl_output_send_scale(resource(), scale_);
  }

  if (aura_output_)
    aura_output_->Flush();

  wl_output_send_done(resource());
}

// Notifies clients about the changes in the output configuration, if any. Doing
// this at bind time is the most common behavior among Wayland compositors. But
// there are some compositors that do it "lazily". An example is ChromeOS'
// Exosphere.
//
// Such behavior can be emulated with this class, by just instantiating an
// object with no setter calls. Such calls might then be done later on demand,
// so clients get notified about such changes when Flush() is called.
void TestOutput::OnBind() {
  Flush();
}

void TestOutput::SetAuraOutput(TestZAuraOutput* aura_output) {
  aura_output_ = aura_output;
}

TestZAuraOutput* TestOutput::GetAuraOutput() {
  return aura_output_;
}

}  // namespace wl
