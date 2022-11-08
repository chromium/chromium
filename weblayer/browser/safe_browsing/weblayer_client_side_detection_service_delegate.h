// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_CLIENT_SIDE_DETECTION_SERVICE_DELEGATE_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_CLIENT_SIDE_DETECTION_SERVICE_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "weblayer/browser/browser_context_impl.h"

namespace weblayer {

class WebLayerClientSideDetectionServiceDelegate
    : public safe_browsing::ClientSideDetectionService::Delegate {
 public:
  explicit WebLayerClientSideDetectionServiceDelegate(
      BrowserContextImpl* browser_context);

  WebLayerClientSideDetectionServiceDelegate(
      const WebLayerClientSideDetectionServiceDelegate&) = delete;
  WebLayerClientSideDetectionServiceDelegate& operator=(
      const WebLayerClientSideDetectionServiceDelegate&) = delete;

  ~WebLayerClientSideDetectionServiceDelegate() override;

  // ClientSideDetectionService::Delegate implementation.
  PrefService* GetPrefs() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  scoped_refptr<network::SharedURLLoaderFactory>
  GetSafeBrowsingURLLoaderFactory() override;

 private:
  raw_ptr<BrowserContextImpl> browser_context_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_CLIENT_SIDE_DETECTION_SERVICE_DELEGATE_H_
