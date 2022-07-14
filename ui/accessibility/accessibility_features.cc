// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/accessibility_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

namespace features {

// Enable recognizing "aria-virtualcontent" as a valid aria property.
const base::Feature kEnableAccessibilityAriaVirtualContent{
    "AccessibilityAriaVirtualContent", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsAccessibilityAriaVirtualContentEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kEnableAccessibilityAriaVirtualContent);
}

// Enable exposing the <html> element to the browser process AXTree
// (as an ignored node).
const base::Feature kEnableAccessibilityExposeHTMLElement{
    "AccessibilityExposeHTMLElement", base::FEATURE_ENABLED_BY_DEFAULT};

bool IsAccessibilityExposeHTMLElementEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kEnableAccessibilityExposeHTMLElement);
}

// Enable exposing ignored nodes from Blink to the browser process AXTree.
// This will allow us to simplify logic by eliminating the distiction between
// "ignored and included in the tree" from "ignored and not included in the
// tree".
const base::Feature kEnableAccessibilityExposeIgnoredNodes{
    "AccessibilityExposeIgnoredNodes", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsAccessibilityExposeIgnoredNodesEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kEnableAccessibilityExposeIgnoredNodes);
}

// Enable language detection to determine language used in page text, exposed
// on the browser process AXTree.
const base::Feature kEnableAccessibilityLanguageDetection{
    "AccessibilityLanguageDetection", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsAccessibilityLanguageDetectionEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kEnableAccessibilityLanguageDetection);
}

// Serializes accessibility information from the Views tree and deserializes it
// into an AXTree in the browser process.
const base::Feature kEnableAccessibilityTreeForViews{
    "AccessibilityTreeForViews", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsAccessibilityTreeForViewsEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kEnableAccessibilityTreeForViews);
}

const base::Feature kAccessibilityFocusHighlight{
    "AccessibilityFocusHighlight", base::FEATURE_ENABLED_BY_DEFAULT};

bool IsAccessibilityFocusHighlightEnabled() {
  return base::FeatureList::IsEnabled(::features::kAccessibilityFocusHighlight);
}

const base::Feature kAutoDisableAccessibility{
    "AutoDisableAccessibility", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsAutoDisableAccessibilityEnabled() {
  return base::FeatureList::IsEnabled(::features::kAutoDisableAccessibility);
}

#if BUILDFLAG(IS_WIN)
const base::Feature kIChromeAccessible{"IChromeAccessible",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

bool IsIChromeAccessibleEnabled() {
  return base::FeatureList::IsEnabled(::features::kIChromeAccessible);
}

const base::Feature kSelectiveUIAEnablement{"SelectiveUIAEnablement",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Returns true if accessibility will be selectively enabled depending on the
// UIA APIs that are called, allowing non-screenreader usage to enable less of
// the accessibility system.
bool IsSelectiveUIAEnablementEnabled() {
  return base::FeatureList::IsEnabled(::features::kSelectiveUIAEnablement);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const base::Feature kMagnifierContinuousMouseFollowingModeSetting{
    "MagnifierContinuousMouseFollowingModeSetting",
    base::FEATURE_ENABLED_BY_DEFAULT};

bool IsMagnifierContinuousMouseFollowingModeSettingEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kMagnifierContinuousMouseFollowingModeSetting);
}

const base::Feature kDockedMagnifierResizing{"DockedMagnifierResizing",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

bool IsDockedMagnifierResizingEnabled() {
  return base::FeatureList::IsEnabled(::features::kDockedMagnifierResizing);
}

bool IsDictationOfflineAvailable() {
  return base::FeatureList::IsEnabled(
      ash::features::kOnDeviceSpeechRecognition);
}

const base::Feature kExperimentalAccessibilityDictationWithPumpkin{
    "ExperimentalAccessibilityDictationWithPumpkin",
    base::FEATURE_DISABLED_BY_DEFAULT};

bool IsExperimentalAccessibilityDictationWithPumpkinEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kExperimentalAccessibilityDictationWithPumpkin);
}

const base::Feature kExperimentalAccessibilityGoogleTtsLanguagePacks{
    "ExperimentalAccessibilityGoogleTtsLanguagePacks",
    base::FEATURE_DISABLED_BY_DEFAULT};

bool IsExperimentalAccessibilityGoogleTtsLanguagePacksEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kExperimentalAccessibilityGoogleTtsLanguagePacks);
}

const base::Feature kEnhancedNetworkVoices{"EnhancedNetworkVoices",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

bool IsEnhancedNetworkVoicesEnabled() {
  return base::FeatureList::IsEnabled(::features::kEnhancedNetworkVoices);
}

const base::Feature kAccessibilityOSSettingsVisibility{
    "AccessibilityOSSettingsVisibility", base::FEATURE_ENABLED_BY_DEFAULT};

bool IsAccessibilityOSSettingsVisibilityEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityOSSettingsVisibility);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const base::Feature kAugmentExistingImageLabels{
    "AugmentExistingImageLabels", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsAugmentExistingImageLabelsEnabled() {
  return base::FeatureList::IsEnabled(::features::kAugmentExistingImageLabels);
}

const base::Feature kUseAXPositionForDocumentMarkers{
    "UseAXPositionForDocumentMarkers", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsUseAXPositionForDocumentMarkersEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kUseAXPositionForDocumentMarkers);
}

const base::Feature kEnableAriaElementReflection{
    "EnableAriaElementReflection", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsAriaElementReflectionEnabled() {
  return base::FeatureList::IsEnabled(::features::kEnableAriaElementReflection);
}

#if BUILDFLAG(IS_ANDROID)
const base::Feature kComputeAXMode{"ComputeAXMode",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

bool IsComputeAXModeEnabled() {
  return base::FeatureList::IsEnabled(::features::kComputeAXMode);
}
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
const base::Feature kReadAnything{"ReadAnything",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

bool IsReadAnythingEnabled() {
  return base::FeatureList::IsEnabled(::features::kReadAnything);
}

const base::Feature kReadAnythingWithScreen2x{
    "ReadAnythingWithScreen2x", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsReadAnythingWithScreen2xEnabled() {
  return base::FeatureList::IsEnabled(::features::kReadAnythingWithScreen2x);
}

const base::Feature kScreenAI{"ScreenAI", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsScreenAIVisualAnnotationsEnabled() {
  return base::FeatureList::IsEnabled(::features::kScreenAI);
}

bool IsScreenAIServiceNeeded() {
  return IsPdfOcrEnabled() || IsScreenAIVisualAnnotationsEnabled() ||
         IsReadAnythingWithScreen2xEnabled();
}

// This feature is only for debug purposes and for security/privacy reasons,
// should be never enabled by default .
const base::Feature kScreenAIDebugMode{"ScreenAIDebugMode",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

bool IsScreenAIDebugModeEnabled() {
  return base::FeatureList::IsEnabled(::features::kScreenAIDebugMode);
}

const base::Feature kPdfOcr{"PdfOcr", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsPdfOcrEnabled() {
  return base::FeatureList::IsEnabled(::features::kPdfOcr);
}

const base::Feature kTextBasedAudioDescription{
    "TextBasedAudioDescription", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsTextBasedAudioDescriptionEnabled() {
  return base::FeatureList::IsEnabled(::features::kTextBasedAudioDescription);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace features
