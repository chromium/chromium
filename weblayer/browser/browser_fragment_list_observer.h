// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_FRAGMENT_LIST_OBSERVER_H_
#define WEBLAYER_BROWSER_BROWSER_FRAGMENT_LIST_OBSERVER_H_

#include "base/observer_list_types.h"
#include "build/build_config.h"

namespace weblayer {

class BrowserFragmentListObserver : public base::CheckedObserver {
 public:
  // Called when the value of BrowserFragmentList::HasAtLeastOneResumedBrowser()
  // changes.
  virtual void OnHasAtLeastOneResumedBrowserFragmentStateChanged(
      bool new_value) {}

 protected:
  ~BrowserFragmentListObserver() override = default;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_FRAGMENT_LIST_OBSERVER_H_
