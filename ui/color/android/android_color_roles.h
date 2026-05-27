// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_ANDROID_ANDROID_COLOR_ROLES_H_
#define UI_COLOR_ANDROID_ANDROID_COLOR_ROLES_H_

namespace ui {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.ui.color
// LINT.IfChange(AndroidColorRole)
enum class AndroidColorRole {
  kPrimary = 0,
  kOnPrimary = 1,
  kPrimaryContainer = 2,
  kOnPrimaryContainer = 3,
  kSecondary = 4,
  kOnSecondary = 5,
  kSecondaryContainer = 6,
  kOnSecondaryContainer = 7,
  kTertiary = 8,
  kOnTertiary = 9,
  kTertiaryContainer = 10,
  kOnTertiaryContainer = 11,
  kBackground = 12,
  kOnBackground = 13,
  kSurface = 14,
  kOnSurface = 15,
  kSurfaceVariant = 16,
  kOnSurfaceVariant = 17,
  kOutline = 18,
  kOutlineVariant = 19,
  kError = 20,
  kOnError = 21,
  kErrorContainer = 22,
  kOnErrorContainer = 23,
  kInverseSurface = 24,
  kInverseOnSurface = 25,
  kInversePrimary = 26,
  kMaxValue = kInversePrimary,
};
// LINT.ThenChange(//chrome/android/java/src/org/chromium/chrome/browser/ui/color/ColorProviderBridgeImpl.java:AndroidColorRoleAttrs)

}  // namespace ui

#endif  // UI_COLOR_ANDROID_ANDROID_COLOR_ROLES_H_
