// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_cursor_factory.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/x/x11_cursor.h"
#include "ui/base/x/x11_cursor_loader.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/connection.h"

namespace ui {

namespace {

X11Cursor* ToX11Cursor(PlatformCursor cursor) {
  return static_cast<X11Cursor*>(cursor);
}

PlatformCursor ToPlatformCursor(X11Cursor* cursor) {
  return static_cast<PlatformCursor>(cursor);
}

scoped_refptr<X11Cursor> CreateInvisibleCursor(XCursorLoader* cursor_loader) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  return cursor_loader->CreateCursor(bitmap, gfx::Point(0, 0));
}

}  // namespace

X11CursorFactory::X11CursorFactory()
    : cursor_loader_(std::make_unique<XCursorLoader>(x11::Connection::Get())),
      invisible_cursor_(CreateInvisibleCursor(cursor_loader_.get())) {}

X11CursorFactory::~X11CursorFactory() = default;

base::Optional<PlatformCursor> X11CursorFactory::GetDefaultCursor(
    mojom::CursorType type) {
  auto cursor = GetDefaultCursorInternal(type);
  if (!cursor)
    return base::nullopt;
  return ToPlatformCursor(cursor.get());
}

PlatformCursor X11CursorFactory::CreateImageCursor(mojom::CursorType type,
                                                   const SkBitmap& bitmap,
                                                   const gfx::Point& hotspot) {
  // There is a problem with custom cursors that have no custom data. The
  // resulting SkBitmap is empty and X crashes when creating a zero size cursor
  // image. Return invisible cursor here instead.
  if (bitmap.drawsNothing()) {
    // The result of |invisible_cursor_| is owned by the caller, and will be
    // Unref()ed by code far away. (Usually in web_cursor.cc in content, among
    // others.) If we don't manually add another reference before we cast this
    // to a void*, we can end up with |invisible_cursor_| being freed out from
    // under us.
    invisible_cursor_->AddRef();
    return ToPlatformCursor(invisible_cursor_.get());
  }

  auto cursor = cursor_loader_->CreateCursor(bitmap, hotspot);
  cursor->AddRef();
  return ToPlatformCursor(cursor.get());
}

PlatformCursor X11CursorFactory::CreateAnimatedCursor(
    mojom::CursorType type,
    const std::vector<SkBitmap>& bitmaps,
    const gfx::Point& hotspot,
    int frame_delay_ms) {
  std::vector<XCursorLoader::Image> images;
  images.reserve(bitmaps.size());
  for (const auto& bitmap : bitmaps)
    images.push_back(XCursorLoader::Image{bitmap, hotspot, frame_delay_ms});
  auto cursor = cursor_loader_->CreateCursor(images);
  cursor->AddRef();
  return ToPlatformCursor(cursor.get());
}

void X11CursorFactory::RefImageCursor(PlatformCursor cursor) {
  ToX11Cursor(cursor)->AddRef();
}

void X11CursorFactory::UnrefImageCursor(PlatformCursor cursor) {
  ToX11Cursor(cursor)->Release();
}

void X11CursorFactory::ObserveThemeChanges() {
  auto* cursor_theme_manager = CursorThemeManager::GetInstance();
  DCHECK(cursor_theme_manager);
  cursor_theme_observation_.Observe(cursor_theme_manager);
}

void X11CursorFactory::OnCursorThemeNameChanged(
    const std::string& cursor_theme_name) {
  ClearThemeCursors();
}

void X11CursorFactory::OnCursorThemeSizeChanged(int cursor_theme_size) {
  ClearThemeCursors();
}

scoped_refptr<X11Cursor> X11CursorFactory::GetDefaultCursorInternal(
    mojom::CursorType type) {
  if (type == mojom::CursorType::kNone)
    return invisible_cursor_;

  if (!default_cursors_.count(type)) {
    // Try to load a predefined X11 cursor.
    default_cursors_[type] =
        cursor_loader_->LoadCursor(CursorNamesFromType(type));
  }

  // Returns owned default cursor for this type.
  return default_cursors_[type];
}

void X11CursorFactory::ClearThemeCursors() {
  default_cursors_.clear();
}

}  // namespace ui
