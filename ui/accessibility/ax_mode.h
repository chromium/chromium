// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_MODE_H_
#define UI_ACCESSIBILITY_AX_MODE_H_

#include <stdint.h>

#include <ostream>
#include <string>

#include "ui/accessibility/ax_base_export.h"

namespace ui {

class AX_BASE_EXPORT AXMode {
 public:
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
  // values, and relationship attributes.
  static constexpr uint32_t kScreenReader = 1 << 3;

  // The accessibility tree will contain the HTML tag name and HTML attributes
  // for all accessibility nodes that come from web content.
  static constexpr uint32_t kHTML = 1 << 4;

  // The accessibility tree will contain some metadata from the
  // HTML HEAD, such as <meta> tags, in AXTreeData. Only supported
  // when doing a tree snapshot, there's no support for keeping these
  // in sync if a page changes them dynamically.
  static constexpr uint32_t kHTMLMetadata = 1 << 5;

  // The accessibility tree will contain automatic image annotations.
  static constexpr uint32_t kLabelImages = 1 << 6;

  // The accessibility tree will contain enough information to export
  // an accessible PDF.
  static constexpr uint32_t kPDF = 1 << 7;

  // Update this to include the last supported mode flag. If you add
  // another, be sure to update the stream insertion operator for
  // logging and debugging, as well as AccessibilityModeFlagEnum (and
  // related metrics callsites, see: |ModeFlagHistogramValue|).
  static constexpr uint32_t kLastModeFlag = 1 << 7;

  constexpr AXMode() : flags_(kNone) {}
  constexpr AXMode(uint32_t flags) : flags_(flags) {}

  bool has_mode(uint32_t flag) const { return (flags_ & flag) == flag; }

  void set_mode(uint32_t flag, bool value) {
    flags_ = value ? (flags_ | flag) : (flags_ & ~flag);
  }

  uint32_t flags() const { return flags_; }

  bool operator==(AXMode rhs) const { return flags_ == rhs.flags_; }

  bool is_mode_off() const { return flags_ == 0; }

  bool operator!=(AXMode rhs) const { return !(*this == rhs); }

  AXMode& operator|=(const AXMode& rhs) {
    flags_ |= rhs.flags_;
    return *this;
  }

  std::string ToString() const;

  uint32_t flags_ = 0U;

  // IMPORTANT!
  // These values are written to logs.  Do not renumber or delete
  // existing items; add new entries to the end of the list.
  enum class ModeFlagHistogramValue {
    UMA_AX_MODE_NATIVE_APIS = 0,
    UMA_AX_MODE_WEB_CONTENTS = 1,
    UMA_AX_MODE_INLINE_TEXT_BOXES = 2,
    UMA_AX_MODE_SCREEN_READER = 3,
    UMA_AX_MODE_HTML = 4,
    UMA_AX_MODE_HTML_METADATA = 5,
    UMA_AX_MODE_LABEL_IMAGES = 6,
    UMA_AX_MODE_PDF = 7,

    // This must always be the last enum. It's okay for its value to
    // increase, but none of the other enum values may change.
    UMA_AX_MODE_MAX
  };
};

// Used when an AT that only require basic accessibility information, such as
// a dictation tool, is present.
static constexpr AXMode kAXModeBasic(AXMode::kNativeAPIs |
                                     AXMode::kWebContents);

// Used when complete accessibility access is desired but a third-party AT is
// not present.
static constexpr AXMode kAXModeWebContentsOnly(AXMode::kWebContents |
                                               AXMode::kInlineTextBoxes |
                                               AXMode::kScreenReader |
                                               AXMode::kHTML);

// Used when an AT that requires full accessibility access, such as a screen
// reader, is present.
static constexpr AXMode kAXModeComplete(AXMode::kNativeAPIs |
                                        AXMode::kWebContents |
                                        AXMode::kInlineTextBoxes |
                                        AXMode::kScreenReader | AXMode::kHTML);

// Similar to kAXModeComplete, used when an AT that requires full accessibility
// access, but does not need all HTML properties or attributes.
static constexpr AXMode kAXModeCompleteNoHTML(AXMode::kNativeAPIs |
                                              AXMode::kWebContents |
                                              AXMode::kInlineTextBoxes |
                                              AXMode::kScreenReader);

// For debugging, test assertions, etc.
AX_BASE_EXPORT std::ostream& operator<<(std::ostream& stream,
                                        const AXMode& mode);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_MODE_H_
