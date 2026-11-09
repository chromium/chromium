// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_CURSOR_LOADER_H_
#define UI_WM_CORE_CURSOR_LOADER_H_

#include <map>
#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "ui/aura/client/cursor_shape_client.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-forward.h"
#include "ui/display/display.h"

namespace ui {
class PlatformCursor;
}

namespace wm {

class COMPONENT_EXPORT(UI_WM) CursorLoader
    : public aura::client::CursorShapeClient,
      public ui::CursorFactoryObserver {
 public:
  explicit CursorLoader(bool use_platform_cursors = true);
  CursorLoader(const CursorLoader&) = delete;
  CursorLoader& operator=(const CursorLoader&) = delete;
  ~CursorLoader() override;

  // ui::CursorFactoryObserver:
  void OnThemeLoaded() override;

  // Returns the rotation of the currently loaded cursor.
  display::Display::Rotation rotation() const { return rotation_; }

  // Sets the rotation and scale the cursors are loaded for.
  // Returns true if the cursor needs to be reset.
  bool SetDisplay(const display::Display& display);

  // Sets the size of the mouse cursor icon in dip. Cursors are scaled
  // uniformly so that their height matches the size. If `std::nullopt`, the
  // default size of the cursor assets is used.
  void SetCustomCursorSize(std::optional<int> custom_cursor_size);

  // Sets the color of the cursor.
  void SetColor(SkColor color);

  // Sets the platform cursor based on the type of |cursor|.
  void SetPlatformCursor(ui::Cursor* cursor);

  // aura::client::CursorShapeClient:
  std::optional<ui::CursorData> GetCursorData(
      const ui::Cursor& cursor) const override;

 private:
  // Resets the cursor cache.
  void UnloadCursors();
  void ApplyColorAndSize(ui::CursorData& data_in_and_out) const;
  // Returns `custom_cursor_size_` in pixels, if any.
  std::optional<int> GetCustomCursorSizeInPixels() const;
  scoped_refptr<ui::PlatformCursor> CursorFromType(ui::mojom::CursorType type);
  scoped_refptr<ui::PlatformCursor> LoadCursorFromAsset(
      ui::mojom::CursorType type);

  // Whether to use cursors provided by the underlying platform (e.g. X11
  // cursors). If false or in the case of a failure, Chromium assets will be
  // used instead.
  const bool use_platform_cursors_;

  std::map<ui::mojom::CursorType, scoped_refptr<ui::PlatformCursor>>
      image_cursors_;
  raw_ptr<ui::CursorFactory> factory_ = nullptr;

  // The scale of the current display, used for system cursors. The selection
  // of the particular cursor is platform-dependent.
  float scale_ = 1.0f;

  // The current rotation of the mouse cursor icon.
  display::Display::Rotation rotation_ = display::Display::ROTATE_0;

  // The size of the mouse cursor icon in dip. If `std::nullopt`, the default
  // size of the cursor assets is used.
  std::optional<int> custom_cursor_size_;

  // The current color set to the mouse cursor icon.
  SkColor color_ = ui::kDefaultCursorColor;
};

}  // namespace wm

#endif  // UI_WM_CORE_CURSOR_LOADER_H_
