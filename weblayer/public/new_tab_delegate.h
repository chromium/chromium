// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_NEW_TAB_DELEGATE_H_
#define WEBLAYER_PUBLIC_NEW_TAB_DELEGATE_H_

#include <memory>

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
  virtual void OnNewTab(std::unique_ptr<Tab> new_tab, NewTabType type) = 0;

  // The page has requested a tab that was created by way of OnNewTab() to be
  // closed. This is sent to the NewTabDelegate set on the page created by way
  // of OnNewTab().
  virtual void CloseTab() = 0;

 protected:
  virtual ~NewTabDelegate() {}
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_NEW_TAB_DELEGATE_H_
