// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/pointer/pointer_device.h"

#include <utility>

#include "base/android/jni_array.h"
#include "base/check_op.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/base/ui_base_jni_headers/TouchDevice_jni.h"

namespace ui {

std::pair<int, int> GetAvailablePointerAndHoverTypesImpl() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  std::vector<int> pointer_and_hover_types;
  base::android::JavaIntArrayToIntVector(
      env, Java_TouchDevice_availablePointerAndHoverTypes(env),
      &pointer_and_hover_types);
  DCHECK_EQ(pointer_and_hover_types.size(), 2u);
  return {pointer_and_hover_types[0], pointer_and_hover_types[1]};
}

TouchScreensAvailability GetTouchScreensAvailability() {
  return TouchScreensAvailability::ENABLED;
}

int MaxTouchPoints() {
  return Java_TouchDevice_maxTouchPoints(jni_zero::AttachCurrentThread());
}

PointerType GetPrimaryPointerType() {
  const int available_pointer_types = GetAvailablePointerAndHoverTypes().first;
  if (available_pointer_types & POINTER_TYPE_COARSE) {
    return POINTER_TYPE_COARSE;
  }
  if (available_pointer_types & POINTER_TYPE_FINE) {
    return POINTER_TYPE_FINE;
  }
  DCHECK_EQ(available_pointer_types, POINTER_TYPE_NONE);
  return POINTER_TYPE_NONE;
}

HoverType GetPrimaryHoverType() {
  const int available_hover_types = GetAvailablePointerAndHoverTypes().second;
  if (available_hover_types & HOVER_TYPE_NONE) {
    return HOVER_TYPE_NONE;
  }
  DCHECK_EQ(available_hover_types, HOVER_TYPE_HOVER);
  return HOVER_TYPE_HOVER;
}

}  // namespace ui

DEFINE_JNI(TouchDevice)
