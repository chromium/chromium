// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BLUETOOTH_WEBLAYER_BLUETOOTH_SCANNING_PROMPT_ANDROID_DELEGATE_H_
#define WEBLAYER_BROWSER_BLUETOOTH_WEBLAYER_BLUETOOTH_SCANNING_PROMPT_ANDROID_DELEGATE_H_

#include "components/permissions/android/bluetooth_scanning_prompt_android_delegate.h"

#include "base/android/scoped_java_ref.h"

namespace weblayer {

// The implementation of BluetoothScanningPromptAndroidDelegate for WebLayer.
class WebLayerBluetoothScanningPromptAndroidDelegate
    : public permissions::BluetoothScanningPromptAndroidDelegate {
 public:
  WebLayerBluetoothScanningPromptAndroidDelegate();

  WebLayerBluetoothScanningPromptAndroidDelegate(
      const WebLayerBluetoothScanningPromptAndroidDelegate&) = delete;
  WebLayerBluetoothScanningPromptAndroidDelegate& operator=(
      const WebLayerBluetoothScanningPromptAndroidDelegate&) = delete;

  ~WebLayerBluetoothScanningPromptAndroidDelegate() override;

  // permissions::BluetoothScanningPromptAndroidDelegate implementation:
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;
  security_state::SecurityLevel GetSecurityLevel(
      content::WebContents* web_contents) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BLUETOOTH_WEBLAYER_BLUETOOTH_SCANNING_PROMPT_ANDROID_DELEGATE_H_
