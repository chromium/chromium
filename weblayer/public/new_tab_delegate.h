// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_NEW_TAB_DELEGATE_H_
#define WEBLAYER_PUBLIC_NEW_TAB_DELEGATE_H_

namespace weblayer {

class Tab;

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.weblayer_private
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ImplNewTabType
// Corresponds to type of browser the page requested.
enum class NewTabType {
  // The new browser should be opened in the foreground.
  kForeground = 0,

  // The new browser should be opened in the foreground.
  kBackground,

  // The page requested the browser be shown in a new window with minimal
  // browser UI. For example, no tabstrip.
  kNewPopup,

  // The page requested the browser be shown in a new window.
  kNewWindow,
};

// An interface that allows clients to handle requests for new browsers, or
// in web terms, a new popup/window (and random other things).
class NewTabDelegate {
 public:
  // Called when a new tab is created by the browser. |new_tab| is owned by the
  // browser.
  virtual void OnNewTab(Tab* new_tab, NewTabType type) = 0;

 protected:
  virtual ~NewTabDelegate() {}
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_NEW_TAB_DELEGATE_H_
