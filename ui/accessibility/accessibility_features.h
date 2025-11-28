// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define all the base::Features used by ui/accessibility.
#ifndef UI_ACCESSIBILITY_ACCESSIBILITY_FEATURES_H_
#define UI_ACCESSIBILITY_ACCESSIBILITY_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_base_export.h"

// This file declares base::Features related to the ui/accessibility code.
//
// If your flag is for all platforms, include it in the first section. If
// the flag is build-specific, include it in the appropriate section or
// add a new section if needed.
//
// Keep all sections ordered alphabetically.
//
// Include the base declaration, and a bool "is...Enabled()" getter for
// convenience and consistency. The bool method does not need a comment. Place a
// comment above the feature flag describing what the flag does when enabled.
// For example, a new entry should look like:
//
//    // <<effect of the experiment>>
//    AX_BASE_EXPORT BASE_DECLARE_FEATURE(kNewFeature);
//    AX_BASE_EXPORT bool IsNewFeatureEnabled();
//
// In the .cc file, a corresponding new entry should look like:
//
//    BASE_FEATURE(kNewFeature, "NewFeature",
//    base::FEATURE_DISABLED_BY_DEFAULT); bool IsNewFeatureEnabled() {
//      return base::FeatureList::IsEnabled(::features::kNewFeature);
//    }
//
// Your feature name should start with "kAccessibility". There is no need to
// include the words "enabled" or "experimental", as these are implied. We
// include accessibility to differentiate these features from others in
// Chromium.

namespace features {

// Enable PDF OCR for Select-to-Speak. It will be disabled by default on
// platforms other than ChromeOS as STS is available only on ChromeOS.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityPdfOcrForSelectToSpeak);
AX_BASE_EXPORT bool IsAccessibilityPdfOcrForSelectToSpeakEnabled();

// A replacement algorithm for AbstractInlineTextBox + InlineCursor for
// AXInlineTextBox creation.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityBlockFlowIterator);
AX_BASE_EXPORT bool IsAccessibilityBlockFlowIteratorEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kAccessibilityPruneRedundantInlineConnectivity);
AX_BASE_EXPORT bool IsAccessibilityPruneRedundantInlineConnectivityEnabled();

// Enables the addition of text formatting information to the Android
// AccessibilityNodeInfo accessibility tree.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityTextFormatting);
AX_BASE_EXPORT bool IsAccessibilityTextFormattingEnabled();

// Enables the addition of `labeledby` relationships in the accessibility tree.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityLabeledBy);
AX_BASE_EXPORT bool IsAccessibilityLabeledByEnabled();

// Expose the accessibility tree for views via an AXTree of AXNodes.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityTreeForViews);
AX_BASE_EXPORT bool IsAccessibilityTreeForViewsEnabled();

// Serialize Views' accessibility data as soon as it changes.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kViewsAccessibilitySerializeOnDataChange);
AX_BASE_EXPORT bool IsViewsAccessibilitySerializeOnDataChangeEnabled();

// Experiment to measure the performance impact of various accessibility
// changes.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kAccessibilityPerformanceMeasurementExperiment);
AX_BASE_EXPORT bool IsAccessibilityPerformanceMeasurementExperimentEnabled();

// Use AXBitset to save boolean attributes in ui/accessibility instead of a
// vector.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityUseAXBitset);
AX_BASE_EXPORT bool IsAccessibilityUseAXBitsetEnabled();

enum class AccessibilityPerformanceMeasurementExperimentGroup {
  kAXModeComplete,
  kWebContentsOnly,
  kAXModeCompleteNoInlineTextBoxes,
  kRendererSerializationOnly,
};

AX_BASE_EXPORT AccessibilityPerformanceMeasurementExperimentGroup
GetAccessibilityPerformanceMeasurementExperimentGroup();

// Use Alternative mechanism for acquiring image descriptions.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kImageDescriptionsAlternateRouting);
AX_BASE_EXPORT bool IsImageDescriptionsAlternateRoutingEnabled();

// Disable the accessibility engine after a certain
// number of user input events spanning a minimum amount of time with no
// accessibility API usage in that time.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAutoDisableAccessibility);
AX_BASE_EXPORT bool IsAutoDisableAccessibilityEnabled();

// Recognize "aria-virtualcontent" as a valid aria property.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kEnableAccessibilityAriaVirtualContent);
AX_BASE_EXPORT bool IsAccessibilityAriaVirtualContentEnabled();

