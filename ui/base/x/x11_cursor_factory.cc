// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_cursor_factory.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/base/x/x11_cursor.h"
#include "ui/base/x/x11_cursor_loader.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/connection.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"
#endif

namespace ui {

namespace {

scoped_refptr<X11Cursor> CreateInvisibleCursor(XCursorLoader* cursor_loader) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  return cursor_loader->CreateCursor(bitmap, gfx::Point(0, 0));
}

}  // namespace

X11CursorFactory::X11CursorFactory()
    : cursor_loader_(std::make_unique<XCursorLoader>(
          x11::Connection::Get(),
          base::BindRepeating(
              &X11CursorFactory::ClearThemeCursors,
              // Unretained is safe here since `cursor_loader_`'s lifetime is
              // contained in `this`'s lifetime.
              base::Unretained(this)))) {}

X11CursorFactory::~X11CursorFactory() = default;

scoped_refptr<PlatformCursor> X11CursorFactory::CreateImageCursor(
    mojom::CursorType type,
    const SkBitmap& bitmap,
    const gfx::Point& hotspot,
    float scale) {
  // There is a problem with custom cursors that have no custom data. The
  // resulting SkBitmap is empty and X crashes when creating a zero size cursor
  // image. Return invisible cursor here instead.
  if (bitmap.drawsNothing()) {
    return GetDefaultCursor(mojom::CursorType::kNone);
  }

  return cursor_loader_->CreateCursor(bitmap, hotspot);
}

scoped_refptr<PlatformCursor> X11CursorFactory::CreateAnimatedCursor(
    mojom::CursorType type,
    const std::vector<SkBitmap>& bitmaps,
    const gfx::Point& hotspot,
    float scale,
    base::TimeDelta frame_delay) {
  std::vector<XCursorLoader::Image> images;
  images.reserve(bitmaps.size());
  for (const auto& bitmap : bitmaps) {
    images.push_back(XCursorLoader::Image{bitmap, hotspot, frame_delay});
  }
  return cursor_loader_->CreateCursor(images);
}

void X11CursorFactory::ObserveThemeChanges() {
#if BUILDFLAG(IS_LINUX)
  auto* linux_ui = LinuxUi::instance();
  DCHECK(linux_ui);
  cursor_theme_observation_.Observe(linux_ui);
#endif
}

void X11CursorFactory::OnCursorThemeNameChanged(
    const std::string& cursor_theme_name) {
  ClearThemeCursors();
}

void X11CursorFactory::OnCursorThemeSizeChanged(int cursor_theme_size) {
  ClearThemeCursors();
}

scoped_refptr<PlatformCursor> X11CursorFactory::GetDefaultCursor(
    mojom::CursorType type) {
  if (!default_cursors_.count(type)) {
    // Try to load a predefined X11 cursor.
    default_cursors_[type] =
        type == mojom::CursorType::kNone
            ? CreateInvisibleCursor(cursor_loader_.get())
            : cursor_loader_->LoadCursor(CursorNamesFromType(type));
  }

  return default_cursors_[type];
}

void X11CursorFactory::ClearThemeCursors() {
  default_cursors_.clear();
}

}  // namespace ui
