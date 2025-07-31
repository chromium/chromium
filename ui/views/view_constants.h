// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_CONSTANTS_H_
#define UI_VIEWS_VIEW_CONSTANTS_H_

namespace views {

// Used to determine whether a drop is on an item or before/after it. If a drop
// occurs kDropBetweenPixels from the top/bottom it is considered before/after
// the item, otherwise it is on the item.
inline constexpr int kDropBetweenPixels = 5;

}  // namespace views

#endif  // UI_VIEWS_VIEW_CONSTANTS_H_
