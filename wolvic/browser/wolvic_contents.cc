// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/wolvic_contents.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

#include "wolvic/wolvic_browser_context.h"
#include "wolvic/wolvic_content_browser_client.h"
#include "wolvic/browser/wolvic_web_contents_delegate.h"

using content::WebContents;

namespace wolvic {

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
  raw_ptr<WolvicContents> contents_;
};

// static
WolvicContents* WolvicContents::FromWebContents(WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return WolvicContentsUserData::GetContents(web_contents);
}

WolvicContents::WolvicContents(
    std::unique_ptr<content::WebContents> web_contents)
    : content::WebContentsObserver(web_contents.get()),
      web_contents_(std::move(web_contents)) {}

void
WolvicContents::Init() {
  web_contents_->SetUserData(kWolvicContentsUserDataKey, std::make_unique<WolvicContentsUserData>(this));
}

WolvicContents::~WolvicContents() {
  web_contents_->RemoveUserData(kWolvicContentsUserDataKey);
  WebContentsObserver::Observe(nullptr);
}

void
WolvicContents::DidFinishNavigation(content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!navigation_handle->HasCommitted())
    return;

  auto* browser_context =
      WolvicBrowserContext::FromWebContents(*web_contents());

  // Do not record visited links in incognito mode.
  if (browser_context->IsOffTheRecord())
    return;

  // Only record a visit if the navigation affects user-facing session history
  // (i.e. it occurs in the primary frame tree).
  if (navigation_handle->IsInPrimaryMainFrame() ||
      (navigation_handle->GetParentFrame() &&
       navigation_handle->GetParentFrame()->GetPage().IsPrimary() &&
       navigation_handle->HasSubframeNavigationEntryCommitted())) {
    browser_context->AddVisitedURLs(navigation_handle->GetRedirectChain());
  }
}

void
WolvicContents::SetDelegate(std::unique_ptr<WolvicWebContentsDelegate> delegate) {
  web_contents_delegate_ = std::move(delegate);
  web_contents_->SetDelegate(web_contents_delegate_.get());
}

} // namespace wolvic
