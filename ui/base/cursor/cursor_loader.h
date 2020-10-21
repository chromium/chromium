// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_LOADER_H_
#define UI_BASE_CURSOR_CURSOR_LOADER_H_

#include <memory>

#include "base/component_export.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-forward.h"
#include "ui/display/display.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

class COMPONENT_EXPORT(UI_BASE_CURSOR) CursorLoader {
 public:
  CursorLoader() = default;
  CursorLoader(const CursorLoader&) = delete;
  CursorLoader& operator=(const CursorLoader&) = delete;
  virtual ~CursorLoader() = default;

  // Returns the rotation and scale of the currently loaded cursor.
  display::Display::Rotation rotation() const { return rotation_; }
  float scale() const { return scale_; }

  // Sets the rotation and scale the cursors are loaded for.
  // Returns true if the cursor image was reloaded.
  bool SetDisplayData(display::Display::Rotation rotation, float scale) {
    if (rotation_ == rotation && scale_ == scale)
      return false;

    rotation_ = rotation;
    scale_ = scale;
    UnloadCursors();
    return true;
  }

  // Returns the size of the currently loaded cursor.
  CursorSize size() const { return size_; }

  // Sets the size of the mouse cursor icon.
  void set_size(CursorSize size) {
    if (size_ == size)
      return;

    size_ = size;
    UnloadCursors();
  }

  // Sets the platform cursor based on the native type of |cursor|.
  virtual void SetPlatformCursor(gfx::NativeCursor* cursor) = 0;

  // Creates a CursorLoader.
  static std::unique_ptr<CursorLoader> Create(bool use_platform_cursors = true);

 protected:
  // Resets the cursor cache.
  virtual void UnloadCursors() = 0;

 private:
  // The current scale of the mouse cursor icon.
  float scale_ = 1.0f;

  // The current rotation of the mouse cursor icon.
  display::Display::Rotation rotation_ = display::Display::ROTATE_0;

  // The preferred size of the mouse cursor icon.
  CursorSize size_ = CursorSize::kNormal;
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_LOADER_H_
