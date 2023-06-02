// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_fragment_list.h"

#include "base/no_destructor.h"
#include "weblayer/browser/browser_fragment_impl.h"
#include "weblayer/browser/browser_fragment_list_observer.h"

namespace weblayer {

inline BrowserFragmentList::BrowserFragmentList() = default;
inline BrowserFragmentList::~BrowserFragmentList() = default;

// static
BrowserFragmentList* BrowserFragmentList::GetInstance() {
  static base::NoDestructor<BrowserFragmentList> browser_fragment_list;
  return browser_fragment_list.get();
}

void BrowserFragmentList::AddBrowserFragment(
    BrowserFragmentImpl* browser_fragment) {
  CHECK(!browser_fragments_.contains(browser_fragment));
  browser_fragments_.insert(browser_fragment);
}

void BrowserFragmentList::RemoveBrowserFragment(
    BrowserFragmentImpl* browser_fragment) {
  CHECK(browser_fragments_.contains(browser_fragment));
  browser_fragments_.erase(browser_fragment);
}

void BrowserFragmentList::AddObserver(BrowserFragmentListObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserFragmentList::RemoveObserver(
    BrowserFragmentListObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool BrowserFragmentList::HasAtLeastOneResumedBrowser() {
  return base::ranges::any_of(browser_fragments_,
                              &BrowserFragmentImpl::fragment_resumed);
}

void BrowserFragmentList::NotifyHasAtLeastOneResumedBrowserFragmentChanged() {
  const bool value = HasAtLeastOneResumedBrowser();
  for (BrowserFragmentListObserver& observer : observers_) {
    observer.OnHasAtLeastOneResumedBrowserFragmentStateChanged(value);
  }
}

}  // namespace weblayer
