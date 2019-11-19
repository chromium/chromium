// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MOUSE_CONSTANTS_H_
#define UI_VIEWS_MOUSE_CONSTANTS_H_

#include "base/time/time.h"

namespace views {

// The amount of time, in milliseconds, between clicks until they're
// considered intentionally different.
constexpr auto kMinimumTimeBetweenButtonClicks =
    base::TimeDelta::FromMilliseconds(100);

}  // namespace views

#endif  // UI_VIEWS_MOUSE_CONSTANTS_H_
