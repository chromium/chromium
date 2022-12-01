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

namespace features {

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kEnableAccessibilityAriaVirtualContent);

// Returns true if "aria-virtualcontent" should be recognized as a valid aria
// property.
AX_BASE_EXPORT bool IsAccessibilityAriaVirtualContentEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kEnableAccessibilityExposeHTMLElement);

// Returns true if the <html> element should be exposed to the
// browser process AXTree (as an ignored node).
AX_BASE_EXPORT bool IsAccessibilityExposeHTMLElementEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kEnableAccessibilityExposeIgnoredNodes);

// Returns true if all ignored nodes are exposed by Blink in the
// accessibility tree.
AX_BASE_EXPORT bool IsAccessibilityExposeIgnoredNodesEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kEnableAccessibilityLanguageDetection);

// Return true if language detection should be used to determine the language
// of text content in page and exposed to the browser process AXTree.
AX_BASE_EXPORT bool IsAccessibilityLanguageDetectionEnabled();

// Serializes accessibility information from the Views tree and deserializes it
// into an AXTree in the browser process.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kEnableAccessibilityTreeForViews);

// Returns true if the Views tree is exposed using an AXTree in the browser
// process. Returns false if the Views tree is exposed to accessibility
// directly.
AX_BASE_EXPORT bool IsAccessibilityTreeForViewsEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityFocusHighlight);

// Returns true if the accessibility focus highlight feature is enabled,
// which draws a visual highlight around the focused element on the page
// briefly whenever focus changes.
AX_BASE_EXPORT bool IsAccessibilityFocusHighlightEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAutoDisableAccessibility);

// Returns true if accessibility will be auto-disabled after a certain
// number of user input events spanning a minimum amount of time with no
// accessibility API usage in that time.
AX_BASE_EXPORT bool IsAutoDisableAccessibilityEnabled();

// Enables a setting that can turn on/off browser vocalization of 'descriptions'
// tracks.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kTextBasedAudioDescription);

// Returns true if the setting to turn on text based audio descriptions is
// enabled.
AX_BASE_EXPORT bool IsTextBasedAudioDescriptionEnabled();

#if BUILDFLAG(IS_WIN)
// Enables an experimental Chrome-specific accessibility COM API
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kIChromeAccessible);

// Returns true if the IChromeAccessible COM API is enabled.
AX_BASE_EXPORT bool IsIChromeAccessibleEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kSelectiveUIAEnablement);

// Returns true if accessibility will be selectively enabled depending on the
// UIA APIs that are called, allowing non-screenreader usage to enable less of
// the accessibility system.
AX_BASE_EXPORT bool IsSelectiveUIAEnablementEnabled();

#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables ability to resize Docked Magnifier.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kDockedMagnifierResizing);

// Returns true if the feature which adds ability for user to grab and resize
// bottom of Docked Magnifier is enabled.
AX_BASE_EXPORT bool IsDockedMagnifierResizingEnabled();

AX_BASE_EXPORT bool IsDictationOfflineAvailable();

// Enables accessibility Dictation with the pumpkin semantic parser.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kExperimentalAccessibilityDictationWithPumpkin);

// Returns true if Dictation with context checking is enabled.
AX_BASE_EXPORT bool
IsExperimentalAccessibilityDictationContextCheckingEnabled();

// Enables Context Checking with the accessibility Dictation feature.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kExperimentalAccessibilityDictationContextChecking);

// Returns true if dictation with pumpkin is enabled.
AX_BASE_EXPORT bool IsExperimentalAccessibilityDictationWithPumpkinEnabled();

// Enables more commands with the accessibility Dictation feature.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kExperimentalAccessibilityDictationMoreCommands);

// Returns true if Dictation with more commands is enabled.
AX_BASE_EXPORT bool IsExperimentalAccessibilityDictationMoreCommandsEnabled();

// Enables downloading Google TTS voices using Language Packs.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kExperimentalAccessibilityGoogleTtsLanguagePacks);

// Returns true if using Language Packs to download Google TTS voices is
// enabled.
AX_BASE_EXPORT bool IsExperimentalAccessibilityGoogleTtsLanguagePacksEnabled();

// Enables Select-to-Speak voice switching.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kExperimentalAccessibilitySelectToSpeakVoiceSwitching);

// Returns true if the Select-to-Speak voice switching feature is enabled.
AX_BASE_EXPORT bool
IsExperimentalAccessibilitySelectToSpeakVoiceSwitchingEnabled();

// Enables the experimental color enhancements settings.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kExperimentalAccessibilityColorEnhancementSettings);

// Returns true if the experimental color enhancements settings are enabled.
AX_BASE_EXPORT bool
AreExperimentalAccessibilityColorEnhancementSettingsEnabled();

// Enables Select-to-Speak settings page migration from extension options page
// to Chrome OS settings page.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilitySelectToSpeakPageMigration);

