// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_ANDROID_ACCESSIBILITY_STATE_H_
#define UI_ACCESSIBILITY_ANDROID_ACCESSIBILITY_STATE_H_

#include <vector>

namespace ui {

// Provides methods for measuring accessibility state on Android via
// org.chromium.ui.accessibility.AccessibilityState.
class AccessibilityState {
 public:
  // Provides an interface for clients to listen to animator duration scale
  // changes.
  class AccessibilityStateDelegate {
   public:
    // Called when the animator duration scale changes.
    virtual void OnAnimatorDurationScaleChanged() = 0;

    // Called when the display inversion state changes.
    virtual void OnDisplayInversionEnabledChanged(bool enabled) = 0;

    // Called when the contrast level changes.
    virtual void OnContrastLevelChanged(bool highContrastEnabled) = 0;

    // Called during browser startup and any time enabled services change.
    virtual void RecordAccessibilityServiceInfoHistograms() = 0;
  };

  static void RegisterAccessibilityStateDelegate(
      AccessibilityStateDelegate* delegate);

  static void UnregisterAccessibilityStateDelegate(
      AccessibilityStateDelegate* delegate);

  // Notifies all delegates of an animator duration scale change.
  static void NotifyAnimatorDurationScaleObservers();

  // Notifies all delegates of a display inversion state change.
  static void NotifyDisplayInversionEnabledObservers(bool enabled);

  // Notifies all delegates of a contrast level change.
  static void NotifyContrastLevelObservers(bool highContrastEnabled);

  // Notifies all delegates to record service info histograms.
  static void NotifyRecordAccessibilityServiceInfoHistogram();

  // --------------------------------------------------------------------------
  // Methods that call into AccessibilityState.java via JNI
  // --------------------------------------------------------------------------

  // Returns the event mask of all running accessibility services.
  static int GetAccessibilityServiceEventTypeMask();

  // Returns the feedback type mask of all running accessibility services.
  static int GetAccessibilityServiceFeedbackTypeMask();

  // Returns the flags mask of all running accessibility services.
  static int GetAccessibilityServiceFlagsMask();

  // Returns the capabilities mask of all running accessibility services.
  static int GetAccessibilityServiceCapabilitiesMask();

  // Returns a vector containing the IDs of all running accessibility services.
  static std::vector<std::string> GetAccessibilityServiceIds();

  // --------------------------------------------------------------------------
  // Methods that call into AccessibilityAutofillHelper.java via JNI
  // --------------------------------------------------------------------------

  // Returns true if this instance should respect the displayed password text
  // (available in the shadow DOM), false if it should return bullets. Default
  // false.
  static bool ShouldRespectDisplayedPasswordText();
  static void ForceRespectDisplayedPasswordTextForTesting();

  // Returns true if this instance should expose password text to AT (e.g. as a
  // user is typing in a field), false if it should return bullets. Default
  // true.
  static bool ShouldExposePasswordText();
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_ANDROID_ACCESSIBILITY_STATE_H_
