// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define all the base::Features used by ui/accessibility.
#ifndef UI_ACCESSIBILITY_ACCESSIBILITY_FEATURES_H_
#define UI_ACCESSIBILITY_ACCESSIBILITY_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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

// Increase the cost of SendPendingAccessibilityEvents.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAblateSendPendingAccessibilityEvents);
AX_BASE_EXPORT bool IsAblateSendPendingAccessibilityEventsEnabled();

// Draw a visual highlight around the focused element on the page
// briefly whenever focus changes.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityFocusHighlight);
AX_BASE_EXPORT bool IsAccessibilityFocusHighlightEnabled();

// Enable PDF OCR for Select-to-Speak. It will be disabled by default on
// platforms other than ChromeOS as STS is available only on ChromeOS.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityPdfOcrForSelectToSpeak);
AX_BASE_EXPORT bool IsAccessibilityPdfOcrForSelectToSpeakEnabled();

// Augment existing images labels in addition to unlabeled images.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAugmentExistingImageLabels);
AX_BASE_EXPORT bool IsAugmentExistingImageLabelsEnabled();

// Disable the accessibility engine after a certain
// number of user input events spanning a minimum amount of time with no
// accessibility API usage in that time.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAutoDisableAccessibility);
AX_BASE_EXPORT bool IsAutoDisableAccessibilityEnabled();

// Make PDFs displayed in the ChromeOS Media App (AKA Backlight)
// accessible by performing OCR on the images for each page.
// TODO(nektar): Should this be moved to ChromeOS section?
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kBacklightOcr);
AX_BASE_EXPORT bool IsBacklightOcrEnabled();

// Recognize "aria-virtualcontent" as a valid aria property.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kEnableAccessibilityAriaVirtualContent);
AX_BASE_EXPORT bool IsAccessibilityAriaVirtualContentEnabled();

// Expose the <html> element to the browser process AXTree (as an
// ignored node).
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kEnableAccessibilityExposeHTMLElement);
AX_BASE_EXPORT bool IsAccessibilityExposeHTMLElementEnabled();

// Expose all ignored nodes by Blink in the accessibility tree.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kEnableAccessibilityExposeIgnoredNodes);
AX_BASE_EXPORT bool IsAccessibilityExposeIgnoredNodesEnabled();

// Use language detection to determine the language
// of text content in page and exposed to the browser process AXTree.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kEnableAccessibilityLanguageDetection);
AX_BASE_EXPORT bool IsAccessibilityLanguageDetectionEnabled();

// Restrict AXModes to web content related modes only when an IA2
// query is performed on a web content node.
// TODO(1441211): Remove flag once the change has been confirmed safe.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kEnableAccessibilityRestrictiveIA2AXModes);
AX_BASE_EXPORT bool IsAccessibilityRestrictiveIA2AXModesEnabled();

// Serialize accessibility information from the Views tree and
// deserialize it into an AXTree in the browser process.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kEnableAccessibilityTreeForViews);
AX_BASE_EXPORT bool IsAccessibilityTreeForViewsEnabled();

// Support aria element reflection. For example:
//     element.ariaActiveDescendantElement = child;
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kEnableAriaElementReflection);
AX_BASE_EXPORT bool IsAriaElementReflectionEnabled();

// Turn on browser vocalization of 'descriptions' tracks.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kTextBasedAudioDescription);
AX_BASE_EXPORT bool IsTextBasedAudioDescriptionEnabled();

// Expose document markers on inline text boxes in addition to
// static nodes. (Note: This will make it possible for AXPosition in the browser
// process to handle document markers, which will be platform agnositc)
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kUseAXPositionForDocumentMarkers);
AX_BASE_EXPORT bool IsUseAXPositionForDocumentMarkersEnabled();

#if BUILDFLAG(IS_WIN)
// Use Chrome-specific accessibility COM API.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kIChromeAccessible);
AX_BASE_EXPORT bool IsIChromeAccessibleEnabled();

// Selectively enable accessibility depending on the
// UIA APIs that are called. (Note: This will make it possible for
// non-screenreader services to enable less of the accessibility system)
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kSelectiveUIAEnablement);
AX_BASE_EXPORT bool IsSelectiveUIAEnablementEnabled();

// Use the browser's UIA provider when requested by
// an accessibility client.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kUiaProvider);
AX_BASE_EXPORT bool IsUiaProviderEnabled();
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// TODO(accessibility): Should this be moved to ash_features.cc?
AX_BASE_EXPORT bool IsDictationOfflineAvailable();

