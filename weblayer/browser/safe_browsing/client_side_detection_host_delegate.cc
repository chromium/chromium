// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/client_side_detection_host_delegate.h"

#include "components/prefs/pref_service.h"
#include "components/safe_browsing/android/remote_database_manager.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/safe_browsing/client_side_detection_service_factory.h"
#include "weblayer/browser/safe_browsing/safe_browsing_service.h"

namespace weblayer {

ClientSideDetectionHostDelegate::ClientSideDetectionHostDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

ClientSideDetectionHostDelegate::~ClientSideDetectionHostDelegate() = default;

bool ClientSideDetectionHostDelegate::HasSafeBrowsingUserInteractionObserver() {
  return false;
}

PrefService* ClientSideDetectionHostDelegate::GetPrefs() {
  BrowserContextImpl* browser_context_impl =
      static_cast<BrowserContextImpl*>(web_contents_->GetBrowserContext());
  DCHECK(browser_context_impl);
  return browser_context_impl->pref_service();
}

scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
ClientSideDetectionHostDelegate::GetSafeBrowsingDBManager() {
  SafeBrowsingService* sb_service =
      BrowserProcess::GetInstance()->GetSafeBrowsingService();
  return sb_service ? sb_service->GetSafeBrowsingDBManager() : nullptr;
}

scoped_refptr<safe_browsing::BaseUIManager>
ClientSideDetectionHostDelegate::GetSafeBrowsingUIManager() {
  SafeBrowsingService* sb_service =
      BrowserProcess::GetInstance()->GetSafeBrowsingService();
  return sb_service ? sb_service->GetSafeBrowsingUIManager() : nullptr;
}

safe_browsing::ClientSideDetectionService*
ClientSideDetectionHostDelegate::GetClientSideDetectionService() {
  return ClientSideDetectionServiceFactory::GetForBrowserContext(
      web_contents_->GetBrowserContext());
}

void ClientSideDetectionHostDelegate::AddReferrerChain(
    safe_browsing::ClientPhishingRequest* verdict,
    GURL current_url) {}

}  // namespace weblayer
