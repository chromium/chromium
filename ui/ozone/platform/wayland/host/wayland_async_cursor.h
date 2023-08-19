// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ASYNC_CURSOR_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ASYNC_CURSOR_H_

#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "ui/ozone/common/bitmap_cursor.h"

namespace ui {

class WaylandAsyncCursor : public PlatformCursor {
 public:
  using Callback = base::OnceCallback<void(scoped_refptr<BitmapCursor>)>;

  static scoped_refptr<WaylandAsyncCursor> FromPlatformCursor(
      scoped_refptr<PlatformCursor> platform_cursor);

  WaylandAsyncCursor();
  explicit WaylandAsyncCursor(scoped_refptr<BitmapCursor>);
  WaylandAsyncCursor(const WaylandAsyncCursor&) = delete;
  WaylandAsyncCursor& operator=(const WaylandAsyncCursor&) = delete;

  // Adds a callback to be invoked when the cursor bitmap is loaded. The
  // callback will be invoked immediately if the cursor was already loaded.
  void AddCursorLoadedCallback(Callback callback);

  // Set the loaded cursor bitmap for this asynchronous cursor object and
  // invokes all pending callbacks.
  void SetBitmapCursor(scoped_refptr<BitmapCursor> bitmap_cursor);

  bool loaded() const { return loaded_; }
  scoped_refptr<BitmapCursor> bitmap_cursor() const { return bitmap_cursor_; }

 private:
  friend class base::RefCounted<PlatformCursor>;

  ~WaylandAsyncCursor() override;

  bool loaded_ = false;
  scoped_refptr<BitmapCursor> bitmap_cursor_;

  std::vector<Callback> callbacks_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ASYNC_CURSOR_H_
