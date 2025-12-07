// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/android/accessibility_state.h"

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/accessibility/ax_jni_headers/AccessibilityAutofillHelper_jni.h"
#include "ui/accessibility/ax_jni_headers/AccessibilityState_jni.h"

namespace ui {

static void JNI_AccessibilityState_OnAnimatorDurationScaleChanged(JNIEnv* env) {
  AccessibilityState::Get()->NotifyAnimatorDurationScaleObservers();
}

static void JNI_AccessibilityState_OnDisplayInversionEnabledChanged(
    JNIEnv* env,
    jboolean enabled) {
  AccessibilityState::Get()->NotifyDisplayInversionEnabledObservers(
      static_cast<bool>(enabled));
}

static void JNI_AccessibilityState_OnContrastLevelChanged(
    JNIEnv* env,
    jboolean highContrastEnabled) {
  AccessibilityState::Get()->NotifyContrastLevelObservers(
      static_cast<bool>(highContrastEnabled));
}

static void JNI_AccessibilityState_OnTextCursorBlinkIntervalChanged(
    JNIEnv* env,
    jint newIntervalMs) {
  AccessibilityState::Get()->NotifyTextCursorBlinkIntervalObservers(
      base::Milliseconds(newIntervalMs));
}

static void JNI_AccessibilityState_RecordAccessibilityServiceInfoHistograms(
    JNIEnv* env) {
  AccessibilityState::Get()->NotifyRecordAccessibilityServiceInfoHistogram();
}

// static
AccessibilityState* AccessibilityState::Get() {
  static base::NoDestructor<AccessibilityState> s_accessibility_state;
  return s_accessibility_state.get();
}

void AccessibilityState::AddObserver(AccessibilityStateObserver* observer) {
  observers_.AddObserver(observer);
}

void AccessibilityState::RemoveObserver(AccessibilityStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AccessibilityState::NotifyAnimatorDurationScaleObservers() {
  observers_.Notify(
      &AccessibilityStateObserver::OnAnimatorDurationScaleChanged);
}

void AccessibilityState::NotifyDisplayInversionEnabledObservers(bool enabled) {
  observers_.Notify(
      &AccessibilityStateObserver::OnDisplayInversionEnabledChanged, enabled);
}

void AccessibilityState::NotifyContrastLevelObservers(
    bool high_contrast_enabled) {
  observers_.Notify(&AccessibilityStateObserver::OnContrastLevelChanged,
                    high_contrast_enabled);
}

void AccessibilityState::NotifyTextCursorBlinkIntervalObservers(
    base::TimeDelta new_interval_ms) {
  observers_.Notify(
      &AccessibilityStateObserver::OnTextCursorBlinkIntervalChanged,
      new_interval_ms);
}

void AccessibilityState::NotifyRecordAccessibilityServiceInfoHistogram() {
  observers_.Notify(
      &AccessibilityStateObserver::RecordAccessibilityServiceInfoHistograms);
}

// static
int AccessibilityState::GetAccessibilityServiceEventTypeMask() {
  return Java_AccessibilityState_getAccessibilityServiceEventTypeMask(
      base::android::AttachCurrentThread());
}

// static
int AccessibilityState::GetAccessibilityServiceFeedbackTypeMask() {
  return Java_AccessibilityState_getAccessibilityServiceFeedbackTypeMask(
      base::android::AttachCurrentThread());
}

// static
int AccessibilityState::GetAccessibilityServiceFlagsMask() {
  return Java_AccessibilityState_getAccessibilityServiceFlagsMask(
      base::android::AttachCurrentThread());
}

// static
int AccessibilityState::GetAccessibilityServiceCapabilitiesMask() {
  return Java_AccessibilityState_getAccessibilityServiceCapabilitiesMask(
      base::android::AttachCurrentThread());
}

// static
std::vector<std::string> AccessibilityState::GetAccessibilityServiceIds() {
  JNIEnv* const env = base::android::AttachCurrentThread();
  std::vector<std::string> service_ids;
  base::android::AppendJavaStringArrayToStringVector(
      env, Java_AccessibilityState_getAccessibilityServiceIds(env),
      &service_ids);
  return service_ids;
}

// static
base::TimeDelta AccessibilityState::GetTextCursorBlinkInterval() {
  return base::Milliseconds(Java_AccessibilityState_getTextCursorBlinkInterval(
      base::android::AttachCurrentThread()));
}

// static
bool AccessibilityState::PrefersReducedMotion() {
  return Java_AccessibilityState_prefersReducedMotion(
      base::android::AttachCurrentThread());
}

// static
bool AccessibilityState::ShouldRespectDisplayedPasswordText() {
  return Java_AccessibilityAutofillHelper_shouldRespectDisplayedPasswordText(
      base::android::AttachCurrentThread());
}

// static
void AccessibilityState::ForceRespectDisplayedPasswordTextForTesting() {
  Java_AccessibilityAutofillHelper_forceRespectDisplayedPasswordTextForTesting(
      base::android::AttachCurrentThread());
}

// static
bool AccessibilityState::ShouldExposePasswordText() {
  return Java_AccessibilityAutofillHelper_shouldExposePasswordText(
      base::android::AttachCurrentThread());
}

AccessibilityState::AccessibilityState() = default;

AccessibilityState::~AccessibilityState() = default;

}  // namespace ui

DEFINE_JNI(AccessibilityAutofillHelper)
DEFINE_JNI(AccessibilityState)
