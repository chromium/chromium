// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_KEYSYM_TO_UNICODE_H_
#define UI_EVENTS_KEYCODES_KEYSYM_TO_UNICODE_H_

#include <stdint.h>

#include "ui/events/keycodes/keycodes_x_export.h"

namespace ui {

// Returns a Unicode character corresponding to the given |keysym|.  If the
// |keysym| doesn't represent a printable character, returns zero.  We don't
// support characters outside the Basic Plane, and this function returns zero
// in that case.
KEYCODES_X_EXPORT uint16_t GetUnicodeCharacterFromXKeySym(unsigned long keysym);

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_KEYSYM_TO_UNICODE_H_
