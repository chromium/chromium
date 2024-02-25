// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_MODE_HISTOGRAM_LOGGER_H_
#define UI_ACCESSIBILITY_AX_MODE_HISTOGRAM_LOGGER_H_

#include "ui/accessibility/ax_base_export.h"
#include "ui/accessibility/ax_mode.h"

namespace ui {

enum class AXHistogramPrefix { kNone = 0, kBlink = 1 };

AX_BASE_EXPORT void RecordAccessibilityModeHistograms(AXHistogramPrefix context,
                                                      AXMode mode,
                                                      AXMode previous_mode);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_MODE_HISTOGRAM_LOGGER_H_
