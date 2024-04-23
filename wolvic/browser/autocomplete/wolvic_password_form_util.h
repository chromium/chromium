// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_AUTOCOMPLETE_WOLVIC_PASSWORD_FORM_UTIL_H_
#define WOLVIC_BROWSER_AUTOCOMPLETE_WOLVIC_PASSWORD_FORM_UTIL_H_

#include "base/android/scoped_java_ref.h"
#include "components/password_manager/core/browser/password_form.h"

namespace wolvic {

base::android::ScopedJavaLocalRef<jobject>
CreatePasswordFormJavaObject(
    JNIEnv* env, const password_manager::PasswordForm& password_form);

void SetGuidToPasswordFormJavaObject(
    JNIEnv* env,
    base::android::ScopedJavaLocalRef<jobject> j_password_form,
    std::string guid);

base::android::ScopedJavaLocalRef<jobjectArray>
CreatePasswordFormJavaArray(JNIEnv* env, int size);

password_manager::PasswordForm GetPasswordFormFromJavaObject(
    JNIEnv* env,
    base::android::ScopedJavaLocalRef<jobject> j_password_form);

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_AUTOCOMPLETE_WOLVIC_PASSWORD_FORM_UTIL_H_
