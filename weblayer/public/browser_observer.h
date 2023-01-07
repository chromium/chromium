// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_BROWSER_OBSERVER_H_
#define WEBLAYER_PUBLIC_BROWSER_OBSERVER_H_

#include "base/observer_list_types.h"

namespace weblayer {

class Tab;

class BrowserObserver : public base::CheckedObserver {
 public:
  // A Tab has been been added to the Browser.
  virtual void OnTabAdded(Tab* tab) {}

  // A Tab has been removed from the Browser. |active_tab_changed| indicates
  // if the active tab changed as a result. If the active tab changed,
  // OnActiveTabChanged() is also called.
  virtual void OnTabRemoved(Tab* tab, bool active_tab_changed) {}

  // The tab the user is interacting with has changed. |tab| may be null if no
  // tabs are active.
  virtual void OnActiveTabChanged(Tab* tab) {}

 protected:
  ~BrowserObserver() override {}
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_BROWSER_OBSERVER_H_
