// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/l10n_util_android.h"

#include <stdint.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/i18n/rtl.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "third_party/icu/source/common/unicode/uloc.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/base/ui_base_jni_headers/LocalizationUtils_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace l10n_util {

jint JNI_LocalizationUtils_GetFirstStrongCharacterDirection(
    JNIEnv* env,
    const JavaParamRef<jstring>& string) {
  return base::i18n::GetFirstStrongCharacterDirection(
      base::android::ConvertJavaStringToUTF16(env, string));
}

bool IsLayoutRtl() {
  static bool is_layout_rtl_cached = false;
  static bool layout_rtl_cache;

  if (!is_layout_rtl_cached) {
    is_layout_rtl_cached = true;
    JNIEnv* env = base::android::AttachCurrentThread();
    layout_rtl_cache =
        static_cast<bool>(Java_LocalizationUtils_isLayoutRtl(env));
  }

  return layout_rtl_cache;
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

namespace {

// Common prototype of ICU uloc_getXXX() functions.
typedef int32_t (*UlocGetComponentFunc)(const char*, char*, int32_t,
                                        UErrorCode*);

std::string GetLocaleComponent(const std::string& locale,
                               UlocGetComponentFunc uloc_func,
                               size_t max_capacity) {
  std::string result;
  UErrorCode error = U_ZERO_ERROR;
  auto actual_length = base::checked_cast<size_t>(
      uloc_func(locale.c_str(), base::WriteInto(&result, max_capacity),
                base::checked_cast<int32_t>(max_capacity), &error));
  DCHECK(U_SUCCESS(error));
  DCHECK(actual_length < max_capacity);
  result.resize(actual_length);
  return result;
}

ScopedJavaLocalRef<jobject> JNI_LocalizationUtils_NewJavaLocale(
    JNIEnv* env,
    const std::string& locale) {
  // TODO(wangxianzhu): Use new Locale API once Android supports scripts.
  std::string language = GetLocaleComponent(
      locale, uloc_getLanguage, ULOC_LANG_CAPACITY);
  std::string country = GetLocaleComponent(
      locale, uloc_getCountry, ULOC_COUNTRY_CAPACITY);
  std::string variant = GetLocaleComponent(
      locale, uloc_getVariant, ULOC_FULLNAME_CAPACITY);
  return Java_LocalizationUtils_getJavaLocale(
      env, base::android::ConvertUTF8ToJavaString(env, language),
      base::android::ConvertUTF8ToJavaString(env, country),
      base::android::ConvertUTF8ToJavaString(env, variant));
}

}  // namespace

std::u16string GetDisplayNameForLocale(const std::string& locale,
                                       const std::string& display_locale) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_locale =
      JNI_LocalizationUtils_NewJavaLocale(env, locale);
  ScopedJavaLocalRef<jobject> java_display_locale =
      JNI_LocalizationUtils_NewJavaLocale(env, display_locale);

  ScopedJavaLocalRef<jstring> java_result(
      Java_LocalizationUtils_getDisplayNameForLocale(env, java_locale,
                                                     java_display_locale));
  return base::android::ConvertJavaStringToUTF16(java_result);
}

ScopedJavaLocalRef<jstring> JNI_LocalizationUtils_GetNativeUiLocale(
    JNIEnv* env) {
  ScopedJavaLocalRef<jstring> native_ui_locale_string =
      base::android::ConvertUTF8ToJavaString(env,
                                             base::i18n::GetConfiguredLocale());
  return native_ui_locale_string;
}

}  // namespace l10n_util
