// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/safe_browsing_tab_observer.h"

#include "base/bind.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/client_side_detection_host.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "content/public/browser/web_contents.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/safe_browsing/client_side_detection_host_delegate.h"
#include "weblayer/browser/safe_browsing/client_side_detection_service_factory.h"
#include "weblayer/browser/safe_browsing/safe_browsing_token_fetcher_impl.h"

namespace weblayer {

namespace {
std::unique_ptr<safe_browsing::ClientSideDetectionHost>
CreateClientSideDetectionHost(content::WebContents* web_contents,
                              PrefService* prefs,
                              BrowserContextImpl* browser_context) {
  return safe_browsing::ClientSideDetectionHost::Create(
      web_contents,
      std::make_unique<ClientSideDetectionHostDelegate>(web_contents), prefs,
      std::make_unique<SafeBrowsingTokenFetcherImpl>(base::BindRepeating(
          &ProfileImpl::access_token_fetch_delegate,
          base::Unretained(ProfileImpl::FromBrowserContext(browser_context)))),
      static_cast<BrowserContextImpl*>(browser_context)->IsOffTheRecord(),
      /* account_signed_in_callback= */ base::BindRepeating([]() {
        return true;
      }));
}

}  // namespace

SafeBrowsingTabObserver::SafeBrowsingTabObserver(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  BrowserContextImpl* browser_context =
      static_cast<BrowserContextImpl*>(web_contents->GetBrowserContext());
  PrefService* prefs = browser_context->pref_service();
  if (prefs) {
    pref_change_registrar_.Init(prefs);
    pref_change_registrar_.Add(
        ::prefs::kSafeBrowsingEnabled,
        base::BindRepeating(
            &SafeBrowsingTabObserver::UpdateSafebrowsingDetectionHost,
            base::Unretained(this)));

    safe_browsing::ClientSideDetectionService* csd_service =
        ClientSideDetectionServiceFactory::GetForBrowserContext(
            browser_context);
    if (safe_browsing::IsSafeBrowsingEnabled(*prefs) &&
        BrowserProcess::GetInstance()->GetSafeBrowsingService() &&
        csd_service) {
      safebrowsing_detection_host_ =
          CreateClientSideDetectionHost(web_contents, prefs, browser_context);
      csd_service->AddClientSideDetectionHost(
          safebrowsing_detection_host_.get());
    }
  }
}

SafeBrowsingTabObserver::~SafeBrowsingTabObserver() {}

void SafeBrowsingTabObserver::UpdateSafebrowsingDetectionHost() {
  BrowserContextImpl* browser_context =
      static_cast<BrowserContextImpl*>(web_contents_->GetBrowserContext());
  PrefService* prefs = browser_context->pref_service();

  bool safe_browsing = safe_browsing::IsSafeBrowsingEnabled(*prefs);
  safe_browsing::ClientSideDetectionService* csd_service =
      ClientSideDetectionServiceFactory::GetForBrowserContext(browser_context);
  if (safe_browsing && csd_service) {
    if (!safebrowsing_detection_host_.get()) {
      safebrowsing_detection_host_ =
          CreateClientSideDetectionHost(web_contents_, prefs, browser_context);
      csd_service->AddClientSideDetectionHost(
          safebrowsing_detection_host_.get());
    }
  } else {
    safebrowsing_detection_host_.reset();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SafeBrowsingTabObserver)

}  // namespace weblayer