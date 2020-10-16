// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/image_cursors.h"

#include "base/check.h"
#include "ui/base/cursor/cursor_loader.h"
#include "ui/base/cursor/cursors_aura.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

namespace {

constexpr mojom::CursorType kImageCursorIds[] = {
    mojom::CursorType::kNull,
    mojom::CursorType::kPointer,
    mojom::CursorType::kNoDrop,
    mojom::CursorType::kNotAllowed,
    mojom::CursorType::kCopy,
    mojom::CursorType::kHand,
    mojom::CursorType::kMove,
    mojom::CursorType::kNorthEastResize,
    mojom::CursorType::kSouthWestResize,
    mojom::CursorType::kSouthEastResize,
    mojom::CursorType::kNorthWestResize,
    mojom::CursorType::kNorthResize,
    mojom::CursorType::kSouthResize,
    mojom::CursorType::kEastResize,
    mojom::CursorType::kWestResize,
    mojom::CursorType::kIBeam,
    mojom::CursorType::kAlias,
    mojom::CursorType::kCell,
    mojom::CursorType::kContextMenu,
    mojom::CursorType::kCross,
    mojom::CursorType::kHelp,
    mojom::CursorType::kVerticalText,
    mojom::CursorType::kZoomIn,
    mojom::CursorType::kZoomOut,
    mojom::CursorType::kRowResize,
    mojom::CursorType::kColumnResize,
    mojom::CursorType::kEastWestResize,
    mojom::CursorType::kNorthSouthResize,
    mojom::CursorType::kNorthEastSouthWestResize,
    mojom::CursorType::kNorthWestSouthEastResize,
    mojom::CursorType::kGrab,
    mojom::CursorType::kGrabbing,
};

constexpr mojom::CursorType kAnimatedCursorIds[] = {
    mojom::CursorType::kWait, mojom::CursorType::kProgress};

}  // namespace

ImageCursors::ImageCursors()
    : cursor_loader_(CursorLoader::Create()),
      cursor_size_(CursorSize::kNormal) {}

ImageCursors::~ImageCursors() = default;

float ImageCursors::GetScale() const {
  return cursor_loader_->scale();
}

display::Display::Rotation ImageCursors::GetRotation() const {
  return cursor_loader_->rotation();
}

bool ImageCursors::SetDisplay(const display::Display& display,
                              float scale_factor) {
  if (cursor_loader_->rotation() == display.panel_rotation() &&
      cursor_loader_->scale() == scale_factor)
    return false;

  cursor_loader_->set_rotation(display.panel_rotation());
  cursor_loader_->set_scale(scale_factor);
  ReloadCursors();
  return true;
}

void ImageCursors::ReloadCursors() {
  float device_scale_factor = cursor_loader_->scale();

  cursor_loader_->UnloadAll();

  for (auto cursor_id : kImageCursorIds) {
    int resource_id = -1;
    gfx::Point hot_point;
    bool success = GetCursorDataFor(
        cursor_size_, cursor_id, device_scale_factor, &resource_id, &hot_point);
    DCHECK(success);
    cursor_loader_->LoadImageCursor(cursor_id, resource_id, hot_point);
  }
  for (auto cursor_id : kAnimatedCursorIds) {
    int resource_id = -1;
    gfx::Point hot_point;
    bool success = GetAnimatedCursorDataFor(
        cursor_size_, cursor_id, device_scale_factor, &resource_id, &hot_point);
    DCHECK(success);
    cursor_loader_->LoadAnimatedCursor(cursor_id, resource_id, hot_point,
                                       kAnimatedCursorFrameDelayMs);
  }
}

void ImageCursors::SetCursorSize(CursorSize cursor_size) {
  if (cursor_size_ == cursor_size)
    return;

  cursor_size_ = cursor_size;

  ReloadCursors();
}

void ImageCursors::SetPlatformCursor(gfx::NativeCursor* cursor) {
  cursor_loader_->SetPlatformCursor(cursor);
}

}  // namespace ui