// Expose <summary>" as a heading instead of a button.
// Two reasons to try this:
// 1. Unlike for a button, JAWS will not enforce leafiness for a heading, so
// that things like child links will still be presented to the user.
// 2. The user can use heading navigation for summaries.
// We may decide to scale this back for use cases such as a summary inside of
// a table or a list.
// Experiment until we validate the approach with ATs and ARIA WG.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityExposeSummaryAsHeading);
AX_BASE_EXPORT bool IsAccessibilityExposeSummaryAsHeadingEnabled();

// Use language detection to determine the language
// of text content in page and exposed to the browser process AXTree.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kEnableAccessibilityLanguageDetection);
AX_BASE_EXPORT bool IsAccessibilityLanguageDetectionEnabled();

// Extension manifest v3 migration for network speech synthesis.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kExtensionManifestV3NetworkSpeechSynthesis);
AX_BASE_EXPORT bool IsExtensionManifestV3NetworkSpeechSynthesisEnabled();


// Turn on browser vocalization of 'descriptions' tracks.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kTextBasedAudioDescription);
AX_BASE_EXPORT bool IsTextBasedAudioDescriptionEnabled();

// Expose document markers on inline text boxes in addition to
// static nodes. (Note: This will make it possible for AXPosition in the browser
// process to handle document markers, which will be platform agnositc)
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kUseAXPositionForDocumentMarkers);
AX_BASE_EXPORT bool IsUseAXPositionForDocumentMarkersEnabled();

// Randomly turn the accessibility engine on based on certain conditions. We do
// not put this flag in chrome://flags so we can get the cleanest data possible.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAXRandomizedStressTests);
AX_BASE_EXPORT bool IsAXRandomizedStressTestsEnabled();

// When enabled, allows the content of <address> tags to be used in
// calculating their ancestors' accessible names.
// TODO(crbug.com/443765360): Remove killswitch after stability period.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAXObjectSupportsNameFromAddressContent);
AX_BASE_EXPORT bool IsAXObjectSupportsNameFromAddressContentEnabled();

// Enable the experimental on-screen AXMode .
// TODO(accessibility): Only turn on the experimental On-Screen mode for when
// screen readers are not running. This is an experimental mode for now, so this
// is fine for now.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityOnScreenMode);

// Returns true if the on screen AXMode is enabled.
AX_BASE_EXPORT bool IsAccessibilityOnScreenAXModeEnabled();

#if BUILDFLAG(IS_WIN)
// This is a killswitch. Controls whether
// HWNDMessageHandler::GetParentOfAXFragmentRoot returns nullptr (legacy) or
// delegates to GetParentNativeViewAccessible().
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityWinAXFragmentRootParent);
AX_BASE_EXPORT bool IsAccessibilityWinAXFragmentRootParentEnabled();

// When enabled, modify the exposed UIA accessibility tree to match Narrator's
// expectations. This fixes a bug keeping Narrator's cursor contained within
// the web content.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kFixNarratorWebContentContainment);
AX_BASE_EXPORT bool IsFixNarratorWebContentContainmentEnabled();

// Use Chrome-specific accessibility COM API.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kIChromeAccessible);
AX_BASE_EXPORT bool IsIChromeAccessibleEnabled();

// Enables calls to UiaDisconnectProvider when destroying a AXFragmentRootWin's
// HWND.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kUiaDisconnectRootProviders);

// Use the browser's UIA provider when requested by
// an accessibility client.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kUiaProvider);

// Optimizes event firing by only emitting events when at least one listener is
// subscribed. Killswitch to turn it off in case this work has negative
// side-effects on assistive technologies.
// TODO(https://crbug.com/402375302): Remove in M139.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kUiaEventOptimization);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
// TODO(accessibility): Should this be moved to ash_features.cc?
AX_BASE_EXPORT bool IsDictationOfflineAvailable();

// Adds option to enable Accessibility accelerator.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityAccelerator);
AX_BASE_EXPORT bool IsAccessibilityAcceleratorEnabled();

// Adds option to limit the movement on the screen.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityReducedAnimations);
AX_BASE_EXPORT bool IsAccessibilityReducedAnimationsEnabled();

// Adds reduced animations toggle to kiosk quick settings.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityReducedAnimationsInKiosk);
AX_BASE_EXPORT bool IsAccessibilityReducedAnimationsInKioskEnabled();

// Allow context checking with the accessibility Dictation
// feature.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kExperimentalAccessibilityDictationContextChecking);
AX_BASE_EXPORT bool
IsExperimentalAccessibilityDictationContextCheckingEnabled();

// Download Google TTS High Quality voices.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kExperimentalAccessibilityGoogleTtsHighQualityVoices);
AX_BASE_EXPORT bool
IsExperimentalAccessibilityGoogleTtsHighQualityVoicesEnabled();

