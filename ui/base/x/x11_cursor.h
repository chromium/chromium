// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_CURSOR_H_
#define UI_BASE_X_X11_CURSOR_H_

#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/gfx/x/xproto.h"

template <class T>
class scoped_refptr;

namespace ui {

// Ref counted class to hold an X11 cursor resource.  Clears the X11 resources
// on destruction
class COMPONENT_EXPORT(UI_BASE_X) X11Cursor : public PlatformCursor {
 public:
  using Callback = base::OnceCallback<void(x11::Cursor)>;

  static scoped_refptr<X11Cursor> FromPlatformCursor(
      scoped_refptr<PlatformCursor> platform_cursor);

  X11Cursor();
  explicit X11Cursor(x11::Cursor cursor);

  X11Cursor(const X11Cursor&) = delete;
  X11Cursor& operator=(const X11Cursor&) = delete;

  void OnCursorLoaded(Callback callback);

  bool loaded() const { return loaded_; }
  x11::Cursor xcursor() const { return xcursor_; }

 private:
  friend class base::RefCounted<PlatformCursor>;
  friend class XCursorLoader;

  ~X11Cursor() override;

  void SetCursor(x11::Cursor cursor);

  // This cannot be named Release() since it conflicts with base::RefCounted.
  x11::Cursor ReleaseCursor();

  bool loaded_ = false;
  x11::Cursor xcursor_ = x11::Cursor::None;

  std::vector<Callback> callbacks_;
};

}  // namespace ui

#endif  // UI_BASE_X_X11_CURSOR_H_
