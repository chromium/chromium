// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_async_cursor.h"

namespace ui {

// static
scoped_refptr<WaylandAsyncCursor> WaylandAsyncCursor::FromPlatformCursor(
    scoped_refptr<PlatformCursor> platform_cursor) {
  return base::WrapRefCounted(
      static_cast<WaylandAsyncCursor*>(platform_cursor.get()));
}

WaylandAsyncCursor::WaylandAsyncCursor() = default;

WaylandAsyncCursor::WaylandAsyncCursor(
    scoped_refptr<BitmapCursor> bitmap_cursor)
    : loaded_(true), bitmap_cursor_(bitmap_cursor) {}

void WaylandAsyncCursor::AddCursorLoadedCallback(Callback callback) {
  if (loaded_) {
    std::move(callback).Run(bitmap_cursor_);
  } else {
    callbacks_.push_back(std::move(callback));
  }
}

void WaylandAsyncCursor::SetBitmapCursor(
    scoped_refptr<BitmapCursor> bitmap_cursor) {
  DCHECK(!loaded_);
  bitmap_cursor_ = bitmap_cursor;
  loaded_ = true;
  for (auto& callback : callbacks_) {
    std::move(callback).Run(bitmap_cursor_);
  }
  callbacks_.clear();
}

WaylandAsyncCursor::~WaylandAsyncCursor() = default;

}  // namespace ui
