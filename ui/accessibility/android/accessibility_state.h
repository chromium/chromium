// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_ANDROID_ACCESSIBILITY_STATE_H_
#define UI_ACCESSIBILITY_ANDROID_ACCESSIBILITY_STATE_H_

#include <vector>
#include "base/no_destructor.h"

namespace ui {

// Provides methods for measuring accessibility state on Android via
// org.chromium.ui.accessibility.AccessibilityState.
class AccessibilityState {
 public:
  // Provides an interface for clients to listen to animator duration scale
  // changes.
  class Delegate {
   public:
    // Called when the animator duration scale changes.
    virtual void OnAnimatorDurationScaleChanged() = 0;
  };

  // Registers a delegate to listen to animator duration scale changes.
  static void RegisterAnimatorDurationScaleDelegate(Delegate* delegate);

  // Unregisters a delegate to listen to animator duration scale changes.
  static void UnregisterAnimatorDurationScaleDelegate(Delegate* delegate);

  // Notifies all delegates of an animator duration scale change.
  static void NotifyAnimatorDurationScaleObservers();

  // --------------------------------------------------------------------------
  // Methods that call into AccessibilityState.java via JNI
  // --------------------------------------------------------------------------

  // Register Java-side Android accessibility state observers and ensure
  // AccessibilityState is initialized.
  static void RegisterObservers();

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

 private:
  // Returns the static vector of Delegates.
  static std::vector<Delegate*> GetDelegates() {
    static base::NoDestructor<std::vector<Delegate*>> delegates;
    return *delegates;
  }
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_ANDROID_ACCESSIBILITY_STATE_H_
