// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/accessibility_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_features.mojom-features.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#endif

namespace features {

BASE_FEATURE(kAccessibilityPdfOcrForSelectToSpeak,
             "kAccessibilityPdfOcrForSelectToSpeak",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_CHROMEOS)
);
bool IsAccessibilityPdfOcrForSelectToSpeakEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityPdfOcrForSelectToSpeak);
}

BASE_FEATURE(kAccessibilityExposeSummaryAsHeading,
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsAccessibilityExposeSummaryAsHeadingEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityExposeSummaryAsHeading);
}

BASE_FEATURE(kAccessibilityBlockFlowIterator, base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAccessibilityBlockFlowIteratorEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityBlockFlowIterator);
}

BASE_FEATURE(kAccessibilityPruneRedundantInlineConnectivity,
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsAccessibilityPruneRedundantInlineConnectivityEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityPruneRedundantInlineConnectivity);
}

BASE_FEATURE(kAccessibilityTextFormatting, base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAccessibilityTextFormattingEnabled() {
  return base::FeatureList::IsEnabled(::features::kAccessibilityTextFormatting);
}

BASE_FEATURE(kAccessibilityLabeledBy, base::FEATURE_DISABLED_BY_DEFAULT);
bool IsAccessibilityLabeledByEnabled() {
  return base::FeatureList::IsEnabled(::features::kAccessibilityLabeledBy);
}

BASE_FEATURE(kAccessibilityTreeForViews, base::FEATURE_DISABLED_BY_DEFAULT);
bool IsAccessibilityTreeForViewsEnabled() {
  return base::FeatureList::IsEnabled(::features::kAccessibilityTreeForViews);
}

BASE_FEATURE(kAccessibilityPerformanceMeasurementExperiment,
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsAccessibilityPerformanceMeasurementExperimentEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityPerformanceMeasurementExperiment);
}

BASE_FEATURE(kAccessibilityUseAXBitset, base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAccessibilityUseAXBitsetEnabled() {
  return base::FeatureList::IsEnabled(::features::kAccessibilityUseAXBitset);
}

namespace {

constexpr base::FeatureParam<
    AccessibilityPerformanceMeasurementExperimentGroup>::Option
    kAccessibilityPerformanceMeasurementExperimentParamOptions[] = {
        {AccessibilityPerformanceMeasurementExperimentGroup ::kAXModeComplete,
         "AXModeComplete"},
        {AccessibilityPerformanceMeasurementExperimentGroup ::kWebContentsOnly,
         "WebContentsOnly"},
        {AccessibilityPerformanceMeasurementExperimentGroup ::
             kAXModeCompleteNoInlineTextBoxes,
         "AXModeCompleteNoInlineTextBoxes"},
        {AccessibilityPerformanceMeasurementExperimentGroup ::
             kRendererSerializationOnly,
         "RendererSerializationOnly"}};

BASE_FEATURE_ENUM_PARAM(
    AccessibilityPerformanceMeasurementExperimentGroup,
    kAccessibilityPerformanceMeasurementExperimentParam,
    &kAccessibilityPerformanceMeasurementExperiment,
    "accessibility_performance_group_name",
    AccessibilityPerformanceMeasurementExperimentGroup::kAXModeComplete,
    &kAccessibilityPerformanceMeasurementExperimentParamOptions);

}  // namespace

AccessibilityPerformanceMeasurementExperimentGroup
GetAccessibilityPerformanceMeasurementExperimentGroup() {
  DCHECK(IsAccessibilityPerformanceMeasurementExperimentEnabled());
  return kAccessibilityPerformanceMeasurementExperimentParam.Get();
}

BASE_FEATURE(kViewsAccessibilitySerializeOnDataChanged,
             base::FEATURE_ENABLED_BY_DEFAULT);
bool IsViewsAccessibilitySerializeOnDataChangeEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kViewsAccessibilitySerializeOnDataChanged);
}

BASE_FEATURE(kImageDescriptionsAlternateRouting,
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsImageDescriptionsAlternateRoutingEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kImageDescriptionsAlternateRouting);
}

BASE_FEATURE(kAutoDisableAccessibility, base::FEATURE_DISABLED_BY_DEFAULT);
bool IsAutoDisableAccessibilityEnabled() {
  return base::FeatureList::IsEnabled(::features::kAutoDisableAccessibility);
}

BASE_FEATURE(kEnableAccessibilityAriaVirtualContent,
             "AccessibilityAriaVirtualContent",
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsAccessibilityAriaVirtualContentEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kEnableAccessibilityAriaVirtualContent);
}

BASE_FEATURE(kEnableAccessibilityLanguageDetection,
             "AccessibilityLanguageDetection",
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsAccessibilityLanguageDetectionEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kEnableAccessibilityLanguageDetection);
}

