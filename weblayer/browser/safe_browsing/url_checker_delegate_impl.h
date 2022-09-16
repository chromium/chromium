// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_URL_CHECKER_DELEGATE_IMPL_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_URL_CHECKER_DELEGATE_IMPL_H_

#include "base/memory/ref_counted.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "content/public/browser/web_contents.h"

namespace security_interstitials {
struct UnsafeResource;
}

namespace safe_browsing {
class SafeBrowsingUIManager;
}

namespace weblayer {

class UrlCheckerDelegateImpl : public safe_browsing::UrlCheckerDelegate {
 public:
  UrlCheckerDelegateImpl(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      scoped_refptr<safe_browsing::SafeBrowsingUIManager> ui_manager);

  UrlCheckerDelegateImpl(const UrlCheckerDelegateImpl&) = delete;
  UrlCheckerDelegateImpl& operator=(const UrlCheckerDelegateImpl&) = delete;

  void SetSafeBrowsingDisabled(bool disabled);

 private:
  ~UrlCheckerDelegateImpl() override;

  // Implementation of UrlCheckerDelegate:
  void MaybeDestroyNoStatePrefetchContents(
      content::WebContents::OnceGetter web_contents_getter) override;
  void StartDisplayingBlockingPageHelper(
      const security_interstitials::UnsafeResource& resource,
      const std::string& method,
      const net::HttpRequestHeaders& headers,
      bool is_main_frame,
      bool has_user_gesture) override;
  void StartObservingInteractionsForDelayedBlockingPageHelper(
      const security_interstitials::UnsafeResource& resource,
      bool is_main_frame) override;
  bool IsUrlAllowlisted(const GURL& url) override;
  void SetPolicyAllowlistDomains(
      const std::vector<std::string>& allowlist_domains) override;
  bool ShouldSkipRequestCheck(const GURL& original_url,
                              int frame_tree_node_id,
                              int render_process_id,
                              int render_frame_id,
                              bool originated_from_service_worker) override;
  void NotifySuspiciousSiteDetected(
      const base::RepeatingCallback<content::WebContents*()>&
          web_contents_getter) override;
  const safe_browsing::SBThreatTypeSet& GetThreatTypes() override;
  safe_browsing::SafeBrowsingDatabaseManager* GetDatabaseManager() override;
  safe_browsing::BaseUIManager* GetUIManager() override;

  void StartDisplayingDefaultBlockingPage(
      const security_interstitials::UnsafeResource& resource);

  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<safe_browsing::SafeBrowsingUIManager> ui_manager_;
  safe_browsing::SBThreatTypeSet threat_types_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_URL_CHECKER_DELEGATE_IMPL_H_
