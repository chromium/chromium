// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/assistive_tech.h"

namespace ui {

bool IsScreenReader(AssistiveTech assistive_tech) {
  switch (assistive_tech) {
    // On some operating systems, we don't know if a screen reader is running
    // until some expensive operations are performed off-thread.
    // assume there is a not screen reader in this case, as this is generally
    // the most appropriate for most call sites.
    case AssistiveTech::kUnknown:
    case AssistiveTech::kNone:
    // ZoomText is a screen magnifier.
    case AssistiveTech::kZoomText:
      return false;
    case AssistiveTech::kChromeVox:
    case AssistiveTech::kJaws:
    case AssistiveTech::kNarrator:
    case AssistiveTech::kNvda:
    case AssistiveTech::kOrca:
    case AssistiveTech::kSupernova:
    case AssistiveTech::kTalkback:
    case AssistiveTech::kVoiceOver:
    case AssistiveTech::kZdsr:
    case AssistiveTech::kGenericScreenReader:
      return true;
  }
}

}  // namespace ui
