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

BASE_FEATURE(kEnableAccessibilityRestrictiveIA2AXModes,
             "AccessibilityRestrictiveIA2AXModes",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsAccessibilityRestrictiveIA2AXModesEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kEnableAccessibilityRestrictiveIA2AXModes);
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

BASE_FEATURE(kUiaProvider, "UiaProvider", base::FEATURE_DISABLED_BY_DEFAULT);

bool IsUiaProviderEnabled() {
  return base::FeatureList::IsEnabled(kUiaProvider);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsDictationOfflineAvailable() {
  return base::FeatureList::IsEnabled(
      ash::features::kOnDeviceSpeechRecognition);
}

BASE_FEATURE(kExperimentalAccessibilityChromeVoxOobeDialogImprovements,
             "ExperimentalAccessibilityChromeVoxOobeDialogImprovements",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsExperimentalAccessibilityChromeVoxOobeDialogImprovementsEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kExperimentalAccessibilityChromeVoxOobeDialogImprovements);
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
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsExperimentalAccessibilityGoogleTtsLanguagePacksEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kExperimentalAccessibilityGoogleTtsLanguagePacks);
}

BASE_FEATURE(kExperimentalAccessibilityColorEnhancementSettings,
             "ExperimentalAccessibilityColorEnhancementSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool AreExperimentalAccessibilityColorEnhancementSettingsEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kExperimentalAccessibilityColorEnhancementSettings);
}

BASE_FEATURE(kAccessibilityChromeVoxPageMigration,
             "AccessibilityChromeVoxPageMigration",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsAccessibilityChromeVoxPageMigrationEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityChromeVoxPageMigration);
}

BASE_FEATURE(kAccessibilityDictationKeyboardImprovements,
             "AccessibilityDictationKeyboardImprovements",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAccessibilityDictationKeyboardImprovementsEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityDictationKeyboardImprovements);
}

BASE_FEATURE(kAccessibilitySelectToSpeakPrefsMigration,
             "AccessibilitySelectToSpeakPrefsMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAccessibilitySelectToSpeakPrefsMigrationEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilitySelectToSpeakPrefsMigration);
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

BASE_FEATURE(kAccessibilityGameFaceIntegration,
             "AccessibilityGameFaceIntegration",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAccessibilityGameFaceIntegrationEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityGameFaceIntegration);
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
BASE_FEATURE(kAccessibilityPerformanceFiltering,
             "AccessibilityPerformanceFiltering",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAccessibilityPerformanceFilteringEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityPerformanceFiltering);
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

// This feature can be used as an emergency kill switch to disable Screen AI
// main content extraction service in case of security or other issues.
// Please talk to components/services/screen_ai/OWNERS if any changes to this
// feature or its functionality is needed.
BASE_FEATURE(kEmergencyDisableScreenAIMainContentExtraction,
             "EmergencyDisableScreenAIMainContentExtraction",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsReadAnythingWithScreen2xEnabled() {
  return base::FeatureList::IsEnabled(::features::kReadAnythingWithScreen2x) &&
         !base::FeatureList::IsEnabled(
             ::features::kEmergencyDisableScreenAIMainContentExtraction);
}

// This feature is only for debug purposes and for security/privacy reasons,
// should be never enabled by default .
BASE_FEATURE(kScreenAIDebugMode,
             "ScreenAIDebugMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsScreenAIDebugModeEnabled() {
  return base::FeatureList::IsEnabled(::features::kScreenAIDebugMode);
}

// This feature can be used as an emergency kill switch to disable Screen AI
// OCR service in case of security or other issues.
// Please talk to components/services/screen_ai/OWNERS if any changes to this
// feature or its functionality is needed.
BASE_FEATURE(kEmergencyDisableScreenAIOCR,
             "EmergencyDisableScreenAIOCR",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReadAnythingWebUIToolbar,
             "ReadAnythingWebUIToolbar",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsReadAnythingWebUIToolbarEnabled() {
  return base::FeatureList::IsEnabled(::features::kReadAnythingWebUIToolbar);
}

BASE_FEATURE(kPdfOcr, "PdfOcr", base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPdfOcrEnabled() {
  return base::FeatureList::IsEnabled(::features::kPdfOcr) &&
         !base::FeatureList::IsEnabled(
             ::features::kEmergencyDisableScreenAIOCR);
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
