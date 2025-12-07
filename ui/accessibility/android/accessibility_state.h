// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_ANDROID_ACCESSIBILITY_STATE_H_
#define UI_ACCESSIBILITY_ANDROID_ACCESSIBILITY_STATE_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"

namespace ui {

// Provides methods for measuring accessibility state on Android via
// org.chromium.ui.accessibility.AccessibilityState.
class COMPONENT_EXPORT(AX_BASE_ANDROID) AccessibilityState {
 public:
  class AccessibilityStateObserver : public base::CheckedObserver {
   public:
    // Called when the animator duration scale changes.
    virtual void OnAnimatorDurationScaleChanged() {}

    // Called when the display inversion state changes.
    virtual void OnDisplayInversionEnabledChanged(bool enabled) {}

    // Called when the contrast level changes.
    virtual void OnContrastLevelChanged(bool high_contrast_enabled) {}

    // Called when the text cursor blink interval changes.
    virtual void OnTextCursorBlinkIntervalChanged(
        base::TimeDelta text_cursor_blink_interval) {}

    // Called during browser startup and any time enabled services change.
    virtual void RecordAccessibilityServiceInfoHistograms() {}
  };

  static AccessibilityState* Get();

  void AddObserver(AccessibilityStateObserver* observer);
  void RemoveObserver(AccessibilityStateObserver* observer);

  // Notifies all delegates of an animator duration scale change.
  void NotifyAnimatorDurationScaleObservers();

  // Notifies all delegates of a display inversion state change.
  void NotifyDisplayInversionEnabledObservers(bool enabled);

  // Notifies all delegates of a contrast level change.
  void NotifyContrastLevelObservers(bool high_contrast_enabled);

  // Notifies all delegates of a cursor blink interval change.
  void NotifyTextCursorBlinkIntervalObservers(base::TimeDelta new_interval);

  // Notifies all delegates to record service info histograms.
  void NotifyRecordAccessibilityServiceInfoHistogram();

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

  // Returns the OS-level setting for the text cursor blink interval.
  static base::TimeDelta GetTextCursorBlinkInterval();

  // Returns true when the user has set the OS-level setting to reduce motion.
  static bool PrefersReducedMotion();

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

 private:
  friend class base::NoDestructor<AccessibilityState>;  // For constructor.

  AccessibilityState();
  ~AccessibilityState();

  base::ObserverList<AccessibilityStateObserver> observers_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_ANDROID_ACCESSIBILITY_STATE_H_
