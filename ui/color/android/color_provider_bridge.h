// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_ANDROID_COLOR_PROVIDER_BRIDGE_H_
#define UI_COLOR_ANDROID_COLOR_PROVIDER_BRIDGE_H_

#include <optional>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/component_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/android/android_color_roles.h"

namespace ui {

class COMPONENT_EXPORT(COLOR) ColorProviderBridge {
 public:
  ColorProviderBridge() = delete;
  ~ColorProviderBridge() = delete;

  static std::vector<std::optional<SkColor>> GetThemeColors(
      const base::android::JavaRef<jobject>& context);
};

}  // namespace ui

#endif  // UI_COLOR_ANDROID_COLOR_PROVIDER_BRIDGE_H_
