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

#if BUILDFLAG(IS_ANDROID)
#include "weblayer/browser/browser_list_proxy.h"
#endif

namespace weblayer {

// static
BrowserList* BrowserList::GetInstance() {
  static base::NoDestructor<BrowserList> browser_list;
  return browser_list.get();
}

#if BUILDFLAG(IS_ANDROID)
bool BrowserList::HasAtLeastOneResumedBrowser() {
  return base::ranges::any_of(browsers_, &BrowserImpl::fragment_resumed);
}
#endif

void BrowserList::AddObserver(BrowserListObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserList::RemoveObserver(BrowserListObserver* observer) {
  observers_.RemoveObserver(observer);
}

BrowserList::BrowserList() {
#if BUILDFLAG(IS_ANDROID)
  browser_list_proxy_ = std::make_unique<BrowserListProxy>();
  AddObserver(browser_list_proxy_.get());
#endif
}

BrowserList::~BrowserList() {
#if BUILDFLAG(IS_ANDROID)
  RemoveObserver(browser_list_proxy_.get());
#endif
}

void BrowserList::AddBrowser(BrowserImpl* browser) {
  DCHECK(!browsers_.contains(browser));
#if BUILDFLAG(IS_ANDROID)
  // Browsers should not start out resumed.
  DCHECK(!browser->fragment_resumed());
#endif
  browsers_.insert(browser);
  for (BrowserListObserver& observer : observers_)
    observer.OnBrowserCreated(browser);
}

void BrowserList::RemoveBrowser(BrowserImpl* browser) {
  DCHECK(browsers_.contains(browser));
#if BUILDFLAG(IS_ANDROID)
  // Browsers should not be resumed when being destroyed.
  DCHECK(!browser->fragment_resumed());
#endif
  browsers_.erase(browser);

  for (BrowserListObserver& observer : observers_)
    observer.OnBrowserDestroyed(browser);
}

#if BUILDFLAG(IS_ANDROID)
void BrowserList::NotifyHasAtLeastOneResumedBrowserChanged() {
  const bool value = HasAtLeastOneResumedBrowser();
  for (BrowserListObserver& observer : observers_)
    observer.OnHasAtLeastOneResumedBrowserStateChanged(value);
}
#endif

}  // namespace weblayer
