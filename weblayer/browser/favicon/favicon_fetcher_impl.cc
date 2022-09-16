// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/favicon/favicon_fetcher_impl.h"

#include "ui/gfx/image/image.h"
#include "weblayer/browser/favicon/favicon_tab_helper.h"
#include "weblayer/public/favicon_fetcher_delegate.h"

#include "base/logging.h"

namespace weblayer {

FaviconFetcherImpl::FaviconFetcherImpl(content::WebContents* web_contents,
                                       FaviconFetcherDelegate* delegate)
    : web_contents_(web_contents),
      observer_subscription_(FaviconTabHelper::FromWebContents(web_contents)
                                 ->RegisterFaviconFetcherDelegate(delegate)) {}

FaviconFetcherImpl::~FaviconFetcherImpl() = default;

gfx::Image FaviconFetcherImpl::GetFavicon() {
  return FaviconTabHelper::FromWebContents(web_contents_)->favicon();
}

}  // namespace weblayer
