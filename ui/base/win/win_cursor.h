// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_WIN_CURSOR_H_
#define UI_BASE_WIN_WIN_CURSOR_H_

#include "base/component_export.h"
#include "base/win/windows_types.h"
#include "ui/base/cursor/platform_cursor.h"

template <class T>
class scoped_refptr;

namespace ui {

// Ref counted class to hold a Windows cursor, i.e. an HCURSOR. Clears the
// resources on destruction.
class COMPONENT_EXPORT(UI_BASE) WinCursor : public PlatformCursor {
 public:
  static scoped_refptr<WinCursor> FromPlatformCursor(
      scoped_refptr<PlatformCursor> platform_cursor);

  explicit WinCursor(HCURSOR hcursor = nullptr, bool should_destroy = false);
  WinCursor(const WinCursor&) = delete;
  WinCursor& operator=(const WinCursor&) = delete;

  HCURSOR hcursor() const { return hcursor_; }

 private:
  friend class base::RefCounted<WinCursor>;
  ~WinCursor() override;

  // Release the cursor on deletion. To be used by custom image cursors.
  bool should_destroy_;
  HCURSOR hcursor_;
};

}  // namespace ui

#endif  // UI_BASE_WIN_WIN_CURSOR_H_
