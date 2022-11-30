// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_LIST_OBSERVER_H_
#define WEBLAYER_BROWSER_BROWSER_LIST_OBSERVER_H_

#include "base/observer_list_types.h"
#include "build/build_config.h"

namespace weblayer {

class Browser;

class BrowserListObserver : public base::CheckedObserver {
 public:
#if BUILDFLAG(IS_ANDROID)
  // Called when the value of BrowserList::HasAtLeastOneResumedBrowser()
  // changes.
  virtual void OnHasAtLeastOneResumedBrowserStateChanged(bool new_value) {}
#endif

  virtual void OnBrowserCreated(Browser* browser) {}

  virtual void OnBrowserDestroyed(Browser* browser) {}

 protected:
  ~BrowserListObserver() override = default;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_LIST_OBSERVER_H_
