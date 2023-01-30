// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/android/accessibility_state.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/no_destructor.h"
#include "ui/accessibility/ax_jni_headers/AccessibilityState_jni.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;

namespace ui {
// static
void AccessibilityState::RegisterObservers() {
  // Setup the listeners for accessibility state changes, so we can
  // inform the renderer about changes.
  JNIEnv* env = AttachCurrentThread();
  ui::Java_AccessibilityState_registerObservers(env);
}

// static
void AccessibilityState::RegisterAnimatorDurationScaleDelegate(
    Delegate* delegate) {
  GetDelegates().push_back(delegate);
}

// static
void AccessibilityState::UnregisterAnimatorDurationScaleDelegate(
    Delegate* delegate) {
  std::vector<Delegate*> delegates = GetDelegates();
  auto it = std::find(delegates.begin(), delegates.end(), delegate);
  delegates.erase(it);
}

// static
int AccessibilityState::GetAccessibilityServiceEventTypeMask() {
  JNIEnv* env = AttachCurrentThread();
  return ui::Java_AccessibilityState_getAccessibilityServiceEventTypeMask(env);
}

// static
int AccessibilityState::GetAccessibilityServiceFeedbackTypeMask() {
  JNIEnv* env = AttachCurrentThread();
  return ui::Java_AccessibilityState_getAccessibilityServiceFeedbackTypeMask(
      env);
}

// static
int AccessibilityState::GetAccessibilityServiceFlagsMask() {
  JNIEnv* env = AttachCurrentThread();
  return ui::Java_AccessibilityState_getAccessibilityServiceFlagsMask(env);
}

// static
int AccessibilityState::GetAccessibilityServiceCapabilitiesMask() {
  JNIEnv* env = AttachCurrentThread();
  return ui::Java_AccessibilityState_getAccessibilityServiceCapabilitiesMask(
      env);
}

// static
std::vector<std::string> AccessibilityState::GetAccessibilityServiceIds() {
  JNIEnv* env = AttachCurrentThread();

  auto j_service_ids =
      ui::Java_AccessibilityState_getAccessibilityServiceIds(env);
  std::vector<std::string> service_ids;
  AppendJavaStringArrayToStringVector(env, j_service_ids, &service_ids);
  return service_ids;
}

// static
bool AccessibilityState::HasSpokenFeedbackServicePresent() {
  JNIEnv* env = AttachCurrentThread();
  return ui::Java_AccessibilityState_isSpokenFeedbackServicePresent(env);
}

// static
void AccessibilityState::NotifyAnimatorDurationScaleObservers() {
  for (Delegate* delegate : GetDelegates())
    delegate->OnAnimatorDurationScaleChanged();
}

// static
std::vector<AccessibilityState::Delegate*> AccessibilityState::GetDelegates() {
  static base::NoDestructor<std::vector<Delegate*>> delegates;
  return *delegates;
}

// static
void JNI_AccessibilityState_OnAnimatorDurationScaleChanged(JNIEnv* env) {
  AccessibilityState::NotifyAnimatorDurationScaleObservers();
}
}  // namespace ui
