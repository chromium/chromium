// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/android/color_provider_bridge.h"

#include <limits>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/check_op.h"
#include "base/numerics/safe_math.h"
#include "ui/color/android/android_color_roles.h"

// JNI header automatically generated from ColorProviderBridgeFactory.java
#include "ui/color/ui_color_jni_headers/ColorProviderBridgeFactory_jni.h"

namespace ui {

namespace {

// Java's ColorUtils provides this value when it encounters an invalid color.
constexpr int64_t kInvalidJavaColor =
    static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1;

std::optional<SkColor> JavaColorToOptionalSkColor(int64_t java_color) {
  if (java_color == kInvalidJavaColor) {
    return std::nullopt;
  }
  DCHECK(base::IsValueInRangeForNumericType<int32_t>(java_color));
  return static_cast<SkColor>(java_color);
}
}  // namespace

// static
std::vector<std::optional<SkColor>> ColorProviderBridge::GetThemeColors(
    const base::android::JavaRef<jobject>& context) {
  if (!context) {
    return {};
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jlongArray> j_colors =
      Java_ColorProviderBridgeFactory_getThemeColors(env, context);

  if (!j_colors) {
    return {};
  }

  std::vector<int64_t> java_colors;
  base::android::JavaLongArrayToInt64Vector(env, j_colors, &java_colors);

  if (java_colors.empty()) {
    return {};
  }

  CHECK_EQ(java_colors.size(),
           static_cast<size_t>(AndroidColorRole::kMaxValue) + 1);

  std::vector<std::optional<SkColor>> sk_colors;
  sk_colors.reserve(java_colors.size());
  for (int64_t java_color : java_colors) {
    sk_colors.push_back(JavaColorToOptionalSkColor(java_color));
  }

  return sk_colors;
}

}  // namespace ui
