// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TESTING_FAKE_CURSOR_DELEGATE_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_TESTING_FAKE_CURSOR_DELEGATE_EVDEV_H_

#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"

namespace ui {

class FakeCursorDelegateEvdev : public CursorDelegateEvdev {
 public:
  FakeCursorDelegateEvdev() {}
  ~FakeCursorDelegateEvdev() override {}

  // CursorDelegateEvdev:
  void MoveCursorTo(gfx::AcceleratedWidget widget,
                    const gfx::PointF& location) override {
    cursor_location_ = location;
  }
  void MoveCursorTo(const gfx::PointF& location) override {
    cursor_location_ = location;
  }
  void MoveCursor(const gfx::Vector2dF& delta) override {
    cursor_location_ = gfx::PointF(delta.x(), delta.y());
  }
  bool IsCursorVisible() override { return 1; }
  gfx::Rect GetCursorConfinedBounds() override {
    NOTIMPLEMENTED();
    return gfx::Rect();
  }
  gfx::PointF GetLocation() override { return cursor_location_; }
  void InitializeOnEvdev() override {}

 private:
  // The location of the mock cursor.
  gfx::PointF cursor_location_;

  DISALLOW_COPY_AND_ASSIGN(FakeCursorDelegateEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TESTING_FAKE_CURSOR_DELEGATE_EVDEV_H_
