// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_CURSOR_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_CURSOR_OZONE_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/x/x11.h"

class SkBitmap;

namespace gfx {
class Point;
}

namespace ui {

// Ref counted class to hold an X11 cursor resource.  Clears the X11 resources
// on destruction
class X11CursorOzone : public base::RefCounted<X11CursorOzone> {
 public:
  // Handles creating X11 cursor resources from SkBitmap/hotspot.
  X11CursorOzone(const SkBitmap& bitmap, const gfx::Point& hotspot);
  X11CursorOzone(const std::vector<SkBitmap>& bitmaps,
                 const gfx::Point& hotspot,
                 int frame_delay_ms);
  // Loads an X11 cursor named |name| by calling XcursorLibraryLoadCursor().
  // May end up wrapping an x11::None so validity must be checked after using
  // this constructor.
  explicit X11CursorOzone(const char* name);

  // Creates a new cursor that is invisible.
  static scoped_refptr<X11CursorOzone> CreateInvisible();

  XID xcursor() const { return xcursor_; }

 private:
  friend class base::RefCounted<X11CursorOzone>;

  X11CursorOzone();
  ~X11CursorOzone();

  XID xcursor_ = x11::None;

  DISALLOW_COPY_AND_ASSIGN(X11CursorOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_CURSOR_OZONE_H_
