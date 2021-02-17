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

AX_BASE_EXPORT extern const base::Feature kEnableAccessibilityExposeDisplayNone;

// Returns true if "display: none" nodes should be exposed to the
// browser process AXTree.
AX_BASE_EXPORT bool IsAccessibilityExposeDisplayNoneEnabled();

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

#if defined(OS_WIN)
// Enables an experimental Chrome-specific accessibility COM API
AX_BASE_EXPORT extern const base::Feature kIChromeAccessible;

// Returns true if the IChromeAccessible COM API is enabled.
AX_BASE_EXPORT bool IsIChromeAccessibleEnabled();

#endif  // defined(OS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables new magnifier focus following feature, which provides a richer
// focus following experience.
AX_BASE_EXPORT extern const base::Feature kMagnifierNewFocusFollowing;

// Returns true if the new magnifier focus following feature is enabled.
AX_BASE_EXPORT bool IsMagnifierNewFocusFollowingEnabled();

// Enables new magnifier panning improvements feature, which adds
// additional keyboard and mouse panning functionality in Magnifier.
AX_BASE_EXPORT extern const base::Feature kMagnifierPanningImprovements;

// Returns true if the new magnifier panning improvements feature is enabled.
AX_BASE_EXPORT bool IsMagnifierPanningImprovementsEnabled();

// Enables ability to choose new continuous mouse following mode in Magnifier
// settings.
AX_BASE_EXPORT extern const base::Feature
    kMagnifierContinuousMouseFollowingModeSetting;

// Returns true if the feature to allow choosing the new continuous mouse
// following mode in Magnifier settings is enabled.
AX_BASE_EXPORT bool IsMagnifierContinuousMouseFollowingModeSettingEnabled();
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables enhanced Select-to-speak features that allow users broader control
// of TTS (pause, resume, skip between sentences and paragraphs).
AX_BASE_EXPORT extern const base::Feature kSelectToSpeakNavigationControl;

// Returns true if enhanced Select-to-speak features are enabled.
AX_BASE_EXPORT bool IsSelectToSpeakNavigationControlEnabled();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace features

#endif  // UI_ACCESSIBILITY_ACCESSIBILITY_FEATURES_H_
