// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_PAGE_H_
#define WEBLAYER_PUBLIC_PAGE_H_

namespace weblayer {

// This objects tracks the lifetime of a loaded web page. Most of the time there
// is only one Page object per tab. However features like back-forward cache,
// prerendering etc... sometime involve the creation of additional Page objects.
// Navigation::getPage() will return the Page for a given navigation. Similarly
// it'll be the same Page object that's passed in
// NavigationObserver::OnPageDestroyed().
class Page {
 protected:
  virtual ~Page() = default;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_PAGE_H_