BASE_FEATURE(kExtensionManifestV3NetworkSpeechSynthesis,
             base::FEATURE_ENABLED_BY_DEFAULT);
bool IsExtensionManifestV3NetworkSpeechSynthesisEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kExtensionManifestV3NetworkSpeechSynthesis);
}


BASE_FEATURE(kTextBasedAudioDescription, base::FEATURE_DISABLED_BY_DEFAULT);
bool IsTextBasedAudioDescriptionEnabled() {
  return base::FeatureList::IsEnabled(::features::kTextBasedAudioDescription);
}

BASE_FEATURE(kUseAXPositionForDocumentMarkers,
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsUseAXPositionForDocumentMarkersEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kUseAXPositionForDocumentMarkers);
}

BASE_FEATURE(kAXRandomizedStressTests, base::FEATURE_DISABLED_BY_DEFAULT);
bool IsAXRandomizedStressTestsEnabled() {
  return base::FeatureList::IsEnabled(::features::kAXRandomizedStressTests);
}

BASE_FEATURE(kAXObjectSupportsNameFromAddressContent,
             base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAXObjectSupportsNameFromAddressContentEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAXObjectSupportsNameFromAddressContent);
}

BASE_FEATURE(kAccessibilityOnScreenMode,
             "AccessibilityOnScreenAXMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAccessibilityOnScreenAXModeEnabled() {
  return base::FeatureList::IsEnabled(::features::kAccessibilityOnScreenMode);
}

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kAccessibilityWinAXFragmentRootParent,
             base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAccessibilityWinAXFragmentRootParentEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityWinAXFragmentRootParent);
}

BASE_FEATURE(kFixNarratorWebContentContainment,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsFixNarratorWebContentContainmentEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kFixNarratorWebContentContainment);
}

BASE_FEATURE(kIChromeAccessible, base::FEATURE_DISABLED_BY_DEFAULT);
bool IsIChromeAccessibleEnabled() {
  return base::FeatureList::IsEnabled(::features::kIChromeAccessible);
}

BASE_FEATURE(kUiaDisconnectRootProviders, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUiaProvider, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUiaEventOptimization, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
bool IsDictationOfflineAvailable() {
  return base::FeatureList::IsEnabled(
      ash::features::kOnDeviceSpeechRecognition);
}

BASE_FEATURE(kAccessibilityAccelerator, base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAccessibilityAcceleratorEnabled() {
  return base::FeatureList::IsEnabled(::features::kAccessibilityAccelerator);
}

BASE_FEATURE(kAccessibilityReducedAnimations, base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAccessibilityReducedAnimationsEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityReducedAnimations);
}

BASE_FEATURE(kAccessibilityReducedAnimationsInKiosk,
             base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAccessibilityReducedAnimationsInKioskEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityReducedAnimationsInKiosk);
}

BASE_FEATURE(kExperimentalAccessibilityGoogleTtsHighQualityVoices,
             base::FEATURE_ENABLED_BY_DEFAULT);
bool IsExperimentalAccessibilityGoogleTtsHighQualityVoicesEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kExperimentalAccessibilityGoogleTtsHighQualityVoices);
}

BASE_FEATURE(kExperimentalAccessibilityDictationContextChecking,
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsExperimentalAccessibilityDictationContextCheckingEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kExperimentalAccessibilityDictationContextChecking);
}

BASE_FEATURE(kAccessibilityMagnifierFollowsChromeVox,
             base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAccessibilityMagnifierFollowsChromeVoxEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityMagnifierFollowsChromeVox);
}

BASE_FEATURE(kAccessibilityMouseKeys, base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAccessibilityMouseKeysEnabled() {
  return base::FeatureList::IsEnabled(::features::kAccessibilityMouseKeys);
}

BASE_FEATURE(kAccessibilityCaptionsOnBrailleDisplay,
             "CaptionsOnBrailleDisplay",
             base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAccessibilityCaptionsOnBrailleDisplayEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityCaptionsOnBrailleDisplay);
}

BASE_FEATURE(kAccessibilityDisableTouchpad, base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAccessibilityDisableTouchpadEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityDisableTouchpad);
}

BASE_FEATURE(kAccessibilityFlashScreenFeature,
             base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAccessibilityFlashScreenFeatureEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityFlashScreenFeature);
}

