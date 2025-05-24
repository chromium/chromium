// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_MODE_H_
#define UI_ACCESSIBILITY_AX_MODE_H_

#include <stdint.h>

#include <ostream>
#include <string>

#include "ui/accessibility/ax_base_export.h"

namespace ax::mojom {
class AXModeDataView;
}

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
}

namespace ui {

class AX_BASE_EXPORT AXMode {
 public:
  // LINT.IfChange
  // No modes set (default).
  static constexpr uint32_t kNone = 0;

  static constexpr uint32_t kFirstModeFlag = 1 << 0;

  // Native accessibility APIs, specific to each platform, are enabled.
  // When this mode is set that indicates the presence of a third-party
  // client accessing Chrome via accessibility APIs. However, unless one
  // of the modes below is set, the contents of web pages will not be
  // accessible.
  static constexpr uint32_t kNativeAPIs = 1 << 0;

  // The renderer process will generate an accessibility tree containing
  // basic information about all nodes, including role, name, value,
  // state, and location. This is the minimum mode required in order for
  // web contents to be accessible, and the remaining modes are meaningless
  // unless this one is set.
  //
  // Note that sometimes this mode will be set when kNativeAPI is not, when the
  // content layer embedder is providing accessibility support via some other
  // mechanism other than what's implemented in content/browser.
  static constexpr uint32_t kWebContents = 1 << 1;

  // The accessibility tree will contain inline text boxes, which are
  // necessary to expose information about line breaks and word boundaries.
  // Without this mode, you can retrieve the plaintext value of a text field
  // but not the information about how it's broken down into lines.
  //
  // Note that when this mode is off it's still possible to request inline
  // text boxes for a specific node on-demand, asynchronously.
  static constexpr uint32_t kInlineTextBoxes = 1 << 2;

  // The accessibility tree will contain extra accessibility
  // attributes typically only needed by screen readers and other
  // assistive technology for blind users. Examples include text style
  // attributes, table cell information, live region properties, range
  // values, and relationship attributes. Note that the HTML tag, ID, class, and
  // display attributes will also be included.
  static constexpr uint32_t kExtendedProperties = 1 << 3;

  // The accessibility tree will contain all the HTML attributes for all
  // accessibility nodes that come from web content. This effectively dumps all
  // the HTML attributes as found in the HTML source, or as created by
  // Javascript, in the accessibility tree, potentially taking up a lot of
  // memory.
  static constexpr uint32_t kHTML = 1 << 4;

  // The accessibility tree will contain some metadata from the
  // HTML HEAD, such as <meta> tags, in AXTreeData. Only supported
  // when doing a tree snapshot, there's no support for keeping these
  // in sync if a page changes them dynamically.
  static constexpr uint32_t kHTMLMetadata = 1 << 5;

  // The accessibility tree will contain automatic image annotations.
  static constexpr uint32_t kLabelImages = 1 << 6;

  // The accessibility tree will contain enough information to export
  // an accessible PDF when printing to PDF.
  static constexpr uint32_t kPDFPrinting = 1 << 7;

  // The PDF renderer process will run OCR to extract text from an inaccessible
  // PDF and add it to the accessibility tree.
  static constexpr uint32_t kPDFOcr = 1 << 8;

  // The accessibility tree will have the main node annotated.
  static constexpr uint32_t kAnnotateMainNode = 1 << 9;

  // Indicates that the bundle containing this and other flags is being applied
  // in response to an interaction with the platform's accessibility
  // integration. This meta-flag, which must always be used in combination with
  // one or more other flags and is never sent to renderers, is used to
  // selectively suppress or permit accessibility modes that are set due to
  // interactions from accessibility tools.
  static constexpr uint32_t kFromPlatform = 1 << 10;

  // The accessibility tree will contain content and properties only needed by
  // actual screen readers. This mode is only present when a known screen reader
  // is detected, e.g. ChromeVox, Talkback, JAWS, NVDA, Narrator, etc.
  static constexpr uint32_t kScreenReader = 1 << 11;