// Whether the screen magnifier can follow the ChromeVox focus.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityMagnifierFollowsChromeVox);
AX_BASE_EXPORT bool IsAccessibilityMagnifierFollowsChromeVoxEnabled();

// Control mouse with keyboard.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityMouseKeys);
AX_BASE_EXPORT bool IsAccessibilityMouseKeysEnabled();

// Show captions on a braille display.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityCaptionsOnBrailleDisplay);
AX_BASE_EXPORT bool IsAccessibilityCaptionsOnBrailleDisplayEnabled();

// Controls whether the shake cursor to locate feature is available.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityShakeToLocate);
AX_BASE_EXPORT bool IsAccessibilityShakeToLocateEnabled();

// Controls whether the disable touchpad feature is enabled.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityDisableTouchpad);
AX_BASE_EXPORT bool IsAccessibilityDisableTouchpadEnabled();

// Controls whether the flash screen for notifications feature is available.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityFlashScreenFeature);
AX_BASE_EXPORT bool IsAccessibilityFlashScreenFeatureEnabled();

// Controls whether the bounce keys feature is available.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityBounceKeys);
AX_BASE_EXPORT bool IsAccessibilityBounceKeysEnabled();

// Controls whether the slow keys feature is available.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilitySlowKeys);
AX_BASE_EXPORT bool IsAccessibilitySlowKeysEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kAccessibilityManifestV3AccessibilityCommon);
AX_BASE_EXPORT bool IsAccessibilityManifestV3EnabledForAccessibilityCommon();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityManifestV3BrailleIme);
AX_BASE_EXPORT bool IsAccessibilityManifestV3EnabledForBrailleIme();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityManifestV3ChromeVox);
AX_BASE_EXPORT bool IsAccessibilityManifestV3EnabledForChromeVox();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityManifestV3EnhancedNetworkTts);
AX_BASE_EXPORT bool IsAccessibilityManifestV3EnabledForEnhancedNetworkTts();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityManifestV3EspeakNGTts);
AX_BASE_EXPORT bool IsAccessibilityManifestV3EnabledForEspeakNGTts();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityManifestV3GoogleTts);
AX_BASE_EXPORT bool IsAccessibilityManifestV3EnabledForGoogleTts();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityManifestV3SelectToSpeak);
AX_BASE_EXPORT bool IsAccessibilityManifestV3EnabledForSelectToSpeak();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityManifestV3SwitchAccess);
AX_BASE_EXPORT bool IsAccessibilityManifestV3EnabledForSwitchAccess();

#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)

// When populating the AccessibilityNodeInfo on Android, Clank will insert Line
// Separator U+2028 characters in the text to denote soft line breaks.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityInlineLineSeparators);
AX_BASE_EXPORT bool IsAccessibilityInlineLineSeparatorsEnabled();

// Propagate bounding rectangles of cursor moves and input focus changes to the
// Android platform to allow Magnification to follow them. For compatibility
// with older behaviour, Android SDK levels before Baklava 36.1 will only be
// notified on cursor moves.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityMagnificationFollowsFocus);
AX_BASE_EXPORT bool IsAccessibilityMagnificationFollowsFocusEnabled();

#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
// Use the AXTree fixing code, which may be an assortment of different
// tools/methods to fix the AXTree. This is not available on Android.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAXTreeFixing);
AX_BASE_EXPORT bool IsAXTreeFixingEnabled();

// Open Read Anything side panel when the browser is opened, and
// call distill after the navigation's load-complete event. (Note: The browser
// is only being opened to render one webpage, for the sake of generating
// training data for Screen2x data collection. The browser is intended to be
// closed by the user who launches Chrome once the first distill call finishes
// executing.)
//
// Note: This feature should be used along with 'ScreenAIDebugModeEnabled=true'
// and --no-sandbox.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kDataCollectionModeForScreen2x);
AX_BASE_EXPORT bool IsDataCollectionModeForScreen2xEnabled();

// Enable Immersive Mode for Read Anything.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kImmersiveReadAnything);
AX_BASE_EXPORT bool IsImmersiveReadAnythingEnabled();

// Identify and annotate the main node of the AXTree where one was not already
// provided.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kMainNodeAnnotations);
AX_BASE_EXPORT bool IsMainNodeAnnotationsEnabled();

enum class ReadAnythingMenuShuffleExperimentGroup {
  kDefault,              // Leaves in default position
  kPlaceWithSeparation,  // Adds a UI separator from previous element.
  kPlaceAtBottom         // Places at bottom of context menu.
};

