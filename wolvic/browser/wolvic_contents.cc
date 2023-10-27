// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/wolvic_contents.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

#include "wolvic/wolvic_browser_context.h"
#include "wolvic/wolvic_content_browser_client.h"

namespace wolvic {

using content::WebContents;

const void* const kWolvicContentsUserDataKey = &kWolvicContentsUserDataKey;

// Used to access WolvicContents from WebContents. It's stored as
// user data in WebContents. Holds a raw pointer to break the reference
// (unique_ptr) cycle.
class WolvicContentsUserData : public base::SupportsUserData::Data {
 public:
  explicit WolvicContentsUserData(WolvicContents* ptr) : contents_(ptr) {}

  static WolvicContents* GetContents(WebContents* web_contents) {
    if (!web_contents)
      return NULL;
    WolvicContentsUserData* data = static_cast<WolvicContentsUserData*>(
        web_contents->GetUserData(kWolvicContentsUserDataKey));
    return data ? data->contents_.get() : NULL;
  }

 private:
  std::unique_ptr<WolvicContents> contents_;
};

WolvicContents::WolvicContents(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents) {
}

void
WolvicContents::Init() {
  web_contents_->SetUserData(kWolvicContentsUserDataKey, std::make_unique<WolvicContentsUserData>(this));
}

WolvicContents::~WolvicContents() {
  WebContentsObserver::Observe(nullptr);
}

void
WolvicContents::DidFinishNavigation(content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!navigation_handle->HasCommitted())
    return;

  // Only record a visit if the navigation affects user-facing session history
  // (i.e. it occurs in the primary frame tree).
  if (navigation_handle->IsInPrimaryMainFrame() ||
      (navigation_handle->GetParentFrame() &&
       navigation_handle->GetParentFrame()->GetPage().IsPrimary() &&
       navigation_handle->HasSubframeNavigationEntryCommitted())) {
    content::WolvicBrowserContext::FromWebContents(*web_contents())
        ->AddVisitedURLs(navigation_handle->GetRedirectChain());
  }
}

} // namespace wolvic
