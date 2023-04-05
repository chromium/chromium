// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEWS_SWITCHES_H_
#define UI_VIEWS_VIEWS_SWITCHES_H_

#include "build/build_config.h"
#include "ui/views/views_export.h"

namespace views::switches {

// Please keep alphabetized.
VIEWS_EXPORT extern const char
    kDisableInputEventActivationProtectionForTesting[];
VIEWS_EXPORT extern const char kDrawViewBoundsRects[];
VIEWS_EXPORT extern const char kViewStackTraces[];

}  // namespace views::switches

#endif  // UI_VIEWS_VIEWS_SWITCHES_H_
