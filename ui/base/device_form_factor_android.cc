// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/device_form_factor.h"

#include "base/android/build_info.h"
#include "base/android/jni_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/base/ui_base_jni_headers/DeviceFormFactor_jni.h"

namespace ui {

// TODO(crbug.com/40941316): Need to land a long-term solution to return
// foldable, either by exposing ui_mode or returning foldable here after
// auditing usages. Currently we are temporarily returning the foldable form
// factor in VariationsServiceClient::GetCurrentFormFactor() and
// FormFactorMetricsProvider::GetFormFactor() for UMA.
DeviceFormFactor GetDeviceFormFactor() {
  if (base::android::BuildInfo::GetInstance()->is_tv()) {
    return DEVICE_FORM_FACTOR_TV;
  }

  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    return DEVICE_FORM_FACTOR_AUTOMOTIVE;
  }

  if (base::android::BuildInfo::GetInstance()->is_desktop()) {
    return DEVICE_FORM_FACTOR_DESKTOP;
  }

  if (Java_DeviceFormFactor_isTablet(base::android::AttachCurrentThread())) {
    return DEVICE_FORM_FACTOR_TABLET;
  }

  return DEVICE_FORM_FACTOR_PHONE;
}

}  // namespace ui
