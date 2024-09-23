// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/android/accessibility_state.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/no_destructor.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/accessibility/ax_jni_headers/AccessibilityAutofillHelper_jni.h"
#include "ui/accessibility/ax_jni_headers/AccessibilityState_jni.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;

namespace ui {

namespace {

// Returns the static vector of Delegates.
std::vector<AccessibilityState::AccessibilityStateDelegate*>& GetDelegates() {
  static base::NoDestructor<
      std::vector<AccessibilityState::AccessibilityStateDelegate*>>
      delegates;
  return *delegates;
}

}  // namespace

// static
void JNI_AccessibilityState_OnAnimatorDurationScaleChanged(JNIEnv* env) {
  AccessibilityState::NotifyAnimatorDurationScaleObservers();
}

// static
void JNI_AccessibilityState_OnDisplayInversionEnabledChanged(JNIEnv* env,
                                                             jboolean enabled) {
  AccessibilityState::NotifyDisplayInversionEnabledObservers((bool)enabled);
}

// static
void JNI_AccessibilityState_RecordAccessibilityServiceInfoHistograms(
    JNIEnv* env) {
  AccessibilityState::NotifyRecordAccessibilityServiceInfoHistogram();
}

// static
void AccessibilityState::RegisterAccessibilityStateDelegate(
    AccessibilityStateDelegate* delegate) {
  GetDelegates().push_back(delegate);
}

// static
void AccessibilityState::UnregisterAccessibilityStateDelegate(
    AccessibilityStateDelegate* delegate) {
  auto& delegates = GetDelegates();
  delegates.erase(std::find(delegates.begin(), delegates.end(), delegate));
}

// static
void AccessibilityState::NotifyAnimatorDurationScaleObservers() {
  for (AccessibilityStateDelegate* delegate : GetDelegates()) {
    delegate->OnAnimatorDurationScaleChanged();
  }
}

// static
void AccessibilityState::NotifyDisplayInversionEnabledObservers(bool enabled) {
  for (AccessibilityStateDelegate* delegate : GetDelegates()) {
    delegate->OnDisplayInversionEnabledChanged(enabled);
  }
}

// static
void AccessibilityState::NotifyContrastLevelObservers(
    bool highContrastEnabled) {
  for (AccessibilityStateDelegate* delegate : GetDelegates()) {
    delegate->OnContrastLevelChanged(highContrastEnabled);
  }
}

// static
void JNI_AccessibilityState_OnContrastLevelChanged(
    JNIEnv* env,
    jboolean highContrastEnabled) {
  AccessibilityState::NotifyContrastLevelObservers((bool)highContrastEnabled);
}

// static
void AccessibilityState::NotifyRecordAccessibilityServiceInfoHistogram() {
  for (AccessibilityStateDelegate* delegate : GetDelegates()) {
    delegate->RecordAccessibilityServiceInfoHistograms();
  }
}

// static
int AccessibilityState::GetAccessibilityServiceEventTypeMask() {
  JNIEnv* env = AttachCurrentThread();
  return Java_AccessibilityState_getAccessibilityServiceEventTypeMask(env);
}

// static
int AccessibilityState::GetAccessibilityServiceFeedbackTypeMask() {
  JNIEnv* env = AttachCurrentThread();
  return Java_AccessibilityState_getAccessibilityServiceFeedbackTypeMask(env);
}

// static
int AccessibilityState::GetAccessibilityServiceFlagsMask() {
  JNIEnv* env = AttachCurrentThread();
  return Java_AccessibilityState_getAccessibilityServiceFlagsMask(env);
}

// static
int AccessibilityState::GetAccessibilityServiceCapabilitiesMask() {
  JNIEnv* env = AttachCurrentThread();
  return Java_AccessibilityState_getAccessibilityServiceCapabilitiesMask(env);
}

// static
std::vector<std::string> AccessibilityState::GetAccessibilityServiceIds() {
  JNIEnv* env = AttachCurrentThread();

  auto j_service_ids = Java_AccessibilityState_getAccessibilityServiceIds(env);
  std::vector<std::string> service_ids;
  AppendJavaStringArrayToStringVector(env, j_service_ids, &service_ids);
  return service_ids;
}

// static
bool AccessibilityState::ShouldRespectDisplayedPasswordText() {
  JNIEnv* env = AttachCurrentThread();
  return Java_AccessibilityAutofillHelper_shouldRespectDisplayedPasswordText(
      env);
}

// static
void AccessibilityState::ForceRespectDisplayedPasswordTextForTesting() {
  JNIEnv* env = AttachCurrentThread();
  Java_AccessibilityAutofillHelper_forceRespectDisplayedPasswordTextForTesting(
      env);
}

// static
bool AccessibilityState::ShouldExposePasswordText() {
  JNIEnv* env = AttachCurrentThread();
  return Java_AccessibilityAutofillHelper_shouldExposePasswordText(env);
}

}  // namespace ui
