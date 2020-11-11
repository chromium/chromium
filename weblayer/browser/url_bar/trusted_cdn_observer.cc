// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/url_bar/trusted_cdn_observer.h"

#include "components/embedder_support/android/util/cdn_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace weblayer {

TrustedCDNObserver::TrustedCDNObserver(content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

TrustedCDNObserver::~TrustedCDNObserver() = default;

void TrustedCDNObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Skip subframe, same-document, or non-committed navigations (downloads or
  // 204/205 responses).
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  publisher_url_ = embedder_support::GetPublisherURL(navigation_handle);

  // Trigger url bar update.
  web_contents()->DidChangeVisibleSecurityState();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TrustedCDNObserver)

}  // namespace weblayer