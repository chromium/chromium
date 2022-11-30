// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef UI_BASE_WIN_ACCESSIBILITY_IDS_WIN_H_
#define UI_BASE_WIN_ACCESSIBILITY_IDS_WIN_H_

#include <limits.h>

namespace base {
namespace win {

// Windows accessibility (MSAA) notifications are posted on an
// accessible object using its owning HWND and a long integer child id.
// Positive child ids can be used to enumerate the children of an object,
// so in Chromium we use only negative values to represent ids of specific
// accessible objects.
//
// Chromium currently has two separate systems that use accessibility ids:
// * views (ui/views/accessibility), and
// * web (content/browser/accessibility)
//
// These constants ensure they use non-overlapping id ranges.

const long kFirstViewsAccessibilityId = -1;
const long kLastViewsAccessibilityId = -999;
const long kFirstBrowserAccessibilityManagerAccessibilityId = -1000;
const long kLastBrowserAccessibilityManagerAccessibilityId = INT_MIN;

}  // win
}  // base

#endif  // UI_BASE_WIN_ACCESSIBILITY_IDS_WIN_H_
