// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_CURSOR_LOADER_H_
#define UI_BASE_X_X11_CURSOR_LOADER_H_

#include <string_view>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/x/x11_cursor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/property_cache.h"
#include "ui/gfx/x/render.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

// This is a port of libxcursor.
class COMPONENT_EXPORT(UI_BASE_X) XCursorLoader {
 public:
  struct Image {
    SkBitmap bitmap;
    gfx::Point hotspot;
    base::TimeDelta frame_delay;
  };

  XCursorLoader(x11::Connection* connection,
                base::RepeatingClosure on_cursor_config_changed);

  ~XCursorLoader();

  // Loads a system cursor for the given |names|. The cursor is loaded
  // asynchronously, but some cursor (possibly a fallback) is always guaranteed
  // to be loaded.
  scoped_refptr<X11Cursor> LoadCursor(const std::vector<std::string>& names);

  // Synchronously loads a cursor for the given |bitmap| or |images|. nullptr
  // may be returned if image cursors are not supported.
  scoped_refptr<X11Cursor> CreateCursor(const std::vector<Image>& images);
  scoped_refptr<X11Cursor> CreateCursor(const SkBitmap& bitmap,
                                        const gfx::Point& hotspot);

 private:
  friend class XCursorLoaderTest;

  void LoadCursorImpl(scoped_refptr<X11Cursor> cursor,
                      const std::vector<std::string>& names,
                      const std::vector<XCursorLoader::Image>& images);

  uint32_t GetPreferredCursorSize() const;

  // Populate the |rm_*| variables from the value of the RESOURCE_MANAGER
  // property on the root window.
  void ParseXResources(std::string_view resources);

  uint16_t CursorNamesToChar(const std::vector<std::string>& names) const;

  bool SupportsCreateCursor() const;
  bool SupportsCreateAnimCursor() const;

  void OnPropertyChanged(x11::Atom property,
                         const x11::GetPropertyResponse& value);

  raw_ptr<x11::Connection> connection_ = nullptr;

  base::RepeatingClosure on_cursor_config_changed_;

  x11::Font cursor_font_ = x11::Font::None;

  x11::Render::PictFormat pict_format_{};

  x11::PropertyCache rm_cache_;
  // Values obtained from the RESOURCE_MANAGER property on the root window.
  std::string rm_xcursor_theme_;
  unsigned int rm_xcursor_size_ = 0;
  unsigned int rm_xft_dpi_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<XCursorLoader> weak_factory_{this};
};

COMPONENT_EXPORT(UI_BASE_X)
std::vector<XCursorLoader::Image> ParseCursorFile(
    scoped_refptr<base::RefCountedMemory> file,
    uint32_t preferred_size);

}  // namespace ui

#endif  // UI_BASE_X_X11_CURSOR_LOADER_H_
