// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/accessibility_switches.h"

#include "base/command_line.h"
#include "build/build_config.h"

namespace switches {

// Shows additional automatic click features that haven't launched yet.
const char kEnableExperimentalAccessibilityAutoclick[] =
    "enable-experimental-accessibility-autoclick";

// Enables the experimental dictation extension on Chrome OS that hasn't
// launched yet.
const char kEnableExperimentalAccessibilityDictationExtension[] =
    "enable-experimental-accessibility-dictation-extension";

const char kEnableExperimentalAccessibilityDictationOffline[] =
    "enable-experimental-accessibility-dictation-offline";

// Enables support for visually debugging the accessibility labels
// feature, which provides images descriptions for screen reader users.
const char kEnableExperimentalAccessibilityLabelsDebugging[] =
    "enable-experimental-accessibility-labels-debugging";

// Enables language detection on in-page text content which is then exposed to
// assistive technology such as screen readers.
const char kEnableExperimentalAccessibilityLanguageDetection[] =
    "enable-experimental-accessibility-language-detection";

// Enables language detection for dynamic content which is then exposed to
// assistive technology such as screen readers.
const char kEnableExperimentalAccessibilityLanguageDetectionDynamic[] =
    "enable-experimental-accessibility-language-detection-dynamic";

// Enables in progress Switch Access features for text input.
const char kEnableExperimentalAccessibilitySwitchAccessText[] =
    "enable-experimental-accessibility-switch-access-text";

// Enables Switch Access point scanning. This feature hasn't launched yet.
const char kEnableSwitchAccessPointScanning[] =
    "enable-switch-access-point-scanning";

// Enables the Switch Access setup guide that hasn't launched yet.
const char kEnableExperimentalAccessibilitySwitchAccessSetupGuide[] =
    "enable-experimental-accessibility-switch-access-setup-guide";

bool IsExperimentalAccessibilityDictationExtensionEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableExperimentalAccessibilityDictationExtension);
}

bool IsExperimentalAccessibilityDictationOfflineEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableExperimentalAccessibilityDictationOffline);
}

bool IsExperimentalAccessibilityLanguageDetectionEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableExperimentalAccessibilityLanguageDetection);
}

bool IsExperimentalAccessibilityLanguageDetectionDynamicEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableExperimentalAccessibilityLanguageDetectionDynamic);
}

bool IsExperimentalAccessibilitySwitchAccessTextEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableExperimentalAccessibilitySwitchAccessText);
}

bool IsSwitchAccessPointScanningEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableSwitchAccessPointScanning);
}

#if defined(OS_WIN)
// Enables UI Automation platform API in addition to the IAccessible API.
const char kEnableExperimentalUIAutomation[] =
    "enable-experimental-ui-automation";
#endif

bool IsExperimentalAccessibilityPlatformUIAEnabled() {
#if defined(OS_WIN)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableExperimentalUIAutomation);
#else
  return false;
#endif
}

// Optionally disable AXMenuList, which makes the internal pop-up menu
// UI for a select element directly accessible.
const char kDisableAXMenuList[] = "disable-ax-menu-list";

}  // namespace switches
