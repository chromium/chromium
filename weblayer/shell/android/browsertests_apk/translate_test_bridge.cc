// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/shell/android/browsertests_apk/translate_test_bridge.h"

#include "base/android/jni_android.h"
#include "weblayer/test/weblayer_browsertests_jni/TranslateTestBridge_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace weblayer {

// static
void TranslateTestBridge::SelectButton(
    TranslateCompactInfoBar* infobar,
    TranslateCompactInfoBar::ActionType action_type) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TranslateTestBridge_selectTab(env, infobar->GetJavaInfoBar(),
                                     action_type);
}

// static
void TranslateTestBridge::ClickOverflowMenuItem(
    TranslateCompactInfoBar* infobar,
    OverflowMenuItemId item_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  switch (item_id) {
    case OverflowMenuItemId::NEVER_TRANSLATE_LANGUAGE:
      Java_TranslateTestBridge_clickNeverTranslateLanguageMenuItem(
          env, infobar->GetJavaInfoBar());
      return;
    case OverflowMenuItemId::NEVER_TRANSLATE_SITE:
      Java_TranslateTestBridge_clickNeverTranslateSiteMenuItem(
          env, infobar->GetJavaInfoBar());
      return;
  }
}

}  // namespace weblayer
