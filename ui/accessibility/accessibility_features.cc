// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/accessibility_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace features {

// Enable recognizing "aria-virtualcontent" as a valid aria property.
const base::Feature kEnableAccessibilityAriaVirtualContent{
    "AccessibilityAriaVirtualContent", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsAccessibilityAriaVirtualContentEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kEnableAccessibilityAriaVirtualContent);
}

// Enable exposing "display: none" nodes to the browser process AXTree
const base::Feature kEnableAccessibilityExposeDisplayNone{
    "AccessibilityExposeDisplayNone", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsAccessibilityExposeDisplayNoneEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kEnableAccessibilityExposeDisplayNone);
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

#if defined(OS_WIN)
const base::Feature kIChromeAccessible{"IChromeAccessible",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

bool IsIChromeAccessibleEnabled() {
  return base::FeatureList::IsEnabled(::features::kIChromeAccessible);
}
#endif  // defined(OS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const base::Feature kMagnifierNewFocusFollowing{
    "MagnifierNewFocusFollowing", base::FEATURE_ENABLED_BY_DEFAULT};

bool IsMagnifierNewFocusFollowingEnabled() {
  return base::FeatureList::IsEnabled(::features::kMagnifierNewFocusFollowing);
}

const base::Feature kMagnifierPanningImprovements{
    "MagnifierPanningImprovements", base::FEATURE_ENABLED_BY_DEFAULT};

bool IsMagnifierPanningImprovementsEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kMagnifierPanningImprovements);
}

const base::Feature kMagnifierContinuousMouseFollowingModeSetting{
    "MagnifierContinuousMouseFollowingModeSetting",
    base::FEATURE_DISABLED_BY_DEFAULT};

bool IsMagnifierContinuousMouseFollowingModeSettingEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kMagnifierContinuousMouseFollowingModeSetting);
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
const base::Feature kSelectToSpeakNavigationControl{
    "SelectToSpeakNavigationControl", base::FEATURE_ENABLED_BY_DEFAULT};

bool IsSelectToSpeakNavigationControlEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kSelectToSpeakNavigationControl);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace features
