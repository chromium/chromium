// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_LOADER_H_
#define UI_BASE_CURSOR_CURSOR_LOADER_H_

#include <map>
#include <memory>

#include "base/component_export.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-forward.h"
#include "ui/display/display.h"

namespace gfx {
class Point;
}

namespace ui {
class CursorFactory;

class COMPONENT_EXPORT(UI_BASE_CURSOR) CursorLoader {
 public:
  explicit CursorLoader(bool use_platform_cursors = true);
  CursorLoader(const CursorLoader&) = delete;
  CursorLoader& operator=(const CursorLoader&) = delete;
  ~CursorLoader();

  // Returns the rotation and scale of the currently loaded cursor.
  display::Display::Rotation rotation() const { return rotation_; }
  float scale() const { return scale_; }

  // Sets the rotation and scale the cursors are loaded for.
  // Returns true if the cursor image was reloaded.
  bool SetDisplayData(display::Display::Rotation rotation, float scale);

  // Returns the size of the currently loaded cursor.
  CursorSize size() const { return size_; }

  // Sets the size of the mouse cursor icon.
  void SetSize(CursorSize size);

  // Sets the platform cursor based on the type of |cursor|.
  void SetPlatformCursor(Cursor* cursor);

 private:
  // Resets the cursor cache.
  void UnloadCursors();
  void LoadImageCursor(mojom::CursorType id,
                       int resource_id,
                       const gfx::Point& hot);
  PlatformCursor CursorFromType(mojom::CursorType type);
  PlatformCursor LoadCursorFromAsset(mojom::CursorType type);

  // Whether to use cursors provided by the underlying platform (e.g. X11
  // cursors). If false or in the case of a failure, Chromium assets will be
  // used instead.
  const bool use_platform_cursors_;

  std::map<mojom::CursorType, PlatformCursor> image_cursors_;
  CursorFactory* factory_ = nullptr;

  // The current scale of the mouse cursor icon.
  float scale_ = 1.0f;

  // The current rotation of the mouse cursor icon.
  display::Display::Rotation rotation_ = display::Display::ROTATE_0;

  // The preferred size of the mouse cursor icon.
  CursorSize size_ = CursorSize::kNormal;
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_LOADER_H_
