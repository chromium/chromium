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
  auto assign_if_present = [&](AndroidColorRole role, ColorId id) {
    size_t index = static_cast<size_t>(role);
    if (index < colors.size() && colors[index].has_value()) {
      mixer[id] = {colors[index].value()};
    }
  };

  // TODO(crbug.com/537488418, crbug.com/537488598): Add missing color mappings
  // and move mapping under ui/color.
  assign_if_present(AndroidColorRole::kPrimary, kColorSysPrimary);
  assign_if_present(AndroidColorRole::kOnPrimary, kColorSysOnPrimary);
  assign_if_present(AndroidColorRole::kPrimaryContainer,
                    kColorSysPrimaryContainer);
  assign_if_present(AndroidColorRole::kOnPrimaryContainer,
                    kColorSysOnPrimaryContainer);
  assign_if_present(AndroidColorRole::kSecondary, kColorSysSecondary);
  assign_if_present(AndroidColorRole::kOnSecondary, kColorSysOnSecondary);
  assign_if_present(AndroidColorRole::kSecondaryContainer,
                    kColorSysSecondaryContainer);
  assign_if_present(AndroidColorRole::kOnSecondaryContainer,
                    kColorSysOnSecondaryContainer);
  assign_if_present(AndroidColorRole::kTertiary, kColorSysTertiary);
  assign_if_present(AndroidColorRole::kOnTertiary, kColorSysOnTertiary);
  assign_if_present(AndroidColorRole::kTertiaryContainer,
                    kColorSysTertiaryContainer);
  assign_if_present(AndroidColorRole::kOnTertiaryContainer,
                    kColorSysOnTertiaryContainer);
  assign_if_present(AndroidColorRole::kBackground, kColorSysBase);
  assign_if_present(AndroidColorRole::kSurface, kColorSysSurface);
  assign_if_present(AndroidColorRole::kOnSurface, kColorSysOnSurface);
  assign_if_present(AndroidColorRole::kSurfaceVariant, kColorSysSurfaceVariant);
  assign_if_present(AndroidColorRole::kOnSurfaceVariant,
                    kColorSysOnSurfaceVariant);
  assign_if_present(AndroidColorRole::kOutline, kColorSysOutline);
  assign_if_present(AndroidColorRole::kError, kColorSysError);
  assign_if_present(AndroidColorRole::kOnError, kColorSysOnError);
  assign_if_present(AndroidColorRole::kErrorContainer, kColorSysErrorContainer);
  assign_if_present(AndroidColorRole::kOnErrorContainer,
                    kColorSysOnErrorContainer);
  assign_if_present(AndroidColorRole::kInverseSurface, kColorSysInverseSurface);
  assign_if_present(AndroidColorRole::kInverseOnSurface,
                    kColorSysInverseOnSurface);
  assign_if_present(AndroidColorRole::kInversePrimary, kColorSysInversePrimary);
}

}  // namespace ui
