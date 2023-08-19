// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/device_form_factor.h"

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "ui/base/ui_base_jni_headers/DeviceFormFactor_jni.h"

namespace ui {

DeviceFormFactor GetDeviceFormFactor() {
  if (base::android::BuildInfo::GetInstance()->is_tv()) {
    return DEVICE_FORM_FACTOR_TV;
  }

  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    return DEVICE_FORM_FACTOR_AUTOMOTIVE;
  }

  if (Java_DeviceFormFactor_isTablet(base::android::AttachCurrentThread())) {
    return DEVICE_FORM_FACTOR_TABLET;
  }

  return DEVICE_FORM_FACTOR_PHONE;
}

}  // namespace ui
