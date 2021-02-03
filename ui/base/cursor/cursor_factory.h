// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_FACTORY_H_
#define UI_BASE_CURSOR_CURSOR_FACTORY_H_

#include <vector>

#include "base/component_export.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-forward.h"

class SkBitmap;

namespace gfx {
class Point;
}

namespace ui {
using PlatformCursor = void*;

class COMPONENT_EXPORT(UI_BASE_CURSOR_BASE) CursorFactory {
 public:
  CursorFactory();
  virtual ~CursorFactory();

  // Returns the thread-local instance.
  static CursorFactory* GetInstance();

  // Return the default cursor of the specified type. The types are listed in
  // ui/base/cursor/cursor.h. Default cursors are managed by the implementation
  // and must live indefinitely; there's no way to know when to free them.
  // nullptr may be a valid value for the hidden cursor. When a default cursor
  // is not available, base::nullopt is returned.
  virtual base::Optional<PlatformCursor> GetDefaultCursor(
      mojom::CursorType type);

  // Return an image cursor for the specified |type| with a |bitmap| and
  // |hotspot|. Image cursors are referenced counted and have an initial
  // refcount of 1. Therefore, each CreateImageCursor call must be matched with
  // a call to UnrefImageCursor.
  virtual PlatformCursor CreateImageCursor(mojom::CursorType type,
                                           const SkBitmap& bitmap,
                                           const gfx::Point& hotspot);

  // Return a animated cursor from the specified image & hotspot. Animated
  // cursors are referenced counted and have an initial refcount of 1.
  // Therefore, each CreateAnimatedCursor call must be matched with a call to
  // UnrefImageCursor.
  // |frame_delay_ms| is the delay between frames in millisecond.
  virtual PlatformCursor CreateAnimatedCursor(
      mojom::CursorType type,
      const std::vector<SkBitmap>& bitmaps,
      const gfx::Point& hotspot,
      int frame_delay_ms);

  // Increment platform image cursor refcount.
  virtual void RefImageCursor(PlatformCursor cursor);

  // Decrement platform image cursor refcount.
  virtual void UnrefImageCursor(PlatformCursor cursor);

  // Called after CursorThemeManager is initialized, to be able to track
  // cursor theme and size changes.
  virtual void ObserveThemeChanges();
};

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
COMPONENT_EXPORT(UI_BASE_CURSOR_BASE)
std::vector<std::string> CursorNamesFromType(mojom::CursorType type);
#endif

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_FACTORY_H_
