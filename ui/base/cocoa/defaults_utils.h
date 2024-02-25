// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_DEFAULTS_UTILS_H_
#define UI_BASE_COCOA_DEFAULTS_UTILS_H_

#include <optional>

#include "base/component_export.h"
#include "base/time/time.h"

namespace ui {

// Returns a text insertion caret blink period derived from insertion point
// flash rate settings in NSUserDefaults. If no settings exist in defaults,
// returns std::nullopt.
COMPONENT_EXPORT(UI_BASE)
std::optional<base::TimeDelta> TextInsertionCaretBlinkPeriodFromDefaults();

COMPONENT_EXPORT(UI_BASE)
bool& BlinkPeriodRefreshFlagForTesting();
}

#endif  // UI_BASE_COCOA_DEFAULTS_UTILS_H_
