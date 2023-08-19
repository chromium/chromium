// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_SCOPED_PREFERRED_SCROLLER_STYLE_MAC_H_
#define UI_BASE_TEST_SCOPED_PREFERRED_SCROLLER_STYLE_MAC_H_

#include <memory>

namespace base::apple {
class ScopedObjCClassSwizzler;
}

namespace ui::test {

// Overrides system setting for scrollbar style with NSScrollerOverlay if we
// want the scrollbar to overlay. Otherwise, override with
// NSScrollerStyleLegacy which means "always show scrollbars".
class ScopedPreferredScrollerStyle {
 public:
  explicit ScopedPreferredScrollerStyle(bool overlay);

  ScopedPreferredScrollerStyle(const ScopedPreferredScrollerStyle&) = delete;
  ScopedPreferredScrollerStyle& operator=(const ScopedPreferredScrollerStyle&) =
      delete;

  ~ScopedPreferredScrollerStyle();

 private:
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler> swizzler_;

  // True if the scrollbar style should overlay.
  bool overlay_;
};

}  // namespace ui::test

#endif  // UI_BASE_TEST_SCOPED_PREFERRED_SCROLLER_STYLE_MAC_H_
