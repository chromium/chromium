// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_WIN_WIN_CURSOR_H_
#define UI_BASE_CURSOR_WIN_WIN_CURSOR_H_

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/win/windows_types.h"

namespace ui {

// Ref counted class to hold a Windows cursor, i.e. an HCURSOR.  Clears the
// resources on destruction.
class COMPONENT_EXPORT(UI_BASE_CURSOR) WinCursor
    : public base::RefCounted<WinCursor> {
 public:
  explicit WinCursor(HCURSOR hcursor = nullptr);
  WinCursor(const WinCursor&) = delete;
  WinCursor& operator=(const WinCursor&) = delete;

  HCURSOR hcursor() const { return hcursor_; }

 private:
  friend class base::RefCounted<WinCursor>;

  ~WinCursor();

  HCURSOR hcursor_;
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_WIN_WIN_CURSOR_H_
