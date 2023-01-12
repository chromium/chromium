// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/weblayer_safe_browsing_tab_observer_delegate.h"

#include "base/functional/bind.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/client_side_detection_host.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "content/public/browser/web_contents.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/safe_browsing/client_side_detection_service_factory.h"
#include "weblayer/browser/safe_browsing/safe_browsing_service.h"
#include "weblayer/browser/safe_browsing/safe_browsing_token_fetcher_impl.h"
#include "weblayer/browser/safe_browsing/weblayer_client_side_detection_host_delegate.h"

namespace weblayer {

namespace {
std::unique_ptr<safe_browsing::ClientSideDetectionHost>
CreateClientSideDetectionHost(content::WebContents* web_contents,
                              PrefService* prefs,
                              BrowserContextImpl* browser_context) {
  return safe_browsing::ClientSideDetectionHost::Create(
      web_contents,
      std::make_unique<WebLayerClientSideDetectionHostDelegate>(web_contents),
      prefs,
      std::make_unique<SafeBrowsingTokenFetcherImpl>(base::BindRepeating(
          &ProfileImpl::access_token_fetch_delegate,
          base::Unretained(ProfileImpl::FromBrowserContext(browser_context)))),
      static_cast<BrowserContextImpl*>(browser_context)->IsOffTheRecord(),
      // TODO(crbug.com/1171215): Change this to production mechanism for
      // enabling Gaia-keyed CSD once that mechanism is determined. See also
      // crbug.com/1190615.
      /* account_signed_in_callback= */ base::BindRepeating([]() {
        return false;
      }));
}

}  // namespace

WebLayerSafeBrowsingTabObserverDelegate::
    WebLayerSafeBrowsingTabObserverDelegate() = default;
WebLayerSafeBrowsingTabObserverDelegate::
    ~WebLayerSafeBrowsingTabObserverDelegate() = default;

PrefService* WebLayerSafeBrowsingTabObserverDelegate::GetPrefs(
    content::BrowserContext* browser_context) {
  return static_cast<BrowserContextImpl*>(browser_context)->pref_service();
}

safe_browsing::ClientSideDetectionService*
WebLayerSafeBrowsingTabObserverDelegate::GetClientSideDetectionServiceIfExists(
    content::BrowserContext* browser_context) {
  return ClientSideDetectionServiceFactory::GetForBrowserContext(
      browser_context);
}

bool WebLayerSafeBrowsingTabObserverDelegate::DoesSafeBrowsingServiceExist() {
  return BrowserProcess::GetInstance()->GetSafeBrowsingService();
}

std::unique_ptr<safe_browsing::ClientSideDetectionHost>
WebLayerSafeBrowsingTabObserverDelegate::CreateClientSideDetectionHost(
    content::WebContents* web_contents) {
  BrowserContextImpl* browser_context =
      static_cast<BrowserContextImpl*>(web_contents->GetBrowserContext());
  return ::weblayer::CreateClientSideDetectionHost(
      web_contents, GetPrefs(browser_context), browser_context);
}

}  // namespace weblayer
