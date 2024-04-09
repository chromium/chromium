// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/autocomplete/wolvic_password_form_util.h"

#include "base/android/jni_string.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "wolvic/jni_headers/PasswordForm_jni.h"
#include "url/origin.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace wolvic {

ScopedJavaLocalRef<jobject> CreatePasswordFormJavaObject(
    JNIEnv* env, const password_manager::PasswordForm& password_form) {
  std::string signon_realm = !password_form.signon_realm.empty()
      ? password_form.signon_realm
      : password_manager::GetSignonRealm(password_form.url);
  return Java_PasswordForm_createPasswordForm(
      env, ConvertUTF16ToJavaString(env, password_form.username_value),
      ConvertUTF16ToJavaString(env, password_form.password_value),
      ConvertUTF8ToJavaString(env, password_form.url.DeprecatedGetOriginAsURL().spec()),
      ConvertUTF8ToJavaString(env, password_form.action.spec()),
      ConvertUTF8ToJavaString(env, signon_realm),
      ConvertUTF8ToJavaString(env, password_form.keychain_identifier));
}

void SetGuidToPasswordFormJavaObject(
    JNIEnv* env, ScopedJavaLocalRef<jobject> j_password_form, std::string guid) {
  Java_PasswordForm_setGuid(env, j_password_form, ConvertUTF8ToJavaString(env, guid));
}

ScopedJavaLocalRef<jobjectArray>
CreatePasswordFormJavaArray(JNIEnv* env, int size) {
  return Java_PasswordForm_createPasswordFormArray(env, size);
}

std::unique_ptr<password_manager::PasswordForm>
GetPasswordFormFromJavaObject(
    JNIEnv* env, ScopedJavaLocalRef<jobject> j_password_form) {
  auto form = std::make_unique<password_manager::PasswordForm>();

  form->scheme = password_manager::PasswordForm::Scheme::kHtml;
  form->username_value = ConvertJavaStringToUTF16(
      Java_PasswordForm_getUsername(env, j_password_form));
  form->password_value = ConvertJavaStringToUTF16(
      Java_PasswordForm_getPassword(env, j_password_form));
  form->url = GURL(ConvertJavaStringToUTF8(
      Java_PasswordForm_getOrigin(env, j_password_form)));

  ScopedJavaLocalRef<jstring> jaction =
      Java_PasswordForm_getFormActionOrigin(env, j_password_form);
  if (jaction.obj() && env->GetStringLength(jaction.obj()) > 0) {
    form->action = GURL(ConvertJavaStringToUTF8(env, jaction));
  }

  ScopedJavaLocalRef<jstring> jsignon_realm =
      Java_PasswordForm_getHttpRealm(env, j_password_form);
  if (jsignon_realm.obj() && env->GetStringLength(jsignon_realm.obj()) > 0) {
    form->signon_realm = ConvertJavaStringToUTF8(env, jsignon_realm);
  } else {
    form->signon_realm = password_manager::GetSignonRealm(form->url);
  }

  ScopedJavaLocalRef<jstring> jguid =
      Java_PasswordForm_getGuid(env, j_password_form);
  if (jguid.obj() && env->GetStringLength(jguid.obj()) > 0) {
    form->keychain_identifier = ConvertJavaStringToUTF8(env, jguid);
  }

  // We always use the profile store.
  form->in_store = password_manager::PasswordForm::Store::kProfileStore;
  return form;
}

}  // namespace wolvic
