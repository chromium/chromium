// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/bluetooth/weblayer_bluetooth_chooser_android_delegate.h"

#include "base/android/jni_android.h"
#include "components/security_state/content/content_utils.h"
#include "weblayer/browser/java/jni/WebLayerBluetoothChooserAndroidDelegate_jni.h"

namespace weblayer {

WebLayerBluetoothChooserAndroidDelegate::
    WebLayerBluetoothChooserAndroidDelegate() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_delegate_.Reset(
      Java_WebLayerBluetoothChooserAndroidDelegate_create(env));
}

WebLayerBluetoothChooserAndroidDelegate::
    ~WebLayerBluetoothChooserAndroidDelegate() = default;

base::android::ScopedJavaLocalRef<jobject>
WebLayerBluetoothChooserAndroidDelegate::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_delegate_);
}

security_state::SecurityLevel
WebLayerBluetoothChooserAndroidDelegate::GetSecurityLevel(
    content::WebContents* web_contents) {
  auto state = security_state::GetVisibleSecurityState(web_contents);
  return security_state::GetSecurityLevel(
      *state,
      /*used_policy_installed_certificate=*/false);
}

}  // namespace weblayer
