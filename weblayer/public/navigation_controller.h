// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_NAVIGATION_CONTROLLER_H_
#define WEBLAYER_PUBLIC_NAVIGATION_CONTROLLER_H_

#include <algorithm>

class GURL;

namespace weblayer {
class NavigationObserver;

class NavigationController {
 public:
  virtual ~NavigationController() {}

  virtual void AddObserver(NavigationObserver* observer) = 0;

  virtual void RemoveObserver(NavigationObserver* observer) = 0;

  virtual void Navigate(const GURL& url) = 0;

  virtual void GoBack() = 0;

  virtual void GoForward() = 0;

  virtual bool CanGoBack() = 0;

  virtual bool CanGoForward() = 0;

  virtual void Reload() = 0;

  virtual void Stop() = 0;

  // Gets the number of entries in the back/forward list.
  virtual int GetNavigationListSize() = 0;

  // Gets the index of the current entry in the back/forward list, or -1 if
  // there are no entries.
  virtual int GetNavigationListCurrentIndex() = 0;

  // Gets the URL of the given entry in the back/forward list, or an empty GURL
  // if there is no navigation entry at that index.
  virtual GURL GetNavigationEntryDisplayURL(int index) = 0;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_NAVIGATION_CONTROLLER_H_
