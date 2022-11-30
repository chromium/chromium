// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_FAVICON_FETCHER_DELEGATE_H_
#define WEBLAYER_PUBLIC_FAVICON_FETCHER_DELEGATE_H_

#include "base/observer_list_types.h"

namespace gfx {
class Image;
}

namespace weblayer {

// Notified of interesting events related to FaviconFetcher.
class FaviconFetcherDelegate : public base::CheckedObserver {
 public:
  // Called when the favicon of the current navigation has changed. This may be
  // called multiple times for the same navigation.
  virtual void OnFaviconChanged(const gfx::Image& image) = 0;

 protected:
  ~FaviconFetcherDelegate() override = default;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_FAVICON_FETCHER_DELEGATE_H_
