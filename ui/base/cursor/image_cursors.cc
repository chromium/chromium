// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/image_cursors.h"

#include <float.h>
#include <stddef.h>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_loader.h"
#include "ui/base/cursor/cursors_aura.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

namespace {

const CursorType kImageCursorIds[] = {
    CursorType::kNull,
    CursorType::kPointer,
    CursorType::kNoDrop,
    CursorType::kNotAllowed,
    CursorType::kCopy,
    CursorType::kHand,
    CursorType::kMove,
    CursorType::kNorthEastResize,
    CursorType::kSouthWestResize,
    CursorType::kSouthEastResize,
    CursorType::kNorthWestResize,
    CursorType::kNorthResize,
    CursorType::kSouthResize,
    CursorType::kEastResize,
    CursorType::kWestResize,
    CursorType::kIBeam,
    CursorType::kAlias,
    CursorType::kCell,
    CursorType::kContextMenu,
    CursorType::kCross,
    CursorType::kHelp,
    CursorType::kVerticalText,
    CursorType::kZoomIn,
    CursorType::kZoomOut,
    CursorType::kRowResize,
    CursorType::kColumnResize,
    CursorType::kEastWestResize,
    CursorType::kNorthSouthResize,
    CursorType::kNorthEastSouthWestResize,
    CursorType::kNorthWestSouthEastResize,
    CursorType::kGrab,
    CursorType::kGrabbing,
};

const CursorType kAnimatedCursorIds[] = {CursorType::kWait,
                                         CursorType::kProgress};

}  // namespace

ImageCursors::ImageCursors() : cursor_size_(CursorSize::kNormal) {}

ImageCursors::~ImageCursors() {
}

void ImageCursors::Initialize() {
  if (!cursor_loader_)
    cursor_loader_.reset(CursorLoader::Create());
}

float ImageCursors::GetScale() const {
  if (!cursor_loader_) {
    NOTREACHED();
    // Returning default on release build as it's not serious enough to crash
    // even if this ever happens.
    return 1.0f;
  }
  return cursor_loader_->scale();
}

display::Display::Rotation ImageCursors::GetRotation() const {
  if (!cursor_loader_) {
    NOTREACHED();
    // Returning default on release build as it's not serious enough to crash
    // even if this ever happens.
    return display::Display::ROTATE_0;
  }
  return cursor_loader_->rotation();
}

bool ImageCursors::SetDisplay(const display::Display& display,
                              float scale_factor) {
  if (!cursor_loader_) {
    cursor_loader_.reset(CursorLoader::Create());
  } else if (cursor_loader_->rotation() == display.rotation() &&
             cursor_loader_->scale() == scale_factor) {
    return false;
  }

  cursor_loader_->set_rotation(display.rotation());
  cursor_loader_->set_scale(scale_factor);
  ReloadCursors();
  return true;
}

void ImageCursors::ReloadCursors() {
  float device_scale_factor = cursor_loader_->scale();

  cursor_loader_->UnloadAll();

  for (size_t i = 0; i < base::size(kImageCursorIds); ++i) {
    int resource_id = -1;
    gfx::Point hot_point;
    bool success =
        GetCursorDataFor(cursor_size_, kImageCursorIds[i], device_scale_factor,
                         &resource_id, &hot_point);
    DCHECK(success);
    cursor_loader_->LoadImageCursor(kImageCursorIds[i], resource_id, hot_point);
  }
  for (size_t i = 0; i < base::size(kAnimatedCursorIds); ++i) {
    int resource_id = -1;
    gfx::Point hot_point;
    bool success =
        GetAnimatedCursorDataFor(cursor_size_, kAnimatedCursorIds[i],
                                 device_scale_factor, &resource_id, &hot_point);
    DCHECK(success);
    cursor_loader_->LoadAnimatedCursor(kAnimatedCursorIds[i],
                                       resource_id,
                                       hot_point,
                                       kAnimatedCursorFrameDelayMs);
  }
}

void ImageCursors::SetCursorSize(CursorSize cursor_size) {
  if (cursor_size_ == cursor_size)
    return;

  cursor_size_ = cursor_size;

  if (cursor_loader_.get())
    ReloadCursors();
}

void ImageCursors::SetPlatformCursor(gfx::NativeCursor* cursor) {
  cursor_loader_->SetPlatformCursor(cursor);
}

base::WeakPtr<ImageCursors> ImageCursors::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ui
