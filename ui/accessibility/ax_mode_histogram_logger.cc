// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_mode_histogram_logger.h"

#include "base/metrics/histogram_functions.h"

namespace ui {

namespace {

void RecordModeFlag(AXHistogramPrefix prefix,
                    AXMode::ModeFlagHistogramValue flag) {
  switch (prefix) {
    case AXHistogramPrefix::kNone:
      base::UmaHistogramEnumeration(
          "Accessibility.ModeFlag", flag,
          AXMode::ModeFlagHistogramValue::UMA_AX_MODE_MAX);
      break;

    case AXHistogramPrefix::kBlink:
      base::UmaHistogramEnumeration(
          "Blink.Accessibility.ModeFlag", flag,
          AXMode::ModeFlagHistogramValue::UMA_AX_MODE_MAX);
  }
}

}  // anonymous namespace

void RecordAccessibilityModeHistograms(AXHistogramPrefix prefix,
                                       AXMode mode,
                                       AXMode previous_mode) {
  if (mode == previous_mode) {
    return;
  }

  // Record individual mode flags transitioning from unset to set state.
  int new_mode_flags = mode.flags() & (~previous_mode.flags());
  if (new_mode_flags) {
    if (new_mode_flags & AXMode::kNativeAPIs) {
      RecordModeFlag(prefix,
                     AXMode::ModeFlagHistogramValue::UMA_AX_MODE_NATIVE_APIS);
    }

    if (new_mode_flags & AXMode::kWebContents) {
      RecordModeFlag(prefix,
                     AXMode::ModeFlagHistogramValue::UMA_AX_MODE_WEB_CONTENTS);
    }

    if (new_mode_flags & AXMode::kInlineTextBoxes) {
      RecordModeFlag(
          prefix,
          AXMode::ModeFlagHistogramValue::UMA_AX_MODE_INLINE_TEXT_BOXES);
    }

    if (new_mode_flags & AXMode::kExtendedProperties) {
      RecordModeFlag(
          prefix,
          AXMode::ModeFlagHistogramValue::UMA_AX_MODE_EXTENDED_PROPERTIES);
    }

    if (new_mode_flags & AXMode::kHTML) {
      RecordModeFlag(prefix, AXMode::ModeFlagHistogramValue::UMA_AX_MODE_HTML);
    }

    if (new_mode_flags & AXMode::kHTMLMetadata) {
      RecordModeFlag(prefix,
                     AXMode::ModeFlagHistogramValue::UMA_AX_MODE_HTML_METADATA);
    }

    if (new_mode_flags & AXMode::kLabelImages) {
      RecordModeFlag(prefix,
                     AXMode::ModeFlagHistogramValue::UMA_AX_MODE_LABEL_IMAGES);
    }

    if (new_mode_flags & AXMode::kPDFPrinting) {
      RecordModeFlag(prefix, AXMode::ModeFlagHistogramValue::UMA_AX_MODE_PDF);
    }

    if (new_mode_flags & AXMode::kAnnotateMainNode) {
      RecordModeFlag(
          prefix,
          AXMode::ModeFlagHistogramValue::UMA_AX_MODE_ANNOTATE_MAIN_NODE);
    }

    // ui::AXMode::kFromPlatform is unconditionally filtered out and is
    // therefore never present in `mode`.
    CHECK(!mode.has_mode(ui::AXMode::kFromPlatform));

    if (new_mode_flags & AXMode::kScreenReader) {
      RecordModeFlag(prefix,
                     AXMode::ModeFlagHistogramValue::UMA_AX_MODE_SCREEN_READER);
    }
  }

  // Record forms control flag transitioning from unset to set.
  int new_filter_mode_flags =
      mode.filter_flags() & (~previous_mode.filter_flags());
  if (new_filter_mode_flags & AXMode::kFormsAndLabelsOnly) {
    switch (prefix) {
      case AXHistogramPrefix::kNone:
        base::UmaHistogramBoolean(
            "Accessibility.ExperimentalModeFlag.FormControls", true);
        break;

      case AXHistogramPrefix::kBlink:
        base::UmaHistogramBoolean(
            "Blink.Accessibility.ExperimentalModeFlag.FormControls", true);
    }
  }

  // Record exact match to a named bundle.
  AXMode::BundleHistogramValue bundle;
  if (mode == kAXModeBasic) {
    bundle = AXMode::BundleHistogramValue::kBasic;
  } else if (mode == kAXModeWebContentsOnly) {
    bundle = AXMode::BundleHistogramValue::kWebContentsOnly;
  } else if (mode == kAXModeComplete) {
    bundle = AXMode::BundleHistogramValue::kComplete;
  } else if (mode == kAXModeFormControls) {
    bundle = AXMode::BundleHistogramValue::kFormControls;
  } else {
    bundle = AXMode::BundleHistogramValue::kUnnamed;
  }

  switch (prefix) {
    case AXHistogramPrefix::kNone:
      base::UmaHistogramEnumeration("Accessibility.Bundle", bundle);
      break;

    case AXHistogramPrefix::kBlink:
      base::UmaHistogramEnumeration("Blink.Accessibility.Bundle", bundle);
  }
}

}  // namespace ui
