// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_DELEGATE_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_DELEGATE_H_

#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "weblayer/browser/browser_context_impl.h"

namespace weblayer {

class ClientSideDetectionServiceDelegate
    : public safe_browsing::ClientSideDetectionService::Delegate {
 public:
  explicit ClientSideDetectionServiceDelegate(
      BrowserContextImpl* browser_context);
  ~ClientSideDetectionServiceDelegate() override;

  // ClientSideDetectionService::Delegate implementation.
  PrefService* GetPrefs() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  scoped_refptr<network::SharedURLLoaderFactory>
  GetSafeBrowsingURLLoaderFactory() override;
  safe_browsing::ChromeUserPopulation GetUserPopulation() override;

 private:
  BrowserContextImpl* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(ClientSideDetectionServiceDelegate);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_DELEGATE_H_
