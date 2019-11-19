// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/device_form_factor.h"

#include "base/android/jni_android.h"
#include "ui/base/ui_base_jni_headers/DeviceFormFactor_jni.h"

namespace ui {

DeviceFormFactor GetDeviceFormFactor() {
  bool is_tablet =
      Java_DeviceFormFactor_isTablet(base::android::AttachCurrentThread());
  return is_tablet ? DEVICE_FORM_FACTOR_TABLET : DEVICE_FORM_FACTOR_PHONE;
}

}  // namespace ui
