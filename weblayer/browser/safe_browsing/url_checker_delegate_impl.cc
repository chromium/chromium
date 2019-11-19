// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/url_checker_delegate_impl.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "components/safe_browsing/db/database_manager.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "weblayer/browser/safe_browsing/safe_browsing_ui_manager.h"

namespace weblayer {

UrlCheckerDelegateImpl::UrlCheckerDelegateImpl(
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<SafeBrowsingUIManager> ui_manager)
    : database_manager_(std::move(database_manager)),
      ui_manager_(std::move(ui_manager)),
      threat_types_(safe_browsing::CreateSBThreatTypeSet(
          {safe_browsing::SB_THREAT_TYPE_URL_MALWARE,
           safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
           safe_browsing::SB_THREAT_TYPE_URL_UNWANTED,
           safe_browsing::SB_THREAT_TYPE_BILLING})) {}

UrlCheckerDelegateImpl::~UrlCheckerDelegateImpl() = default;

void UrlCheckerDelegateImpl::MaybeDestroyPrerenderContents(
    const base::Callback<content::WebContents*()>& web_contents_getter) {}

void UrlCheckerDelegateImpl::StartDisplayingBlockingPageHelper(
    const security_interstitials::UnsafeResource& resource,
    const std::string& method,
    const net::HttpRequestHeaders& headers,
    bool is_main_frame,
    bool has_user_gesture) {
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &UrlCheckerDelegateImpl::StartDisplayingDefaultBlockingPage,
          base::Unretained(this), resource));
}

void UrlCheckerDelegateImpl::StartDisplayingDefaultBlockingPage(
    const security_interstitials::UnsafeResource& resource) {
  content::WebContents* web_contents = resource.web_contents_getter.Run();
  if (web_contents) {
    GetUIManager()->DisplayBlockingPage(resource);
    return;
  }

  // Report back that it is not ok to proceed with loading the URL.
  base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                 base::BindOnce(resource.callback, false));
}

bool UrlCheckerDelegateImpl::IsUrlWhitelisted(const GURL& url) {
  // TODO(timvolodine): false for now, we may want whitelisting support later.
  return false;
}

bool UrlCheckerDelegateImpl::ShouldSkipRequestCheck(
    content::ResourceContext* resource_context,
    const GURL& original_url,
    int frame_tree_node_id,
    int render_process_id,
    int render_frame_id,
    bool originated_from_service_worker) {
  // TODO(timvolodine): this is needed when safebrowsing is not enabled.
  // For now in the context of weblayer we consider safebrowsing as always
  // enabled. This may change in the future.
  return false;
}

void UrlCheckerDelegateImpl::NotifySuspiciousSiteDetected(
    const base::RepeatingCallback<content::WebContents*()>&
        web_contents_getter) {}

const safe_browsing::SBThreatTypeSet& UrlCheckerDelegateImpl::GetThreatTypes() {
  return threat_types_;
}

safe_browsing::SafeBrowsingDatabaseManager*
UrlCheckerDelegateImpl::GetDatabaseManager() {
  return database_manager_.get();
}

safe_browsing::BaseUIManager* UrlCheckerDelegateImpl::GetUIManager() {
  return ui_manager_.get();
}

}  // namespace weblayer
