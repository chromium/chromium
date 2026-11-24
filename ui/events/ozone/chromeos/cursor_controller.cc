// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/chromeos/cursor_controller.h"

#include "base/check.h"
#include "base/no_destructor.h"

namespace ui {

namespace {

void TransformCursorMove(display::Display::Rotation rotation,
                         float scale,
                         gfx::Vector2dF* delta) {
  float dx;
  float dy;

  switch (rotation) {
    case display::Display::ROTATE_90:
      dx = -delta->y();
      dy = delta->x();
      break;
    case display::Display::ROTATE_180:
      dx = -delta->x();
      dy = -delta->y();
      break;
    case display::Display::ROTATE_270:
      dx = delta->y();
      dy = -delta->x();
      break;
    default:  // display::Display::ROTATE_0
      dx = delta->x();
      dy = delta->y();
      break;
  }

  delta->set_x(dx * scale);
  delta->set_y(dy * scale);
}

}  // namespace

// static
CursorController* CursorController::GetInstance() {
  static base::NoDestructor<CursorController> instance;
  return instance.get();
}

void CursorController::SetCursorConfigForWindow(
    gfx::AcceleratedWidget widget,
    display::Display::Rotation rotation,
    float scale) {
  base::AutoLock lock(window_to_cursor_configuration_map_lock_);
  PerWindowCursorConfiguration config = {rotation, scale};
  window_to_cursor_configuration_map_[widget] = config;
}

void CursorController::ClearCursorConfigForWindow(
    gfx::AcceleratedWidget widget) {
  base::AutoLock lock(window_to_cursor_configuration_map_lock_);
  window_to_cursor_configuration_map_.erase(widget);
}

void CursorController::ApplyCursorConfigForWindow(gfx::AcceleratedWidget widget,
                                                  gfx::Vector2dF* delta) const {
  base::AutoLock lock(window_to_cursor_configuration_map_lock_);
  auto it = window_to_cursor_configuration_map_.find(widget);
  if (it != window_to_cursor_configuration_map_.end())
    TransformCursorMove(it->second.rotation, it->second.scale, delta);
}

void CursorController::SetInternalDisplayRotation(
    display::Display::Rotation rotation) {
  base::AutoLock lock(window_to_cursor_configuration_map_lock_);
  internal_display_rotation_ = rotation;
}

void CursorController::ApplyInternalDisplayRotation(
    gfx::Vector2dF* delta) const {
  base::AutoLock lock(window_to_cursor_configuration_map_lock_);
  // TransformCursorMove() negates the clockwise display rotation, so apply
  // the opposite rotation here. ApplyCursorConfigForWindow() then cancels it
  // out on the internal display, leaving only the panel orientation.
  TransformCursorMove(static_cast<display::Display::Rotation>(
                          (4 - internal_display_rotation_) % 4),
                      1.f, delta);
}

CursorController::CursorController() {
}

CursorController::~CursorController() {
}

}  // namespace ui
