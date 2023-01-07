// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_BUFFER_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_BUFFER_H_

namespace ui {

// |ClipboardBuffer| designates which clipboard buffer the action should be
// applied to.
// TODO(huangdarwin): Use an #ifdef per platform for kSelection and kDrag.
enum class ClipboardBuffer {
  kCopyPaste,
  kSelection,  // Only supported on systems running X11.
  kDrag,       // Only supported on Mac OS X.
  kMaxValue = kDrag
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_BUFFER_H_