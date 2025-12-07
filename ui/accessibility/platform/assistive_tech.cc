// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/assistive_tech.h"

#include "base/notreached.h"

namespace ui {

namespace {
static constexpr std::string_view kNoneString{"None"};
static constexpr std::string_view kUninitializedString{"Uninitialized"};
static constexpr std::string_view kChromeVoxString{"ChromeVox"};
static constexpr std::string_view kJawsString{"Jaws"};
static constexpr std::string_view kNarratorString{"Narrator"};
static constexpr std::string_view kNvdaString{"Nvda"};
static constexpr std::string_view kOrcaString{"Orca"};
static constexpr std::string_view kSupernovaString{"Supernova"};
static constexpr std::string_view kTalkbackString{"Talkback"};
static constexpr std::string_view kVoiceOverString{"VoiceOver"};
static constexpr std::string_view kZdsrString{"Zdsr"};
static constexpr std::string_view kWinMagnifierString{"Magnifier"};
static constexpr std::string_view kZoomTextString{"ZoomText"};
static constexpr std::string_view kGenericScreenReaderString{
    "GenericScreenReader"};
}  // namespace

bool IsScreenReader(AssistiveTech assistive_tech) {
  switch (assistive_tech) {
    // On some operating systems, we don't know if a screen reader is running
    // until some expensive operations are performed off-thread.
    // assume there is a not screen reader in this case, as this is generally
    // the most appropriate for most call sites.
    case AssistiveTech::kUninitialized:
    case AssistiveTech::kNone:
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
    // ZoomText And Windows Magnifier are screen magnifier with some screen
    // reader features, such as the ability to navigate by heading.
    case AssistiveTech::kWinMagnifier:
    case AssistiveTech::kZoomText:
    case AssistiveTech::kGenericScreenReader:
      return true;
  }
}

std::string_view GetAssistiveTechString(AssistiveTech assistive_tech) {
  switch (assistive_tech) {
    case AssistiveTech::kNone:
      return kNoneString;
    case AssistiveTech::kUninitialized:
      return kUninitializedString;
    case AssistiveTech::kChromeVox:
      return kChromeVoxString;
    case AssistiveTech::kJaws:
      return kJawsString;
    case AssistiveTech::kNarrator:
      return kNarratorString;
    case AssistiveTech::kNvda:
      return kNvdaString;
    case AssistiveTech::kOrca:
      return kOrcaString;
    case AssistiveTech::kSupernova:
      return kSupernovaString;
    case AssistiveTech::kTalkback:
      return kTalkbackString;
    case AssistiveTech::kVoiceOver:
      return kVoiceOverString;
    case AssistiveTech::kZdsr:
      return kZdsrString;
    case AssistiveTech::kWinMagnifier:
      return kWinMagnifierString;
    case AssistiveTech::kZoomText:
      return kZoomTextString;
    case AssistiveTech::kGenericScreenReader:
      return kGenericScreenReaderString;
  }
}

}  // namespace ui
