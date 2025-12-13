// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_METRICS_H_
#define UI_VIEWS_METRICS_H_

#include "base/time/time.h"
#include "ui/views/views_export.h"

namespace views {

// The default value for how long to wait before showing a menu button on hover.
// This value is used if the OS doesn't supply one.
inline constexpr base::TimeDelta kDefaultMenuShowDelay =
    base::Milliseconds(400);

// Returns the amount of time between double clicks.
VIEWS_EXPORT base::TimeDelta GetDoubleClickInterval();

// Returns the amount of time to wait from hovering over a menu button until
// showing the menu.
VIEWS_EXPORT base::TimeDelta GetMenuShowDelay();

}  // namespace views

#endif  // UI_VIEWS_METRICS_H_
