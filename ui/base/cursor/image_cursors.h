// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_IMAGE_CURSORS_H_
#define UI_BASE_CURSOR_IMAGE_CURSORS_H_

#include <memory>

#include "base/component_export.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/display/display.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

class CursorLoader;

// A utility class that provides cursors for NativeCursors for which we have
// image resources.
class COMPONENT_EXPORT(UI_BASE_CURSOR) ImageCursors {
 public:
  ImageCursors();
  ImageCursors(const ImageCursors&) = delete;
  ImageCursors& operator=(const ImageCursors&) = delete;
  ~ImageCursors();

  // Returns the scale and rotation of the currently loaded cursor.
  float GetScale() const;
  display::Display::Rotation GetRotation() const;

  // Sets the display the cursors are loaded for. |scale_factor| determines the
  // size of the image to load. Returns true if the cursor image is reloaded.
  bool SetDisplay(const display::Display& display, float scale_factor);

  // Sets the size of the mouse cursor icon.
  void SetCursorSize(CursorSize cursor_size);

  // Sets the platform cursor based on the native type of |cursor|.
  void SetPlatformCursor(gfx::NativeCursor* cursor);

 private:
  // Reloads the all loaded cursors in the cursor loader.
  void ReloadCursors();

  std::unique_ptr<CursorLoader> cursor_loader_;
  CursorSize cursor_size_;
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_IMAGE_CURSORS_H_