// Current usage of ReadAnything corresponds to fairly short sessions on
// sites that are not naturally readable sites. We want to research whether
// people are entering reading mode by accident. Given the proximity to
// the Lens feature (and similar usage) in the context menu. We want to test
// the hypothesis of whether or not people are clicking on the ReadAnything
// menu item by mistake (targeting instead Lens).
// The parameters allow us to see the effects if we separate Lens and
// ReadAnything and if we take a more extreme position of sending ReadAnything
// to the bottom.
AX_BASE_EXPORT ReadAnythingMenuShuffleExperimentGroup
GetReadAnythingMenuShuffleExperimentGroup();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kReadAnythingMenuShuffleExperiment);
AX_BASE_EXPORT bool IsReadAnythingMenuShuffleExperimentEnabled();

// Show the Read Aloud feature in Read Anything.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kReadAnythingReadAloud);
AX_BASE_EXPORT bool IsReadAnythingReadAloudEnabled();

// Enable phrase highlighting in Read Anything Read Aloud.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kReadAnythingReadAloudPhraseHighlighting);
AX_BASE_EXPORT bool IsReadAnythingReadAloudPhraseHighlightingEnabled();

// Enable TypeScript-based text segmentation in Read Anything Read Aloud.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kReadAnythingReadAloudTSTextSegmentation);
AX_BASE_EXPORT bool IsReadAnythingReadAloudTSTextSegmentationEnabled();

// Enable the omnibox entrypoint for Read Anything.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kReadAnythingOmniboxChip);
AX_BASE_EXPORT bool IsReadAnythingOmniboxChipEnabled();

// Enable images to be distilled via algorithm. Should be disabled by
// default.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kReadAnythingImagesViaAlgorithm);
AX_BASE_EXPORT bool IsReadAnythingImagesViaAlgorithmEnabled();

// Enable Reading Mode to work on Google Docs. Should be disabled by default.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kReadAnythingDocsIntegration);
AX_BASE_EXPORT bool IsReadAnythingDocsIntegrationEnabled();

// Enable "load more" button to show at the end of Reading Mode panel.
// Should be disabled by default.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kReadAnythingDocsLoadMoreButton);
AX_BASE_EXPORT bool IsReadAnythingDocsLoadMoreButtonEnabled();

// Enable ReadabilityJS as the distillation source for Reading Mode.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kReadAnythingWithReadability);
AX_BASE_EXPORT bool IsReadAnythingWithReadabilityEnabled();

// Write some ScreenAI library debug data in /tmp.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kScreenAIDebugMode);
AX_BASE_EXPORT bool IsScreenAIDebugModeEnabled();

// ScreenAI library's Main Content Extraction service is enabled.
AX_BASE_EXPORT bool IsScreenAIMainContentExtractionEnabled();

// ScreenAI library's OCR service is enabled.
AX_BASE_EXPORT bool IsScreenAIOCREnabled();

// Enables to use the Screen AI component available for testing.
// If enabled, ScreenAI library will be loaded from //third_party/screen-ai.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kScreenAITestMode);
AX_BASE_EXPORT bool IsScreenAITestModeEnabled();

#if BUILDFLAG(IS_LINUX)
// Enables advanced partition allocation checks in ScreenAI service.
// TODO(crbug.com/418199684): Remove when the bug is fixed.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kScreenAIPartitionAllocAdvancedChecksEnabled);
#endif  // BUILDFLAG(IS_LINUX)

#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kMacAccessibilityAPIMigration);
AX_BASE_EXPORT bool IsMacAccessibilityAPIMigrationEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kMacAccessibilityOptimizeChildrenChanged);
AX_BASE_EXPORT bool IsMacAccessibilityOptimizeChildrenChangedEnabled();

// Set NSAccessibilityRemoteUIElement's RemoteUIApp to YES to fix
// some accessibility bugs in PWA Mac. (Note: When enabling
// NSAccessibilityRemoteUIElement's RemoteUIApp previously, chromium would hang.
// See: https://crbug.com/1491329).
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityRemoteUIApp);
AX_BASE_EXPORT bool IsAccessibilityRemoteUIAppEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kBlockRootWindowAccessibleNameChangeEvent);
AX_BASE_EXPORT bool IsBlockRootWindowAccessibleNameChangeEventEnabled();
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Disable the wasm tts engine component to use dev version local extension
// files.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kWasmTtsEngineAutoInstallDisabled);
AX_BASE_EXPORT bool IsWasmTtsEngineAutoInstallDisabled();
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

}  // namespace features

#endif  // UI_ACCESSIBILITY_ACCESSIBILITY_FEATURES_H_