  // Update this to include the last supported mode flag. If you add
  // another, be sure to update the stream insertion operator for
  // logging and debugging, as well as AccessibilityModeFlagEnum (and
  // related metrics callsites, see: |ModeFlagHistogramValue|).
  static constexpr uint32_t kLastModeFlag = 1 << 11;
  // LINT.ThenChange(//chrome/browser/metrics/accessibility_state_provider.cc,//chrome/browser/performance_manager/metrics/metrics_provider_common.cc,//chrome/browser/resources/accessibility/accessibility.ts,//tools/metrics/histograms/enums.xml,//ui/accessibility/ax_mode_histogram_logger.cc)

  constexpr AXMode() : flags_(kNone), filter_flags_(kNone) {}
  constexpr AXMode(uint32_t flags) : flags_(flags), filter_flags_(kNone) {}
  constexpr AXMode(uint32_t flags, uint32_t filter_flags)
      : flags_(flags), filter_flags_(filter_flags) {}

  constexpr bool has_mode(uint32_t flag) const {
    return (flags_ & flag) == flag;
  }

  constexpr void set_mode(uint32_t flag, bool value) {
    flags_ = value ? (flags_ | flag) : (flags_ & ~flag);
  }

  constexpr uint32_t flags() const { return flags_; }

  constexpr uint32_t filter_flags() const { return filter_flags_; }

  constexpr bool is_mode_off() const { return !flags_ && !filter_flags_; }

  constexpr AXMode& operator|=(const AXMode& rhs) {
    flags_ |= rhs.flags_;
    filter_flags_ |= rhs.filter_flags_;
    return *this;
  }

  constexpr AXMode& operator&=(const AXMode& rhs) {
    flags_ &= rhs.flags_;
    filter_flags_ &= rhs.filter_flags_;
    return *this;
  }

  constexpr AXMode operator~() const { return {~flags_, ~filter_flags_}; }

  bool HasFilterFlags(uint32_t filter_flag) const;
  void SetFilterFlags(uint32_t filter_flag, bool value);

  std::string ToString() const;

  friend constexpr bool operator==(const AXMode&, const AXMode&) = default;

  // IMPORTANT!
  // These values are written to logs.  Do not renumber or delete
  // existing items; add new entries to the end of the list.
  enum class ModeFlagHistogramValue {
    UMA_AX_MODE_NATIVE_APIS = 0,
    UMA_AX_MODE_WEB_CONTENTS = 1,
    UMA_AX_MODE_INLINE_TEXT_BOXES = 2,
    UMA_AX_MODE_EXTENDED_PROPERTIES = 3,
    UMA_AX_MODE_HTML = 4,
    UMA_AX_MODE_HTML_METADATA = 5,
    UMA_AX_MODE_LABEL_IMAGES = 6,
    UMA_AX_MODE_PDF = 7,
    UMA_AX_MODE_PDF_OCR = 8,
    UMA_AX_MODE_ANNOTATE_MAIN_NODE = 9,
    UMA_AX_MODE_FROM_PLATFORM = 10,
    UMA_AX_MODE_SCREEN_READER = 11,

    // This must always be the last enum. It's okay for its value to
    // increase, but none of the other enum values may change.
    UMA_AX_MODE_MAX
  };

  // IMPORTANT!
  // These values are written to logs. Do not renumber or delete
  // existing items; add new entries to the end of the list.
  //
  // LINT.IfChange
  enum class BundleHistogramValue {
    // The unnamed bucket is a catch all for modes that do not match one of the
    // named sets.
    kUnnamed = 0,
    // See static constants below for a description of each context.
    kBasic = 1,
    kWebContentsOnly = 2,
    kComplete = 3,
    kCompleteNoHTML = 4,
    kFormControls = 5,

