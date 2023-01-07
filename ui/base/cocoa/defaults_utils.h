// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_DEFAULTS_UTILS_H_
#define UI_BASE_COCOA_DEFAULTS_UTILS_H_

#include "base/component_export.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ui {

// Returns a text insertion caret blink period derived from insertion point
// flash rate settings in NSUserDefaults. If no settings exist in defaults,
// returns absl::nullopt.
COMPONENT_EXPORT(UI_BASE)
absl::optional<base::TimeDelta> TextInsertionCaretBlinkPeriodFromDefaults();

COMPONENT_EXPORT(UI_BASE)
bool& BlinkPeriodRefreshFlagForTesting();
}

#endif  // UI_BASE_COCOA_DEFAULTS_UTILS_H_
