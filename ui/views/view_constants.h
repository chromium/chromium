// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_CONSTANTS_H_
#define UI_VIEWS_VIEW_CONSTANTS_H_

#include "ui/views/views_export.h"

namespace views {

// Size (width or height) within which the user can hold the mouse and the
// view should scroll.
VIEWS_EXPORT extern const int kAutoscrollSize;

// Time in milliseconds to autoscroll by a row. This is used during drag and
// drop.
VIEWS_EXPORT extern const int kAutoscrollRowTimerMS;

// Used to determine whether a drop is on an item or before/after it. If a drop
// occurs kDropBetweenPixels from the top/bottom it is considered before/after
// the item, otherwise it is on the item.
VIEWS_EXPORT extern const int kDropBetweenPixels;

}  // namespace views

#endif  // UI_VIEWS_VIEW_CONSTANTS_H_
