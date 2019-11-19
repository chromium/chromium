// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor.h"

#include "base/logging.h"
#include "ui/gfx/skia_util.h"

namespace ui {

Cursor::Cursor() = default;

Cursor::Cursor(CursorType type) : native_type_(type) {}

Cursor::Cursor(const Cursor& cursor)
    : native_type_(cursor.native_type_),
      platform_cursor_(cursor.platform_cursor_),
      device_scale_factor_(cursor.device_scale_factor_),
      custom_hotspot_(cursor.custom_hotspot_),
      custom_bitmap_(cursor.custom_bitmap_) {
  if (native_type_ == CursorType::kCustom)
    RefCustomCursor();
}

Cursor::~Cursor() {
  if (native_type_ == CursorType::kCustom)
    UnrefCustomCursor();
}

void Cursor::SetPlatformCursor(const PlatformCursor& platform) {
  if (native_type_ == CursorType::kCustom)
    UnrefCustomCursor();
  platform_cursor_ = platform;
  if (native_type_ == CursorType::kCustom)
    RefCustomCursor();
}

#if !defined(USE_AURA)
void Cursor::RefCustomCursor() {
  NOTIMPLEMENTED();
}
void Cursor::UnrefCustomCursor() {
  NOTIMPLEMENTED();
}
#endif

SkBitmap Cursor::GetBitmap() const {
  if (native_type_ == CursorType::kCustom)
    return custom_bitmap_;
#if defined(USE_AURA)
  return GetDefaultBitmap();
#else
  return SkBitmap();
#endif
}

gfx::Point Cursor::GetHotspot() const {
  if (native_type_ == CursorType::kCustom)
    return custom_hotspot_;
#if defined(USE_AURA)
  return GetDefaultHotspot();
#else
  return gfx::Point();
#endif
}

bool Cursor::operator==(const Cursor& cursor) const {
  return native_type_ == cursor.native_type_ &&
         platform_cursor_ == cursor.platform_cursor_ &&
         device_scale_factor_ == cursor.device_scale_factor_ &&
         custom_hotspot_ == cursor.custom_hotspot_ &&
         (native_type_ != CursorType::kCustom ||
          gfx::BitmapsAreEqual(custom_bitmap_, cursor.custom_bitmap_));
}

void Cursor::operator=(const Cursor& cursor) {
  if (*this == cursor)
    return;
  if (native_type_ == CursorType::kCustom)
    UnrefCustomCursor();
  native_type_ = cursor.native_type_;
  platform_cursor_ = cursor.platform_cursor_;
  if (native_type_ == CursorType::kCustom)
    RefCustomCursor();
  device_scale_factor_ = cursor.device_scale_factor_;
  custom_hotspot_ = cursor.custom_hotspot_;
  custom_bitmap_ = cursor.custom_bitmap_;
}

}  // namespace ui