    // This must always be the last enum. It's okay for its value to
    // increase, but none of the other enum values may change.
    kMaxValue = 5
  };
  // LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:AccessibilityModeBundleEnum)

  // Filter Flags
  // These are defined separately from the base flags to avoid conflating
  // flags that are additive from flags that remove information.
  static constexpr uint32_t kFilterFirstFlag = 1 << 0;
  // kFormsAndLabelsOnly filters out everything except for forms and labels
  // necessary for password managers to operate.
  static constexpr uint32_t kFormsAndLabelsOnly = 1 << 0;
  // This flag filters all nodes that are not on-screen. This is guarded behind
  // a feature flag. Please see kAccessibilityOnScreenMode.
  static constexpr uint32_t kOnScreenOnly = 1 << 1;
  static constexpr uint32_t kFilterLastFlag = 1 << 1;

 private:
  friend struct mojo::StructTraits<ax::mojom::AXModeDataView, AXMode>;

  // The core flags_ are in a bit field and can be added together to enable more
  // accessibility properties to be computed and exposed.
  uint32_t flags_ = 0U;
  // The filter_flags_ are also in a bit field but they are subtractive. They
  // represent the idea that nodes or properties may be removed for performance.
  uint32_t filter_flags_ = 0U;
};

constexpr AXMode operator|(const AXMode& lhs, const AXMode& rhs) {
  return {lhs.flags() | rhs.flags(), lhs.filter_flags() | rhs.filter_flags()};
}

constexpr AXMode operator&(const AXMode& lhs, const AXMode& rhs) {
  return {lhs.flags() & rhs.flags(), lhs.filter_flags() & rhs.filter_flags()};
}

// Used when an AT that only require basic accessibility information, such as
// a dictation tool, is present.
inline constexpr AXMode kAXModeBasic(AXMode::kNativeAPIs |
                                     AXMode::kWebContents);

// Used when complete accessibility access is desired but a third-party AT is
// not present.
inline constexpr AXMode kAXModeWebContentsOnly(AXMode::kWebContents |
                                               AXMode::kInlineTextBoxes |
                                               AXMode::kExtendedProperties);

// Used when an AT that requires full accessibility access, such as a screen
// reader, is present.
inline constexpr AXMode kAXModeComplete(AXMode::kNativeAPIs |
                                        AXMode::kWebContents |
                                        AXMode::kInlineTextBoxes |
                                        AXMode::kExtendedProperties);

// Used for DOM Inspector.
// TODO(accessibility) Inspector should not need kInlineTextBoxes.
inline constexpr AXMode kAXModeInspector(AXMode::kWebContents |
                                         AXMode::kInlineTextBoxes |
                                         AXMode::kExtendedProperties |
                                         AXMode::kScreenReader);

// The default for tests is to include kScreenReader mode, ensuring that the
// entire tree is built, rather than the default for API consumers, which
// assumes there is no screen reader present, enabling optimizations such as
// removal of AXNodes that are not currently relevant.
inline constexpr AXMode kAXModeDefaultForTests(
    AXMode::kNativeAPIs | AXMode::kWebContents | AXMode::kInlineTextBoxes |
    AXMode::kExtendedProperties | AXMode::kHTML | AXMode::kScreenReader);

// Used when tools that only need autofill functionality are present.
inline constexpr AXMode kAXModeFormControls(AXMode::kNativeAPIs |
                                                AXMode::kWebContents,
                                            AXMode::kFormsAndLabelsOnly);

// If adding a new named set of mode flags, please update BundleHistogramValue.
inline constexpr AXMode kAXModeOnScreen(AXMode::kNativeAPIs |
                                            AXMode::kWebContents |
                                            AXMode::kInlineTextBoxes |
                                            AXMode::kExtendedProperties,
                                        AXMode::kOnScreenOnly);

// For debugging, test assertions, etc.
AX_BASE_EXPORT std::ostream& operator<<(std::ostream& stream,
                                        const AXMode& mode);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_MODE_H_
