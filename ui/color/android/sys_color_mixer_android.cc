// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/android/sys_color_mixer_android.h"

#include <optional>
#include <vector>

#include "base/android/jni_android.h"
#include "ui/color/android/android_color_roles.h"
#include "ui/color/android/color_provider_bridge.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_recipe.h"

namespace ui {

void AddSysColorMixerAndroid(ColorProvider* provider,
                             const ColorProviderKey& key) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> j_context = key.context.get(env);
  if (!j_context) {
    return;
  }
  std::vector<std::optional<SkColor>> colors =
      ColorProviderBridge::GetThemeColors(j_context);
  if (colors.empty()) {
    return;
  }
  ColorMixer& mixer = provider->AddMixer();
  for (const auto& [role, id] : internal::kAndroidColorRoleToColorId) {
    size_t index = static_cast<size_t>(role);
    if (index < colors.size() && colors[index].has_value()) {
      mixer[id] = {colors[index].value()};
    }
  }
}

}  // namespace ui
