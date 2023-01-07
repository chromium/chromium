// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_TAB_OBSERVER_H_
#define WEBLAYER_PUBLIC_TAB_OBSERVER_H_

#include <string>


class GURL;

namespace weblayer {

class TabObserver {
 public:
  // The URL bar should be updated to |url|.
  virtual void DisplayedUrlChanged(const GURL& url) {}

  // Triggered when the render process dies, either due to crash or killed by
  // the system to reclaim memory.
  virtual void OnRenderProcessGone() {}

  // Called when the title of this tab changes. Note before the page sets a
  // title, the title may be a portion of the Uri.
  virtual void OnTitleUpdated(const std::u16string& title) {}

 protected:
  virtual ~TabObserver() {}
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_TAB_OBSERVER_H_
