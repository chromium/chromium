// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_LIST_H_
#define WEBLAYER_BROWSER_BROWSER_LIST_H_

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "build/build_config.h"

namespace weblayer {

class BrowserImpl;
class BrowserListObserver;

class BrowserListProxy;

// Tracks the set of browsers.
class BrowserList {
 public:
  BrowserList(const BrowserList&) = delete;
  BrowserList& operator=(const BrowserList&) = delete;

  static BrowserList* GetInstance();

  const base::flat_set<BrowserImpl*>& browsers() { return browsers_; }

#if BUILDFLAG(IS_ANDROID)
  // Returns true if there is at least one Browser in a resumed state.
  bool HasAtLeastOneResumedBrowser();
#endif

  void AddObserver(BrowserListObserver* observer);
  void RemoveObserver(BrowserListObserver* observer);

 private:
  friend class BrowserImpl;
  friend class base::NoDestructor<BrowserList>;

  BrowserList();
  ~BrowserList();

  void AddBrowser(BrowserImpl* browser);
  void RemoveBrowser(BrowserImpl* browser);

#if BUILDFLAG(IS_ANDROID)
  void NotifyHasAtLeastOneResumedBrowserChanged();
#endif

  base::flat_set<BrowserImpl*> browsers_;
  base::ObserverList<BrowserListObserver> observers_;
  std::unique_ptr<BrowserListProxy> browser_list_proxy_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_LIST_H_