// Allow accessibility accelerator notifications to timeout.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kAccessibilityAcceleratorNotificationsTimeout);
AX_BASE_EXPORT bool IsAccessibilityAcceleratorNotificationsTimeoutEnabled();

// Use Dictation keyboard improvements.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kAccessibilityDictationKeyboardImprovements);
AX_BASE_EXPORT bool IsAccessibilityDictationKeyboardImprovementsEnabled();

// Integrate with FaceGaze.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityFaceGaze);
AX_BASE_EXPORT bool IsAccessibilityFaceGazeEnabled();

// Use Select-to-Speak hover text improvements.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kAccessibilitySelectToSpeakHoverTextImprovements);
AX_BASE_EXPORT bool IsAccessibilitySelectToSpeakHoverTextImprovementsEnabled();

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

// Use Language Packs to download Google TTS voices.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kExperimentalAccessibilityGoogleTtsLanguagePacks);
AX_BASE_EXPORT bool IsExperimentalAccessibilityGoogleTtsLanguagePacksEnabled();

// Whether the extra-large cursor size feature is enabled.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityExtraLargeCursor);
AX_BASE_EXPORT bool IsAccessibilityExtraLargeCursorEnabled();

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID)
// Filter AXModes based on running accessibility services.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityPerformanceFiltering);
AX_BASE_EXPORT bool IsAccessibilityPerformanceFilteringEnabled();

// Disable max node and timeout limits on the
// AXTreeSnapshotter's Snapshot method, and track related histograms.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilitySnapshotStressTests);
AX_BASE_EXPORT bool IsAccessibilitySnapshotStressTestsEnabled();
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
// Use the experimental Accessibility Service.
// TODO(katydek): Provide a more descriptive name here.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityService);
AX_BASE_EXPORT bool IsAccessibilityServiceEnabled();

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

// Use local MI service to make inaccessibile surfaces (e.g.
// canvases) accessible.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kLayoutExtraction);
AX_BASE_EXPORT bool IsLayoutExtractionEnabled();

// Use OCR to make inaccessible (i.e. untagged) PDFs
// accessibility. (Note: Due to the size of the OCR component, this feature
// targets only desktop versions of Chrome for now.)
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kPdfOcr);
AX_BASE_EXPORT bool IsPdfOcrEnabled();

// Include the Read Anything feature. (Note: This feature shows
// users websites, such as articles, in a comfortable reading experience in a
// side panel)
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kReadAnything);
AX_BASE_EXPORT bool IsReadAnythingEnabled();

// Make the Read Anything Side Panel local (don't persist when opening a new
// tab)
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kReadAnythingLocalSidePanel);
AX_BASE_EXPORT bool IsReadAnythingLocalSidePanelEnabled();

// Show a reading mode icon in the omnibox.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kReadAnythingOmniboxIcon);
AX_BASE_EXPORT bool IsReadAnythingOmniboxIconEnabled();

// Show the Read Aloud feature in Read Anything.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kReadAnythingReadAloud);
AX_BASE_EXPORT bool IsReadAnythingReadAloudEnabled();

// Use the WebUI toolbar in Read Anything.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kReadAnythingWebUIToolbar);
AX_BASE_EXPORT bool IsReadAnythingWebUIToolbarEnabled();

// Use screen2x integration for Read Anything to distill web pages
// using an ML model.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kReadAnythingWithScreen2x);
AX_BASE_EXPORT bool IsReadAnythingWithScreen2xEnabled();

// Enable rules based algorithm for distilling content. Should be enabled by
// default.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kReadAnythingWithAlgorithm);
AX_BASE_EXPORT bool IsReadAnythingWithAlgorithmEnabled();

// Write some ScreenAI library debug data in /tmp.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kScreenAIDebugMode);
AX_BASE_EXPORT bool IsScreenAIDebugModeEnabled();
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
// Set NSAccessibilityRemoteUIElement's RemoteUIApp to YES to fix
// some accessibility bugs in PWA Mac. (Note: When enabling
// NSAccessibilityRemoteUIElement's RemoteUIApp previously, chromium would hang.
// See: https://crbug.com/1491329).
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityRemoteUIApp);
AX_BASE_EXPORT bool IsAccessibilityRemoteUIAppEnabled();
#endif  // BUILDFLAG(IS_MAC)

}  // namespace features

#endif  // UI_ACCESSIBILITY_ACCESSIBILITY_FEATURES_H_
