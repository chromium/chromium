// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/client_side_detection_service_delegate.h"

#include "components/safe_browsing/core/common/utils.h"
#include "content/public/browser/storage_partition.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/safe_browsing/safe_browsing_service.h"

namespace weblayer {

ClientSideDetectionServiceDelegate::ClientSideDetectionServiceDelegate(
    BrowserContextImpl* browser_context)
    : browser_context_(browser_context) {}

ClientSideDetectionServiceDelegate::~ClientSideDetectionServiceDelegate() =
    default;

PrefService* ClientSideDetectionServiceDelegate::GetPrefs() {
  DCHECK(browser_context_);
  return browser_context_->pref_service();
}

scoped_refptr<network::SharedURLLoaderFactory>
ClientSideDetectionServiceDelegate::GetURLLoaderFactory() {
  return content::BrowserContext::GetDefaultStoragePartition(browser_context_)
      ->GetURLLoaderFactoryForBrowserProcess();
}

scoped_refptr<network::SharedURLLoaderFactory>
ClientSideDetectionServiceDelegate::GetSafeBrowsingURLLoaderFactory() {
  SafeBrowsingService* sb_service =
      BrowserProcess::GetInstance()->GetSafeBrowsingService();
  return sb_service ? sb_service->GetURLLoaderFactory() : nullptr;
}

safe_browsing::ChromeUserPopulation::ProfileManagementStatus
ClientSideDetectionServiceDelegate::GetManagementStatus() {
  // corresponds to unmanaged "unavailable" status on android
  return safe_browsing::GetProfileManagementStatus(nullptr);
}

}  // namespace weblayer
