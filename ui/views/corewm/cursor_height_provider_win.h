// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_CURSOR_HEIGHT_PROVIDER_WIN_H_
#define UI_VIEWS_COREWM_CURSOR_HEIGHT_PROVIDER_WIN_H_

namespace views {
namespace corewm {

// Gets the visible height of current cursor.
//
// The height is offset between cursor's hot point and it's
// bottom edge, derived from first non-transparent row of cursor's mask.

int GetCurrentCursorVisibleHeight();

}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_CURSOR_HEIGHT_PROVIDER_WIN_H_
