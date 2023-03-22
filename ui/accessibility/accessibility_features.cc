// Copyright 2019 The Chromium Authors
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
BASE_FEATURE(kEnableAccessibilityAriaVirtualContent,
             "AccessibilityAriaVirtualContent",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAccessibilityAriaVirtualContentEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kEnableAccessibilityAriaVirtualContent);
}

// Enable exposing the <html> element to the browser process AXTree
// (as an ignored node).
BASE_FEATURE(kEnableAccessibilityExposeHTMLElement,
             "AccessibilityExposeHTMLElement",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsAccessibilityExposeHTMLElementEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kEnableAccessibilityExposeHTMLElement);
}

// Enable exposing ignored nodes from Blink to the browser process AXTree.
// This will allow us to simplify logic by eliminating the distiction between
// "ignored and included in the tree" from "ignored and not included in the
// tree".
BASE_FEATURE(kEnableAccessibilityExposeIgnoredNodes,
             "AccessibilityExposeIgnoredNodes",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAccessibilityExposeIgnoredNodesEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kEnableAccessibilityExposeIgnoredNodes);
}

// Enable language detection to determine language used in page text, exposed
// on the browser process AXTree.
BASE_FEATURE(kEnableAccessibilityLanguageDetection,
             "AccessibilityLanguageDetection",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAccessibilityLanguageDetectionEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kEnableAccessibilityLanguageDetection);
}

// Serializes accessibility information from the Views tree and deserializes it
// into an AXTree in the browser process.
BASE_FEATURE(kEnableAccessibilityTreeForViews,
             "AccessibilityTreeForViews",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAccessibilityTreeForViewsEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kEnableAccessibilityTreeForViews);
}

BASE_FEATURE(kAccessibilityFocusHighlight,
             "AccessibilityFocusHighlight",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsAccessibilityFocusHighlightEnabled() {
  return base::FeatureList::IsEnabled(::features::kAccessibilityFocusHighlight);
}

BASE_FEATURE(kAutoDisableAccessibility,
             "AutoDisableAccessibility",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAutoDisableAccessibilityEnabled() {
  return base::FeatureList::IsEnabled(::features::kAutoDisableAccessibility);
}

BASE_FEATURE(kTextBasedAudioDescription,
             "TextBasedAudioDescription",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTextBasedAudioDescriptionEnabled() {
  return base::FeatureList::IsEnabled(::features::kTextBasedAudioDescription);
}

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kIChromeAccessible,
             "IChromeAccessible",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsIChromeAccessibleEnabled() {
  return base::FeatureList::IsEnabled(::features::kIChromeAccessible);
}

BASE_FEATURE(kSelectiveUIAEnablement,
             "SelectiveUIAEnablement",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Returns true if accessibility will be selectively enabled depending on the
// UIA APIs that are called, allowing non-screenreader usage to enable less of
// the accessibility system.
bool IsSelectiveUIAEnablementEnabled() {
  return base::FeatureList::IsEnabled(::features::kSelectiveUIAEnablement);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsDictationOfflineAvailable() {
  return base::FeatureList::IsEnabled(
      ash::features::kOnDeviceSpeechRecognition);
}

BASE_FEATURE(kExperimentalAccessibilityDictationContextChecking,
             "ExperimentalAccessibilityDictationContextChecking",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsExperimentalAccessibilityDictationContextCheckingEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kExperimentalAccessibilityDictationContextChecking);
}

BASE_FEATURE(kExperimentalAccessibilityGoogleTtsLanguagePacks,
             "ExperimentalAccessibilityGoogleTtsLanguagePacks",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsExperimentalAccessibilityGoogleTtsLanguagePacksEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kExperimentalAccessibilityGoogleTtsLanguagePacks);
}

BASE_FEATURE(kExperimentalAccessibilityColorEnhancementSettings,
             "ExperimentalAccessibilityColorEnhancementSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool AreExperimentalAccessibilityColorEnhancementSettingsEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kExperimentalAccessibilityColorEnhancementSettings);
}

BASE_FEATURE(kAccessibilitySelectToSpeakPageMigration,
             "AccessibilitySelectToSpeakPageMigration",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsAccessibilitySelectToSpeakPageMigrationEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilitySelectToSpeakPageMigration);
}

BASE_FEATURE(kAccessibilityChromeVoxPageMigration,
             "AccessibilityChromeVoxPageMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAccessibilityChromeVoxPageMigrationEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityChromeVoxPageMigration);
}

BASE_FEATURE(kAccessibilitySelectToSpeakPrefsMigration,
             "AccessibilitySelectToSpeakPrefsMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAccessibilitySelectToSpeakPrefsMigrationEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilitySelectToSpeakPrefsMigration);
}

BASE_FEATURE(kAccessibilitySelectToSpeakContextMenuOption,
             "AccessibilitySelectToSpeakContextMenuOption",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsAccessibilitySelectToSpeakContextMenuOptionEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilitySelectToSpeakContextMenuOption);
}

BASE_FEATURE(kAccessibilitySelectToSpeakHoverTextImprovements,
             "AccessibilitySelectToSpeakHoverTextImprovements",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsAccessibilitySelectToSpeakHoverTextImprovementsEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilitySelectToSpeakHoverTextImprovements);
}

