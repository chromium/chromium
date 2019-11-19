// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_H_
#define UI_BASE_CURSOR_CURSOR_H_

#include "build/build_config.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/types/cursor_types.h"
#include "ui/base/ui_base_export.h"
#include "ui/gfx/geometry/point.h"

#if defined(OS_WIN)
typedef struct HINSTANCE__* HINSTANCE;
typedef struct HICON__* HICON;
typedef HICON HCURSOR;
#endif

namespace ui {

#if defined(OS_WIN)
typedef ::HCURSOR PlatformCursor;
#elif defined(USE_X11)
typedef unsigned long PlatformCursor;
#else
typedef void* PlatformCursor;
#endif

// Ref-counted cursor that supports both default and custom cursors.
class UI_BASE_EXPORT Cursor {
 public:
  Cursor();

  // Implicit constructor.
  Cursor(CursorType type);

  // Allow copy.
  Cursor(const Cursor& cursor);

  ~Cursor();

  void SetPlatformCursor(const PlatformCursor& platform);

  void RefCustomCursor();
  void UnrefCustomCursor();

  CursorType native_type() const { return native_type_; }
  PlatformCursor platform() const { return platform_cursor_; }
  float device_scale_factor() const { return device_scale_factor_; }
  void set_device_scale_factor(float scale) { device_scale_factor_ = scale; }

  SkBitmap GetBitmap() const;
  void set_custom_bitmap(const SkBitmap& bitmap) { custom_bitmap_ = bitmap; }

  gfx::Point GetHotspot() const;
  void set_custom_hotspot(const gfx::Point& hotspot) {
    custom_hotspot_ = hotspot;
  }

  // Note: custom cursor comparison may perform expensive pixel equality checks!
  bool operator==(const Cursor& cursor) const;
  bool operator!=(const Cursor& cursor) const { return !(*this == cursor); }

  bool operator==(CursorType type) const { return native_type_ == type; }
  bool operator!=(CursorType type) const { return native_type_ != type; }

  void operator=(const Cursor& cursor);

 private:
#if defined(USE_AURA)
  SkBitmap GetDefaultBitmap() const;
  gfx::Point GetDefaultHotspot() const;
#endif

  // The basic cursor type.
  CursorType native_type_ = CursorType::kNull;

  // The native platform cursor.
  PlatformCursor platform_cursor_ = 0;

  // The device scale factor for the cursor.
  float device_scale_factor_ = 0.0f;

  // The hotspot for the cursor. This is only used for the custom cursor type.
  gfx::Point custom_hotspot_;

  // The bitmap for the cursor. This is only used for the custom cursor type.
  SkBitmap custom_bitmap_;
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_H_