BASE_FEATURE(kAccessibilityBounceKeys, base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAccessibilityBounceKeysEnabled() {
  return base::FeatureList::IsEnabled(::features::kAccessibilityBounceKeys);
}

BASE_FEATURE(kAccessibilitySlowKeys, base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAccessibilitySlowKeysEnabled() {
  return base::FeatureList::IsEnabled(::features::kAccessibilitySlowKeys);
}

BASE_FEATURE(kAccessibilityShakeToLocate, base::FEATURE_DISABLED_BY_DEFAULT);
bool IsAccessibilityShakeToLocateEnabled() {
  return base::FeatureList::IsEnabled(::features::kAccessibilityShakeToLocate);
}

BASE_FEATURE(kAccessibilityManifestV3BrailleIme,
             base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAccessibilityManifestV3EnabledForBrailleIme() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityManifestV3BrailleIme);
}

BASE_FEATURE(kAccessibilityManifestV3ChromeVox,
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsAccessibilityManifestV3EnabledForChromeVox() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityManifestV3ChromeVox);
}

BASE_FEATURE(kAccessibilityManifestV3EnhancedNetworkTts,
             base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAccessibilityManifestV3EnabledForEnhancedNetworkTts() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityManifestV3EnhancedNetworkTts);
}

BASE_FEATURE(kAccessibilityManifestV3EspeakNGTts,
             base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAccessibilityManifestV3EnabledForEspeakNGTts() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityManifestV3EspeakNGTts);
}

BASE_FEATURE(kAccessibilityManifestV3GoogleTts,
             base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAccessibilityManifestV3EnabledForGoogleTts() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityManifestV3GoogleTts);
}

BASE_FEATURE(kAccessibilityManifestV3AccessibilityCommon,
             base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAccessibilityManifestV3EnabledForAccessibilityCommon() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityManifestV3AccessibilityCommon);
}

BASE_FEATURE(kAccessibilityManifestV3SelectToSpeak,
             base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAccessibilityManifestV3EnabledForSelectToSpeak() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityManifestV3SelectToSpeak);
}

BASE_FEATURE(kAccessibilityManifestV3SwitchAccess,
             base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAccessibilityManifestV3EnabledForSwitchAccess() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityManifestV3SwitchAccess);
}

#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kAccessibilityInlineLineSeparators,
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsAccessibilityInlineLineSeparatorsEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityInlineLineSeparators);
}

BASE_FEATURE(kAccessibilityMagnificationFollowsFocus,
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsAccessibilityMagnificationFollowsFocusEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kAccessibilityMagnificationFollowsFocus);
}

#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kAXTreeFixing, base::FEATURE_DISABLED_BY_DEFAULT);
bool IsAXTreeFixingEnabled() {
  return base::FeatureList::IsEnabled(::features::kAXTreeFixing);
}

bool IsScreenAIMainContentExtractionEnabled() {
  return base::FeatureList::IsEnabled(
      ax::mojom::features::kScreenAIMainContentExtractionEnabled);
}

bool IsScreenAIOCREnabled() {
  return base::FeatureList::IsEnabled(ax::mojom::features::kScreenAIOCREnabled);
}

// This feature is only used for generating training data for Screen2x and
// should never be used in any other circumstance, and should not be enabled by
// default.
BASE_FEATURE(kDataCollectionModeForScreen2x, base::FEATURE_DISABLED_BY_DEFAULT);
bool IsDataCollectionModeForScreen2xEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kDataCollectionModeForScreen2x);
}

BASE_FEATURE(kImmersiveReadAnything, base::FEATURE_DISABLED_BY_DEFAULT);
bool IsImmersiveReadAnythingEnabled() {
  return base::FeatureList::IsEnabled(::features::kImmersiveReadAnything);
}

BASE_FEATURE(kMainNodeAnnotations, base::FEATURE_DISABLED_BY_DEFAULT);
bool IsMainNodeAnnotationsEnabled() {
  return base::FeatureList::IsEnabled(::features::kMainNodeAnnotations);
}

BASE_FEATURE(kReadAnythingMenuShuffleExperiment,
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsReadAnythingMenuShuffleExperimentEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kReadAnythingMenuShuffleExperiment);
}

namespace {
constexpr base::FeatureParam<ReadAnythingMenuShuffleExperimentGroup>::Option
    kReadAnythingMenuShuffleExperimentParamOptions[3] = {
        {ReadAnythingMenuShuffleExperimentGroup::kDefault,
         "MenuShuffleDefault"},
        {ReadAnythingMenuShuffleExperimentGroup::kPlaceWithSeparation,
         "MenuShuffleSeparation"},
        {ReadAnythingMenuShuffleExperimentGroup::kPlaceAtBottom,
         "MenuShufflePlaceAtBottom"}};

BASE_FEATURE_ENUM_PARAM(ReadAnythingMenuShuffleExperimentGroup,
                        kReadAnythingMenuShuffleExperimentParam,
                        &kReadAnythingMenuShuffleExperiment,
                        "read_anything_menu_shuffle_group_name",
                        ReadAnythingMenuShuffleExperimentGroup::kDefault,
                        &kReadAnythingMenuShuffleExperimentParamOptions);
}  // namespace

