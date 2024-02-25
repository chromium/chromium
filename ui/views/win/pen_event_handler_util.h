// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_PEN_EVENT_HANDLER_UTIL_H_
#define UI_VIEWS_WIN_PEN_EVENT_HANDLER_UTIL_H_

#include "ui/views/views_export.h"

namespace views {

// Lets pen events fall through to the default window procedure until the next
// WM_POINTERUP event.
// Defined in hwnd_message_handler.cc.
VIEWS_EXPORT void UseDefaultHandlerForPenEventsUntilPenUp();

}  // namespace views

#endif  // UI_VIEWS_WIN_PEN_EVENT_HANDLER_UTIL_H_
