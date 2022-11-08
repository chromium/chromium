// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_CLIENT_SIDE_DETECTION_HOST_DELEGATE_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_CLIENT_SIDE_DETECTION_HOST_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/safe_browsing/content/browser/client_side_detection_host.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "url/gurl.h"

namespace content {
struct GlobalRenderFrameHostId;
}  // namespace content

namespace weblayer {

class WebLayerClientSideDetectionHostDelegate
    : public safe_browsing::ClientSideDetectionHost::Delegate {
 public:
  explicit WebLayerClientSideDetectionHostDelegate(
      content::WebContents* web_contents);

  WebLayerClientSideDetectionHostDelegate(
      const WebLayerClientSideDetectionHostDelegate&) = delete;
  WebLayerClientSideDetectionHostDelegate& operator=(
      const WebLayerClientSideDetectionHostDelegate&) = delete;

  ~WebLayerClientSideDetectionHostDelegate() override;

  // ClientSideDetectionHost::Delegate implementation.
  bool HasSafeBrowsingUserInteractionObserver() override;
  PrefService* GetPrefs() override;
  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
  GetSafeBrowsingDBManager() override;
  scoped_refptr<safe_browsing::BaseUIManager> GetSafeBrowsingUIManager()
      override;
  base::WeakPtr<safe_browsing::ClientSideDetectionService>
  GetClientSideDetectionService() override;
  void AddReferrerChain(safe_browsing::ClientPhishingRequest* verdict,
                        GURL current_url,
                        const content::GlobalRenderFrameHostId&
                            current_outermost_main_frame_id) override;
  raw_ptr<safe_browsing::VerdictCacheManager> GetCacheManager() override;
  safe_browsing::ChromeUserPopulation GetUserPopulation() override;

 private:
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_CLIENT_SIDE_DETECTION_HOST_DELEGATE_H_
