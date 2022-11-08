// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/weblayer_client_side_detection_service_delegate.h"

#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/browser/storage_partition.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/safe_browsing/safe_browsing_service.h"

namespace weblayer {

WebLayerClientSideDetectionServiceDelegate::
    WebLayerClientSideDetectionServiceDelegate(
        BrowserContextImpl* browser_context)
    : browser_context_(browser_context) {}

WebLayerClientSideDetectionServiceDelegate::
    ~WebLayerClientSideDetectionServiceDelegate() = default;

PrefService* WebLayerClientSideDetectionServiceDelegate::GetPrefs() {
  DCHECK(browser_context_);
  return browser_context_->pref_service();
}

scoped_refptr<network::SharedURLLoaderFactory>
WebLayerClientSideDetectionServiceDelegate::GetURLLoaderFactory() {
  return browser_context_->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

scoped_refptr<network::SharedURLLoaderFactory>
WebLayerClientSideDetectionServiceDelegate::GetSafeBrowsingURLLoaderFactory() {
  SafeBrowsingService* sb_service =
      BrowserProcess::GetInstance()->GetSafeBrowsingService();
  return sb_service->GetURLLoaderFactory();
}

}  // namespace weblayer
