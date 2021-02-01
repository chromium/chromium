// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_UI_MANAGER_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_UI_MANAGER_H_

#include "components/safe_browsing/content/base_ui_manager.h"
#include "components/security_interstitials/core/unsafe_resource.h"

namespace content {
class WebContents;
}

namespace safe_browsing {
class BaseBlockingPage;
class PingManager;
}  // namespace safe_browsing

namespace weblayer {
class SafeBrowsingService;

class SafeBrowsingUIManager : public safe_browsing::BaseUIManager {
 public:
  // Construction needs to happen on the UI thread.
  SafeBrowsingUIManager(SafeBrowsingService* safe_browsing_service);

  // BaseUIManager overrides.

  // Called on the UI thread by the ThreatDetails with the serialized
  // protocol buffer, so the service can send it over.
  void SendSerializedThreatDetails(content::BrowserContext* browser_context,
                                   const std::string& serialized) override;

 protected:
  ~SafeBrowsingUIManager() override;

 private:
  safe_browsing::BaseBlockingPage* CreateBlockingPageForSubresource(
      content::WebContents* contents,
      const GURL& blocked_url,
      const UnsafeResource& unsafe_resource) override;

  // Provides phishing and malware statistics. Accessed on IO thread.
  std::unique_ptr<safe_browsing::PingManager> ping_manager_;

  SafeBrowsingService* safe_browsing_service_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingUIManager);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_UI_MANAGER_H_
