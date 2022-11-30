// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_SHELL_ANDROID_BROWSERTESTS_APK_TRANSLATE_TEST_BRIDGE_H_
#define WEBLAYER_SHELL_ANDROID_BROWSERTESTS_APK_TRANSLATE_TEST_BRIDGE_H_

#include "weblayer/browser/translate_compact_infobar.h"

namespace weblayer {

// Bridge to support translate_browsertest.cc to calling into Java.
class TranslateTestBridge {
 public:
  TranslateTestBridge();
  ~TranslateTestBridge();

  TranslateTestBridge(const TranslateTestBridge&) = delete;
  TranslateTestBridge& operator=(const TranslateTestBridge&) = delete;

  enum class OverflowMenuItemId {
    NEVER_TRANSLATE_LANGUAGE = 0,
    NEVER_TRANSLATE_SITE = 1,
  };

  // Instructs the Java infobar to select the button corresponding to
  // |action_type|.
  static void SelectButton(TranslateCompactInfoBar* infobar,
                           TranslateCompactInfoBar::ActionType action_type);

  // Instructs the Java infobar to click the specified overflow menu item.
  static void ClickOverflowMenuItem(TranslateCompactInfoBar* infobar,
                                    OverflowMenuItemId item_id);
};

}  // namespace weblayer

#endif  // WEBLAYER_SHELL_ANDROID_BROWSERTESTS_APK_TRANSLATE_TEST_BRIDGE_H_
