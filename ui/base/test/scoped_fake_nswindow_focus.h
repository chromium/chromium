// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_SCOPED_FAKE_NSWINDOW_FOCUS_H_
#define UI_BASE_TEST_SCOPED_FAKE_NSWINDOW_FOCUS_H_

#include <memory>

namespace base::apple {
class ScopedObjCClassSwizzler;
}

namespace ui::test {

// Simulates key and main status by listening for -makeKeyWindow,
// -makeMainWindow and -orderOut:. This allows test coverage of code relying on
// window focus changes without resorting to an interactive_ui_test.
class ScopedFakeNSWindowFocus {
 public:
  ScopedFakeNSWindowFocus();

  ScopedFakeNSWindowFocus(const ScopedFakeNSWindowFocus&) = delete;
  ScopedFakeNSWindowFocus& operator=(const ScopedFakeNSWindowFocus&) = delete;

  ~ScopedFakeNSWindowFocus();

 private:
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler> is_main_swizzler_;
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler> make_main_swizzler_;
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler> resign_main_swizzler_;
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler> is_key_swizzler_;
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler> make_key_swizzler_;
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler> resign_key_swizzler_;
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler> order_out_swizzler_;
};

}  // namespace ui::test

#endif  // UI_BASE_TEST_SCOPED_FAKE_NSWINDOW_FOCUS_H_
