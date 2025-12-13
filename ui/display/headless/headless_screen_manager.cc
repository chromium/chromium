// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/headless/headless_screen_manager.h"

#include "base/notimplemented.h"
#include "ui/display/display.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

namespace display {

// static
HeadlessScreenManager* HeadlessScreenManager::Get() {
  static base::NoDestructor<HeadlessScreenManager> headless_screen_manager;
  return headless_screen_manager.get();
}

// static
int64_t HeadlessScreenManager::GetNewDisplayId() {
  static int64_t headless_display_id = 1;
  return headless_display_id++;
}

// static
void HeadlessScreenManager::SetDisplayGeometry(
    Display& display,
    const gfx::Rect& bounds_in_pixels,
    const gfx::Insets& work_area_insets_pixels,
    float device_pixel_ratio) {
  display.SetScale(device_pixel_ratio);
  display.set_size_in_pixels(bounds_in_pixels.size());
  display.set_native_origin(bounds_in_pixels.origin());

  gfx::SizeF size(bounds_in_pixels.size());
  size.InvScale(display.device_scale_factor());
  gfx::Rect bounds(bounds_in_pixels.origin(), gfx::ToCeiledSize(size));
  display.set_bounds(bounds);

  gfx::Rect work_area = bounds;
  if (!work_area_insets_pixels.IsEmpty()) {
    work_area.Inset(gfx::ScaleToCeiledInsets(
        work_area_insets_pixels, 1.0f / display.device_scale_factor()));
  }
  display.set_work_area(work_area);
}

void HeadlessScreenManager::SetDelegate(Delegate* delegate,
                                        const base::Location& location) {
  CHECK(!delegate_ || delegate == nullptr)
      << "Delegate is already set by " << location_.ToString();

  delegate_ = delegate;
  location_ = location;
}

int64_t HeadlessScreenManager::AddDisplay(const Display& display) {
  if (!delegate_) {
    NOTIMPLEMENTED();
    return kInvalidDisplayId;
  }

  return delegate_->AddDisplay(display);
}

void HeadlessScreenManager::RemoveDisplay(int64_t display_id) {
  if (!delegate_) {
    NOTIMPLEMENTED();
    return;
  }

  delegate_->RemoveDisplay(display_id);
}

}  // namespace display
