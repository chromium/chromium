// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_list.h"

#include <functional>

#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "weblayer/browser/browser_impl.h"
#include "weblayer/browser/browser_list_observer.h"

#include "weblayer/browser/browser_list_proxy.h"

namespace weblayer {

// static
BrowserList* BrowserList::GetInstance() {
  static base::NoDestructor<BrowserList> browser_list;
  return browser_list.get();
}

void BrowserList::AddObserver(BrowserListObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserList::RemoveObserver(BrowserListObserver* observer) {
  observers_.RemoveObserver(observer);
}

BrowserList::BrowserList() {
  browser_list_proxy_ = std::make_unique<BrowserListProxy>();
  AddObserver(browser_list_proxy_.get());
}

BrowserList::~BrowserList() {
  RemoveObserver(browser_list_proxy_.get());
}

void BrowserList::AddBrowser(BrowserImpl* browser) {
  DCHECK(!browsers_.contains(browser));
  browsers_.insert(browser);
  for (BrowserListObserver& observer : observers_)
    observer.OnBrowserCreated(browser);
}

void BrowserList::RemoveBrowser(BrowserImpl* browser) {
  DCHECK(browsers_.contains(browser));
  browsers_.erase(browser);

  for (BrowserListObserver& observer : observers_)
    observer.OnBrowserDestroyed(browser);
}

}  // namespace weblayer
