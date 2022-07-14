// Copyright (c) 2019 The Chromium Authors. All rights reserved.
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

AX_BASE_EXPORT extern const base::Feature
    kEnableAccessibilityAriaVirtualContent;

// Returns true if "aria-virtualcontent" should be recognized as a valid aria
// property.
AX_BASE_EXPORT bool IsAccessibilityAriaVirtualContentEnabled();

AX_BASE_EXPORT extern const base::Feature kEnableAccessibilityExposeHTMLElement;

// Returns true if the <html> element should be exposed to the
// browser process AXTree (as an ignored node).
AX_BASE_EXPORT bool IsAccessibilityExposeHTMLElementEnabled();

AX_BASE_EXPORT extern const base::Feature
    kEnableAccessibilityExposeIgnoredNodes;

// Returns true if all ignored nodes are exposed by Blink in the
// accessibility tree.
AX_BASE_EXPORT bool IsAccessibilityExposeIgnoredNodesEnabled();

AX_BASE_EXPORT extern const base::Feature kEnableAccessibilityLanguageDetection;

// Return true if language detection should be used to determine the language
// of text content in page and exposed to the browser process AXTree.
AX_BASE_EXPORT bool IsAccessibilityLanguageDetectionEnabled();

// Serializes accessibility information from the Views tree and deserializes it
// into an AXTree in the browser process.
AX_BASE_EXPORT extern const base::Feature kEnableAccessibilityTreeForViews;

// Returns true if the Views tree is exposed using an AXTree in the browser
// process. Returns false if the Views tree is exposed to accessibility
// directly.
AX_BASE_EXPORT bool IsAccessibilityTreeForViewsEnabled();

AX_BASE_EXPORT extern const base::Feature kAccessibilityFocusHighlight;

// Returns true if the accessibility focus highlight feature is enabled,
// which draws a visual highlight around the focused element on the page
// briefly whenever focus changes.
AX_BASE_EXPORT bool IsAccessibilityFocusHighlightEnabled();

AX_BASE_EXPORT extern const base::Feature kAutoDisableAccessibility;

// Returns true if accessibility will be auto-disabled after a certain
// number of user input events spanning a minimum amount of time with no
// accessibility API usage in that time.
AX_BASE_EXPORT bool IsAutoDisableAccessibilityEnabled();

#if BUILDFLAG(IS_WIN)
// Enables an experimental Chrome-specific accessibility COM API
AX_BASE_EXPORT extern const base::Feature kIChromeAccessible;

// Returns true if the IChromeAccessible COM API is enabled.
AX_BASE_EXPORT bool IsIChromeAccessibleEnabled();

AX_BASE_EXPORT extern const base::Feature kSelectiveUIAEnablement;

// Returns true if accessibility will be selectively enabled depending on the
// UIA APIs that are called, allowing non-screenreader usage to enable less of
// the accessibility system.
AX_BASE_EXPORT bool IsSelectiveUIAEnablementEnabled();

#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables ability to choose new continuous mouse following mode in Magnifier
// settings.
AX_BASE_EXPORT extern const base::Feature
    kMagnifierContinuousMouseFollowingModeSetting;

// Returns true if the feature to allow choosing the new continuous mouse
// following mode in Magnifier settings is enabled.
AX_BASE_EXPORT bool IsMagnifierContinuousMouseFollowingModeSettingEnabled();

// Enables ability to resize Docked Magnifier.
AX_BASE_EXPORT extern const base::Feature kDockedMagnifierResizing;

// Returns true if the feature which adds ability for user to grab and resize
// bottom of Docked Magnifier is enabled.
AX_BASE_EXPORT bool IsDockedMagnifierResizingEnabled();

AX_BASE_EXPORT bool IsDictationOfflineAvailable();

// Enables accessibility Dictation with the pumpkin semantic parser.
AX_BASE_EXPORT extern const base::Feature
    kExperimentalAccessibilityDictationWithPumpkin;

// Returns true if dictation with pumpkin is enabled.
AX_BASE_EXPORT bool IsExperimentalAccessibilityDictationWithPumpkinEnabled();

// Enables downloading Google TTS voices using Language Packs.
AX_BASE_EXPORT extern const base::Feature
    kExperimentalAccessibilityGoogleTtsLanguagePacks;

// Returns true if using Language Packs to download Google TTS voices is
// enabled.
AX_BASE_EXPORT bool IsExperimentalAccessibilityGoogleTtsLanguagePacksEnabled();

