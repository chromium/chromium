// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/l10n_util_android.h"

#include <stdint.h>

#include <atomic>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/rtl.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/base/ui_base_jni_headers/LocalizationUtils_jni.h"

namespace l10n_util {

namespace {

// -1: uncached, 0: false (LTR), 1: true (RTL).
constinit std::atomic<int> g_layout_rtl_cache{-1};

}  // namespace

static int32_t JNI_LocalizationUtils_GetFirstStrongCharacterDirection(
    const std::u16string& string) {
  return base::i18n::GetFirstStrongCharacterDirection(string);
}

static void JNI_LocalizationUtils_InvalidateLayoutRtlCacheForTesting() {
  g_layout_rtl_cache.store(-1, std::memory_order_relaxed);
}

bool IsLayoutRtl() {
  int cached = g_layout_rtl_cache.load(std::memory_order_relaxed);
  if (cached == -1) {
    JNIEnv* env = base::android::AttachCurrentThread();
    cached = Java_LocalizationUtils_isLayoutRtlForNative(env) ? 1 : 0;
    g_layout_rtl_cache.store(cached, std::memory_order_relaxed);
  }

  return cached != 0;
}

bool ShouldMirrorBackForwardGestures() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return static_cast<bool>(
      Java_LocalizationUtils_shouldMirrorBackForwardGestures(env));
}

void SetRtlForTesting(bool is_rtl) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_LocalizationUtils_setRtlForTesting(env, is_rtl);  // IN-TEST
}

std::u16string GetDisplayNameForLocale(
    const base::i18n::LanguageTag& locale,
    const base::i18n::LanguageTag& display_locale) {
  JNIEnv* env = base::android::AttachCurrentThread();
  jni_zero::ScopedJavaLocalRef<jobject> java_locale =
      Java_LocalizationUtils_getJavaLocaleForBcp47Tag(env, locale.tag_string());
  jni_zero::ScopedJavaLocalRef<jobject> java_display_locale =
      Java_LocalizationUtils_getJavaLocaleForBcp47Tag(
          env, display_locale.tag_string());

  return Java_LocalizationUtils_getDisplayNameForLocale(env, java_locale,
                                                        java_display_locale);
}

static std::string JNI_LocalizationUtils_GetNativeUiLocale() {
  return base::i18n::GetConfiguredLocale();
}

}  // namespace l10n_util

DEFINE_JNI(LocalizationUtils)
