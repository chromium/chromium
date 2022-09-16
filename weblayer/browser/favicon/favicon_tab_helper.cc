// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/favicon/favicon_tab_helper.h"

#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "weblayer/browser/favicon/favicon_service_impl.h"
#include "weblayer/browser/favicon/favicon_service_impl_factory.h"
#include "weblayer/public/favicon_fetcher_delegate.h"

namespace weblayer {
namespace {

bool IsSquareImage(const gfx::Image& image) {
  return !image.IsEmpty() && image.Width() == image.Height();
}

// Returns true if |image_a| is better than |image_b|. A value of false means
// |image_a| is not better than |image_b|. Either image may be empty, if both
// are empty false is returned.
bool IsImageBetterThan(const gfx::Image& image_a, const gfx::Image& image_b) {
  // Any image is better than an empty image.
  if (!image_a.IsEmpty() && image_b.IsEmpty())
    return true;

  // Prefer square favicons as they will scale much better.
  if (IsSquareImage(image_a) && !IsSquareImage(image_b))
    return true;

  return image_a.Width() > image_b.Width();
}

}  // namespace

FaviconTabHelper::ObserverSubscription::ObserverSubscription(
    FaviconTabHelper* helper,
    FaviconFetcherDelegate* delegate)
    : helper_(helper), delegate_(delegate) {
  helper_->AddDelegate(delegate_);
}

FaviconTabHelper::ObserverSubscription::~ObserverSubscription() {
  helper_->RemoveDelegate(delegate_);
}

FaviconTabHelper::~FaviconTabHelper() {
  // All of the ObserverSubscriptions should have been destroyed before this.
  DCHECK_EQ(0, observer_count_);
}

std::unique_ptr<FaviconTabHelper::ObserverSubscription>
FaviconTabHelper::RegisterFaviconFetcherDelegate(
    FaviconFetcherDelegate* delegate) {
  // WrapUnique as constructor is private.
  return base::WrapUnique(new ObserverSubscription(this, delegate));
}

FaviconTabHelper::FaviconTabHelper(content::WebContents* contents)
    : content::WebContentsUserData<FaviconTabHelper>(*contents),
      WebContentsObserver(contents) {}

void FaviconTabHelper::AddDelegate(FaviconFetcherDelegate* delegate) {
  delegates_.AddObserver(delegate);
  if (++observer_count_ == 1) {
    FaviconServiceImpl* favicon_service =
        FaviconServiceImplFactory::GetForBrowserContext(
            web_contents()->GetBrowserContext());
    favicon::ContentFaviconDriver::CreateForWebContents(web_contents(),
                                                        favicon_service);
    favicon::ContentFaviconDriver::FromWebContents(web_contents())
        ->AddObserver(this);
  }
}

void FaviconTabHelper::RemoveDelegate(FaviconFetcherDelegate* delegate) {
  delegates_.RemoveObserver(delegate);
  --observer_count_;
  DCHECK_GE(observer_count_, 0);
  if (observer_count_ == 0) {
    favicon::ContentFaviconDriver::FromWebContents(web_contents())
        ->RemoveObserver(this);
    // ContentFaviconDriver downloads images, if there are no observers there
    // is no need to keep it around. This triggers deleting it.
    web_contents()->SetUserData(favicon::ContentFaviconDriver::UserDataKey(),
                                nullptr);
    favicon_ = gfx::Image();
  }
}

void FaviconTabHelper::OnFaviconUpdated(
    favicon::FaviconDriver* favicon_driver,
    NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  if (!IsImageBetterThan(image, favicon_))
    return;

  favicon_ = image;
  for (FaviconFetcherDelegate& delegate : delegates_)
    delegate.OnFaviconChanged(favicon_);
}

void FaviconTabHelper::PrimaryPageChanged(content::Page& page) {
  if (page.GetMainDocument().IsErrorDocument() || favicon_.IsEmpty()) {
    return;
  }

  favicon_ = gfx::Image();
  for (FaviconFetcherDelegate& delegate : delegates_)
    delegate.OnFaviconChanged(favicon_);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FaviconTabHelper);

}  // namespace weblayer
