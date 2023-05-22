// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_FRAGMENT_LIST_H_
#define WEBLAYER_BROWSER_BROWSER_FRAGMENT_LIST_H_

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"

namespace weblayer {

class BrowserFragmentImpl;
class BrowserFragmentListObserver;

// Tracks the set of browsers.
class BrowserFragmentList {
 public:
  BrowserFragmentList();
  ~BrowserFragmentList();

  BrowserFragmentList(const BrowserFragmentList&) = delete;
  BrowserFragmentList& operator=(const BrowserFragmentList&) = delete;

  static BrowserFragmentList* GetInstance();

  const base::flat_set<BrowserFragmentImpl*>& browser_fragments() {
    return browser_fragments_;
  }

  // Returns true if there is at least one BrowserFragmentImpl in a resumed
  // state.
  bool HasAtLeastOneResumedBrowser();

  void AddObserver(BrowserFragmentListObserver* observer);
  void RemoveObserver(BrowserFragmentListObserver* observer);

 private:
  friend class BrowserFragmentImpl;
  friend class base::NoDestructor<BrowserFragmentList>;

  void AddBrowserFragment(BrowserFragmentImpl* browser_fragment);
  void RemoveBrowserFragment(BrowserFragmentImpl* browser_fragment);

  void NotifyHasAtLeastOneResumedBrowserFragmentChanged();

  base::flat_set<BrowserFragmentImpl*> browser_fragments_;
  base::ObserverList<BrowserFragmentListObserver> observers_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_FRAGMENT_LIST_H_