// Enables high-quality, network-based voices in Select-to-speak.
AX_BASE_EXPORT extern const base::Feature kEnhancedNetworkVoices;

// Returns true if network-based voices are enabled in Select-to-speak.
AX_BASE_EXPORT bool IsEnhancedNetworkVoicesEnabled();

// Enables improved Accessibility OS Settings visibility.
AX_BASE_EXPORT extern const base::Feature kAccessibilityOSSettingsVisibility;

// Returns true if improved Accessibility OS Settings visibility is enabled.
AX_BASE_EXPORT bool IsAccessibilityOSSettingsVisibilityEnabled();

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Enables Get Image Descriptions to augment existing images labels,
// rather than only provide descriptions for completely unlabeled images.
AX_BASE_EXPORT extern const base::Feature kAugmentExistingImageLabels;

// Returns true if augmenting existing image labels is enabled.
AX_BASE_EXPORT bool IsAugmentExistingImageLabelsEnabled();

// Once this flag is enabled, a single codebase in AXPosition will be used for
// handling document markers on all platforms, including the announcement of
// spelling mistakes.
AX_BASE_EXPORT extern const base::Feature kUseAXPositionForDocumentMarkers;

// Returns true if document markers are exposed on inline text boxes in the
// accessibility tree in addition to on static text nodes. This in turn enables
// AXPosition on the browser to discover and work with document markers, instead
// of the legacy code that collects document markers manually from static text
// nodes and which is different for each platform.
AX_BASE_EXPORT bool IsUseAXPositionForDocumentMarkersEnabled();

// Enable support for ARIA element reflection, for example
// element.ariaActiveDescendantElement = child;
AX_BASE_EXPORT extern const base::Feature kEnableAriaElementReflection;

// Returns true if ARIA element reflection is enabled.
AX_BASE_EXPORT bool IsAriaElementReflectionEnabled();

#if BUILDFLAG(IS_ANDROID)
// Compute the AXMode based on AccessibilityServiceInfo. If disabled,
// the AXMode is either entirely on or entirely off.
AX_BASE_EXPORT extern const base::Feature kComputeAXMode;

// Returns true if the IChromeAccessible COM API is enabled.
AX_BASE_EXPORT bool IsComputeAXModeEnabled();
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
AX_BASE_EXPORT extern const base::Feature kReadAnything;

// Returns true if read anything is enabled. This feature shows users websites,
// such as articles, in a comfortable reading experience in a side panel.
AX_BASE_EXPORT bool IsReadAnythingEnabled();

AX_BASE_EXPORT extern const base::Feature kReadAnythingWithScreen2x;

// Returns true if read anything is enabled with screen2x integration, which
// distills web pages using an ML model.
AX_BASE_EXPORT bool IsReadAnythingWithScreen2xEnabled();

// Enables using Screen AI library to add metadata for accessibility tools.
AX_BASE_EXPORT extern const base::Feature kScreenAI;

// Returns true if Screen AI Visual Annotations feature is enabled. This feature
// uses a local machine intelligence library to process browser screenshots and
// add metadata to the accessibility tree.
AX_BASE_EXPORT bool IsScreenAIVisualAnnotationsEnabled();

// Returns true if Screen AI Service is needed as either
// ScreenAIVisualAnnotations or ReadAnythingWithScreen2x are enabled.
AX_BASE_EXPORT bool IsScreenAIServiceNeeded();

// If enabled, ScreenAI library writes some debug data in /tmp.
AX_BASE_EXPORT bool IsScreenAIDebugModeEnabled();

// Enables a feature whereby inaccessible (i.e. untagged) PDFs are made
// accessible using an optical character recognition service. Due to the size of
// the OCR component, this feature targets desktop versions of Chrome for now.
AX_BASE_EXPORT extern const base::Feature kPdfOcr;

// Returns true if OCR will be performed on inaccessible (i.e. untagged) PDFs
// and the resulting text, together with its layout information, will be added
// to the accessibility tree.
AX_BASE_EXPORT bool IsPdfOcrEnabled();

// Enables a setting that can turn on/off browser vocalization of 'descriptions'
// tracks.
AX_BASE_EXPORT extern const base::Feature kTextBasedAudioDescription;

// Returns true if the setting to turn on text based audio descriptions is
// enabled.
AX_BASE_EXPORT bool IsTextBasedAudioDescriptionEnabled();
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace features

#endif  // UI_ACCESSIBILITY_ACCESSIBILITY_FEATURES_H_
