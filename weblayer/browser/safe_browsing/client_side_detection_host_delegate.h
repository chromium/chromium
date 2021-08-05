// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_HOST_DELEGATE_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_HOST_DELEGATE_H_

#include "components/safe_browsing/content/browser/client_side_detection_host.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "url/gurl.h"

namespace weblayer {

class ClientSideDetectionHostDelegate
    : public safe_browsing::ClientSideDetectionHost::Delegate {
 public:
  explicit ClientSideDetectionHostDelegate(content::WebContents* web_contents);
  ~ClientSideDetectionHostDelegate() override;

  // ClientSideDetectionHost::Delegate implementation.
  bool HasSafeBrowsingUserInteractionObserver() override;
  PrefService* GetPrefs() override;
  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
  GetSafeBrowsingDBManager() override;
  scoped_refptr<safe_browsing::BaseUIManager> GetSafeBrowsingUIManager()
      override;
  safe_browsing::ClientSideDetectionService* GetClientSideDetectionService()
      override;
  void AddReferrerChain(safe_browsing::ClientPhishingRequest* verdict,
                        GURL current_url) override;

 private:
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(ClientSideDetectionHostDelegate);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_HOST_DELEGATE_H_