ReadAnythingMenuShuffleExperimentGroup
GetReadAnythingMenuShuffleExperimentGroup() {
  CHECK(IsReadAnythingMenuShuffleExperimentEnabled());
  return kReadAnythingMenuShuffleExperimentParam.Get();
}

BASE_FEATURE(kReadAnythingReadAloud, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsReadAnythingReadAloudEnabled() {
  return base::FeatureList::IsEnabled(::features::kReadAnythingReadAloud);
}

BASE_FEATURE(kReadAnythingReadAloudPhraseHighlighting,
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsReadAnythingReadAloudPhraseHighlightingEnabled() {
  return base::FeatureList::IsEnabled(::features::kReadAnythingReadAloud) &&
         base::FeatureList::IsEnabled(
             ::features::kReadAnythingReadAloudPhraseHighlighting);
}

BASE_FEATURE(kReadAnythingReadAloudTSTextSegmentation,
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsReadAnythingReadAloudTSTextSegmentationEnabled() {
  return base::FeatureList::IsEnabled(::features::kReadAnythingReadAloud) &&
         base::FeatureList::IsEnabled(
             ::features::kReadAnythingReadAloudTSTextSegmentation);
}

BASE_FEATURE(kReadAnythingOmniboxChip, base::FEATURE_DISABLED_BY_DEFAULT);
bool IsReadAnythingOmniboxChipEnabled() {
  return base::FeatureList::IsEnabled(::features::kReadAnythingOmniboxChip);
}

BASE_FEATURE(kReadAnythingImagesViaAlgorithm,
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsReadAnythingImagesViaAlgorithmEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kReadAnythingImagesViaAlgorithm);
}

BASE_FEATURE(kReadAnythingDocsIntegration, base::FEATURE_DISABLED_BY_DEFAULT);
bool IsReadAnythingDocsIntegrationEnabled() {
  return base::FeatureList::IsEnabled(
      ax::mojom::features::kReadAnythingDocsIntegration);
}

BASE_FEATURE(kReadAnythingDocsLoadMoreButton,
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsReadAnythingDocsLoadMoreButtonEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kReadAnythingDocsLoadMoreButton);
}

BASE_FEATURE(kReadAnythingWithReadability, base::FEATURE_DISABLED_BY_DEFAULT);
bool IsReadAnythingWithReadabilityEnabled() {
  return base::FeatureList::IsEnabled(
             ::features::kReadAnythingReadAloudTSTextSegmentation) &&
         base::FeatureList::IsEnabled(::features::kReadAnythingWithReadability);
  ;
}

// This feature is only for debug purposes and for security/privacy reasons,
// should be never enabled by default .
BASE_FEATURE(kScreenAIDebugMode, base::FEATURE_DISABLED_BY_DEFAULT);
bool IsScreenAIDebugModeEnabled() {
  return base::FeatureList::IsEnabled(::features::kScreenAIDebugMode);
}

// This feature is only used in tests and must not be enabled by default.
BASE_FEATURE(kScreenAITestMode, base::FEATURE_DISABLED_BY_DEFAULT);
bool IsScreenAITestModeEnabled() {
  return base::FeatureList::IsEnabled(::features::kScreenAITestMode);
}

#if BUILDFLAG(IS_LINUX)
BASE_FEATURE(kScreenAIPartitionAllocAdvancedChecksEnabled,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
// Enables the switchover to the newer NSAccessibility property-based API.
BASE_FEATURE(kMacAccessibilityAPIMigration, base::FEATURE_DISABLED_BY_DEFAULT);
bool IsMacAccessibilityAPIMigrationEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kMacAccessibilityAPIMigration);
}

BASE_FEATURE(kMacAccessibilityOptimizeChildrenChanged,
             base::FEATURE_ENABLED_BY_DEFAULT);
bool IsMacAccessibilityOptimizeChildrenChangedEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kMacAccessibilityOptimizeChildrenChanged);
}

BASE_FEATURE(kAccessibilityRemoteUIApp, base::FEATURE_ENABLED_BY_DEFAULT);
bool IsAccessibilityRemoteUIAppEnabled() {
  return base::FeatureList::IsEnabled(::features::kAccessibilityRemoteUIApp);
}

BASE_FEATURE(kBlockRootWindowAccessibleNameChangeEvent,
             base::FEATURE_ENABLED_BY_DEFAULT);
bool IsBlockRootWindowAccessibleNameChangeEventEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kBlockRootWindowAccessibleNameChangeEvent);
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kWasmTtsEngineAutoInstallDisabled,
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsWasmTtsEngineAutoInstallDisabled() {
  return base::FeatureList::IsEnabled(
      ::features::kWasmTtsEngineAutoInstallDisabled);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

}  // namespace features