// Returns true if Select-to-Speak settings page migration enabled.
AX_BASE_EXPORT bool IsAccessibilitySelectToSpeakPageMigrationEnabled();

// Enables AccessibilitySelectToSpeakPrefsMigration.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilitySelectToSpeakPrefsMigration);

// Returns true if AccessibilitySelectToSpeakPrefsMigration enabled.
AX_BASE_EXPORT bool IsAccessibilitySelectToSpeakPrefsMigrationEnabled();

// Enables AccessibilitySelectToSpeakContextMenuOption.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kAccessibilitySelectToSpeakContextMenuOption);

// Returns true if AccessibilitySelectToSpeakContextMenuOption is enabled.
AX_BASE_EXPORT bool IsAccessibilitySelectToSpeakContextMenuOptionEnabled();

// Enables AccessibilitySelectToSpeakHoverTextImprovements.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kAccessibilitySelectToSpeakHoverTextImprovements);

// Returns true if AccessibilitySelectToSpeakHoverTextImprovements is enabled.
AX_BASE_EXPORT bool IsAccessibilitySelectToSpeakHoverTextImprovementsEnabled();

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Enables Get Image Descriptions to augment existing images labels,
// rather than only provide descriptions for completely unlabeled images.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAugmentExistingImageLabels);

// Returns true if augmenting existing image labels is enabled.
AX_BASE_EXPORT bool IsAugmentExistingImageLabelsEnabled();

// Once this flag is enabled, a single codebase in AXPosition will be used for
// handling document markers on all platforms, including the announcement of
// spelling mistakes.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kUseAXPositionForDocumentMarkers);

// Returns true if document markers are exposed on inline text boxes in the
// accessibility tree in addition to on static text nodes. This in turn enables
// AXPosition on the browser to discover and work with document markers, instead
// of the legacy code that collects document markers manually from static text
// nodes and which is different for each platform.
AX_BASE_EXPORT bool IsUseAXPositionForDocumentMarkersEnabled();

// Enable support for ARIA element reflection, for example
// element.ariaActiveDescendantElement = child;
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kEnableAriaElementReflection);

// Returns true if ARIA element reflection is enabled.
AX_BASE_EXPORT bool IsAriaElementReflectionEnabled();

// Experiment to increase the cost of SendPendingAccessibilityEvents.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAblateSendPendingAccessibilityEvents);

// Returns true if |kAblateSendPendingAccessibilityEvents| is enabled.
AX_BASE_EXPORT bool IsAblateSendPendingAccessibilityEventsEnabled();

#if BUILDFLAG(IS_ANDROID)
// Compute the AXMode based on AccessibilityServiceInfo. If disabled,
// the AXMode is either entirely on or entirely off.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kComputeAXMode);

// Returns true if the IChromeAccessible COM API is enabled.
AX_BASE_EXPORT bool IsComputeAXModeEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kOptimizeAccessibilityUiThreadWork);

bool IsOptimizeAccessibilityUiThreadWorkEnabled();

#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kReadAnything);

// Returns true if read anything is enabled. This feature shows users websites,
// such as articles, in a comfortable reading experience in a side panel.
AX_BASE_EXPORT bool IsReadAnythingEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kReadAnythingWithScreen2x);

// Returns true if read anything is enabled with screen2x integration, which
// distills web pages using an ML model.
AX_BASE_EXPORT bool IsReadAnythingWithScreen2xEnabled();

// Returns true if Screen AI Service is needed as either
// ScreenAIVisualAnnotations or ReadAnythingWithScreen2x are enabled.
AX_BASE_EXPORT bool IsScreenAIServiceNeeded();

// If enabled, ScreenAI library writes some debug data in /tmp.
AX_BASE_EXPORT bool IsScreenAIDebugModeEnabled();

// Enables a feature whereby inaccessible (i.e. untagged) PDFs are made
// accessible using an optical character recognition service. Due to the size of
// the OCR component, this feature targets desktop versions of Chrome for now.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kPdfOcr);

// Returns true if OCR will be performed on inaccessible (i.e. untagged) PDFs
// and the resulting text, together with its layout information, will be added
// to the accessibility tree.
AX_BASE_EXPORT bool IsPdfOcrEnabled();

// Enables a feature whereby inaccessible surfaces such as canvases are made
// accessible using a local machine intelligence service.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kLayoutExtraction);

// Returns true if Layout Extraction feature is enabled. This feature uses a
// local machine intelligence library to process screenshots and adds metadata
// to the accessibility tree.
AX_BASE_EXPORT bool IsLayoutExtractionEnabled();

// Enables the experimental Accessibility Service.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityService);

// Returns true if the Accessibility Service enabled.
AX_BASE_EXPORT bool IsAccessibilityServiceEnabled();

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace features

#endif  // UI_ACCESSIBILITY_ACCESSIBILITY_FEATURES_H_
