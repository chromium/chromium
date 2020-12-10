// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/safe_browsing_ui_manager.h"

#include "components/safe_browsing/core/ping_manager.h"
#include "content/public/browser/browser_thread.h"
#include "weblayer/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "weblayer/browser/safe_browsing/safe_browsing_service.h"
#include "weblayer/browser/safe_browsing/safe_browsing_subresource_helper.h"

using content::BrowserThread;

namespace {

std::string GetProtocolConfigClientName() {
  // Return a weblayer specific client name.
  return "weblayer";
}

}  // namespace

namespace weblayer {

SafeBrowsingUIManager::SafeBrowsingUIManager(
    SafeBrowsingService* safe_browsing_service)
    : safe_browsing_service_(safe_browsing_service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

SafeBrowsingUIManager::~SafeBrowsingUIManager() = default;

void SafeBrowsingUIManager::SendSerializedThreatDetails(
    content::BrowserContext* browser_context,
    const std::string& serialized) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!ping_manager_) {
    ping_manager_ =
        ::safe_browsing::PingManager::Create(safe_browsing::GetV4ProtocolConfig(
            GetProtocolConfigClientName(), false /* auto_update */));
  }

  if (serialized.empty())
    return;

  DVLOG(1) << "Sending serialized threat details";
  ping_manager_->ReportThreatDetails(
      safe_browsing_service_->GetURLLoaderFactory(), serialized);
}

safe_browsing::BaseBlockingPage*
SafeBrowsingUIManager::CreateBlockingPageForSubresource(
    content::WebContents* contents,
    const GURL& blocked_url,
    const UnsafeResource& unsafe_resource) {
  SafeBrowsingSubresourceHelper::CreateForWebContents(contents, this);
  SafeBrowsingBlockingPage* blocking_page =
      SafeBrowsingBlockingPage::CreateBlockingPage(this, contents, blocked_url,
                                                   unsafe_resource);
  return blocking_page;
}

}  // namespace weblayer
