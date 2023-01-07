// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_FAVICON_FAVICON_FETCHER_IMPL_H_
#define WEBLAYER_BROWSER_FAVICON_FAVICON_FETCHER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "weblayer/browser/favicon/favicon_tab_helper.h"
#include "weblayer/public/favicon_fetcher.h"

namespace content {
class WebContents;
}

namespace weblayer {

class FaviconFetcherDelegate;

// FaviconFetcher implementation that largely delegates to FaviconTabHelper
// for the real implementation.
class FaviconFetcherImpl : public FaviconFetcher {
 public:
  FaviconFetcherImpl(content::WebContents* web_contents,
                     FaviconFetcherDelegate* delegate);
  FaviconFetcherImpl(const FaviconFetcherImpl&) = delete;
  FaviconFetcherImpl& operator=(const FaviconFetcherImpl&) = delete;
  ~FaviconFetcherImpl() override;

  // FaviconFetcher:
  gfx::Image GetFavicon() override;

 private:
  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<FaviconTabHelper::ObserverSubscription>
      observer_subscription_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_FAVICON_FAVICON_FETCHER_IMPL_H_