BASE_FEATURE(kAccessibilityAcceleratorNotificationsTimeout,
             "AccessibilityAcceleratorNotificationsTimeout",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsAccessibilityAcceleratorNotificationsTimeoutEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityAcceleratorNotificationsTimeout);
}

BASE_FEATURE(kAccessibilityDeprecateChromeVoxTabs,
             "AccessibilityDeprecateChromeVoxTabs",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAccessibilityDeprecateChromeVoxTabsEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityDeprecateChromeVoxTabs);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

BASE_FEATURE(kAugmentExistingImageLabels,
             "AugmentExistingImageLabels",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAugmentExistingImageLabelsEnabled() {
  return base::FeatureList::IsEnabled(::features::kAugmentExistingImageLabels);
}

BASE_FEATURE(kUseAXPositionForDocumentMarkers,
             "UseAXPositionForDocumentMarkers",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsUseAXPositionForDocumentMarkersEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kUseAXPositionForDocumentMarkers);
}

BASE_FEATURE(kEnableAriaElementReflection,
             "EnableAriaElementReflection",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAriaElementReflectionEnabled() {
  return base::FeatureList::IsEnabled(::features::kEnableAriaElementReflection);
}

BASE_FEATURE(kAblateSendPendingAccessibilityEvents,
             "AblateSendPendingAccessibilityEvents",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAblateSendPendingAccessibilityEventsEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAblateSendPendingAccessibilityEvents);
}

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kComputeAXMode,
             "ComputeAXMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsComputeAXModeEnabled() {
  return base::FeatureList::IsEnabled(::features::kComputeAXMode);
}

BASE_FEATURE(kAccessibilityFormControlsMode,
             "AccessibilityFormControlsAXMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAccessibilityFormControlsAXModeEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityFormControlsMode);
}
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kReadAnything, "ReadAnything", base::FEATURE_DISABLED_BY_DEFAULT);

bool IsReadAnythingEnabled() {
  return base::FeatureList::IsEnabled(::features::kReadAnything);
}

BASE_FEATURE(kReadAnythingWithScreen2x,
             "ReadAnythingWithScreen2x",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsReadAnythingWithScreen2xEnabled() {
  return base::FeatureList::IsEnabled(::features::kReadAnythingWithScreen2x);
}

bool IsScreenAIServiceNeeded() {
  return IsPdfOcrEnabled() || IsLayoutExtractionEnabled() ||
         IsReadAnythingWithScreen2xEnabled();
}

// This feature is only for debug purposes and for security/privacy reasons,
// should be never enabled by default .
BASE_FEATURE(kScreenAIDebugMode,
             "ScreenAIDebugMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsScreenAIDebugModeEnabled() {
  return base::FeatureList::IsEnabled(::features::kScreenAIDebugMode);
}

BASE_FEATURE(kPdfOcr, "PdfOcr", base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPdfOcrEnabled() {
  return base::FeatureList::IsEnabled(::features::kPdfOcr);
}

BASE_FEATURE(kLayoutExtraction,
             "LayoutExtraction",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsLayoutExtractionEnabled() {
  return base::FeatureList::IsEnabled(::features::kLayoutExtraction);
}

BASE_FEATURE(kAccessibilityService,
             "AccessibilityService",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAccessibilityServiceEnabled() {
  return base::FeatureList::IsEnabled(::features::kAccessibilityService);
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace features
