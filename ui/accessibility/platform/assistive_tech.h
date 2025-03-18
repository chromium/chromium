// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_ASSISTIVE_TECH_H_
#define UI_ACCESSIBILITY_PLATFORM_ASSISTIVE_TECH_H_

#include "base/component_export.h"

namespace ui {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AssistiveTech)
enum class AssistiveTech {
  // Use kUnknown if still waiting for the assistive tech to be computed,
  // because some platforms need to scan modules/processes which is done
  // off-thread.
  kNone = 0,
  kUnknown = 1,
  kChromeVox = 2,
  kJaws = 3,
  kNarrator = 4,
  kNvda = 5,
  kOrca = 6,
  kSupernova = 7,
  kTalkback = 8,
  kVoiceOver = 9,
  kZoomText = 10,
  kZdsr = 11,
  kGenericScreenReader = 12,
  kMaxValue = 12
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/accessibility/enums.xml:AssistiveTech)

// Returns true if the given assistive tech is a screen reader.
COMPONENT_EXPORT(AX_PLATFORM) bool IsScreenReader(AssistiveTech assistive_tech);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_ASSISTIVE_TECH_H_
