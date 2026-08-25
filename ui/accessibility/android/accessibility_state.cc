// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/android/accessibility_state.h"

#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/accessibility/ax_jni_headers/AccessibilityAutofillHelper_jni.h"
#include "ui/accessibility/ax_jni_headers/AccessibilityState_jni.h"

namespace ui {

namespace {

// Cached value of whether Samsung TalkBack is enabled, pushed from Java via
// `JNI_AccessibilityState_OnSamsungTalkBackStateChanged`. We cache this value
// here in C++ to avoid making synchronous JNI calls into Java on hot
// accessibility node inspection paths.
//
// Thread safety: Written from Java via JNI on the UI thread and read by
// accessibility tree node builders on the UI thread.
bool g_is_samsung_talkback_enabled = false;

}  // namespace

static void JNI_AccessibilityState_OnAnimatorDurationScaleChanged(JNIEnv* env) {
  AccessibilityState::Get()->NotifyAnimatorDurationScaleObservers();
}

static void JNI_AccessibilityState_OnDisplayInversionEnabledChanged(
    JNIEnv* env,
    bool enabled) {
  AccessibilityState::Get()->NotifyDisplayInversionEnabledObservers(enabled);
}

static void JNI_AccessibilityState_OnContrastLevelChanged(
    JNIEnv* env,
    bool highContrastEnabled) {
  AccessibilityState::Get()->NotifyContrastLevelObservers(highContrastEnabled);
}

static void JNI_AccessibilityState_OnTextCursorBlinkIntervalChanged(
    JNIEnv* env,
    int32_t newIntervalMs) {
  AccessibilityState::Get()->NotifyTextCursorBlinkIntervalObservers(
      base::Milliseconds(newIntervalMs));
}

static void JNI_AccessibilityState_RecordAccessibilityServiceInfoHistograms(
    JNIEnv* env) {
  AccessibilityState::Get()->NotifyRecordAccessibilityServiceInfoHistogram();
}

static void JNI_AccessibilityState_OnSamsungTalkBackStateChanged(
    JNIEnv* env,
    bool enabled) {
  g_is_samsung_talkback_enabled = enabled;
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
std::vector<bool> AccessibilityState::GetAccessibilityToolFlags() {
  JNIEnv* const env = base::android::AttachCurrentThread();
  std::vector<bool> tool_flags;
  base::android::JavaBooleanArrayToBoolVector(
      env, Java_AccessibilityState_getAccessibilityToolFlags(env), &tool_flags);
  return tool_flags;
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
bool AccessibilityState::IsSamsungTalkBackEnabled() {
  return g_is_samsung_talkback_enabled;
}

ScopedSamsungTalkBackForTesting::ScopedSamsungTalkBackForTesting(bool enabled)
    : previous_value_(std::exchange(g_is_samsung_talkback_enabled, enabled)) {}

ScopedSamsungTalkBackForTesting::~ScopedSamsungTalkBackForTesting() {
  g_is_samsung_talkback_enabled = previous_value_;
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
