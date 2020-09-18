// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/url_checker_delegate_impl.h"

#include "base/bind.h"
#include "components/prerender/browser/prerender_manager.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "weblayer/browser/no_state_prefetch/prerender_manager_factory.h"
#include "weblayer/browser/no_state_prefetch/prerender_utils.h"
#include "weblayer/browser/safe_browsing/safe_browsing_ui_manager.h"

namespace weblayer {

namespace {

// Destroys the prerender contents associated with the web_contents, if any.
void DestroyPrerenderContents(
    content::WebContents::OnceGetter web_contents_getter) {
  content::WebContents* web_contents = std::move(web_contents_getter).Run();

  auto* prerender_contents = PrerenderContentsFromWebContents(web_contents);
  if (prerender_contents)
    prerender_contents->Destroy(prerender::FINAL_STATUS_SAFE_BROWSING);
}

}  // namespace

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
    content::WebContents::OnceGetter web_contents_getter) {
  // Destroy the prefetch with FINAL_STATUS_SAFEBROSWING.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&DestroyPrerenderContents,
                                std::move(web_contents_getter)));
}

void UrlCheckerDelegateImpl::StartDisplayingBlockingPageHelper(
    const security_interstitials::UnsafeResource& resource,
    const std::string& method,
    const net::HttpRequestHeaders& headers,
    bool is_main_frame,
    bool has_user_gesture) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &UrlCheckerDelegateImpl::StartDisplayingDefaultBlockingPage,
          base::Unretained(this), resource));
}

void UrlCheckerDelegateImpl::
    StartObservingInteractionsForDelayedBlockingPageHelper(
        const security_interstitials::UnsafeResource& resource,
        bool is_main_frame) {
  NOTREACHED() << "Delayed warnings aren't implemented for WebLayer";
}

void UrlCheckerDelegateImpl::StartDisplayingDefaultBlockingPage(
    const security_interstitials::UnsafeResource& resource) {
  content::WebContents* web_contents = resource.web_contents_getter.Run();
  if (web_contents) {
    GetUIManager()->DisplayBlockingPage(resource);
    return;
  }

  // Report back that it is not ok to proceed with loading the URL.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(resource.callback, false /* proceed */,
                                false /* showed_interstitial */));
}

bool UrlCheckerDelegateImpl::IsUrlAllowlisted(const GURL& url) {
  // TODO(timvolodine): false for now, we may want allowlisting support later.
  return false;
}

bool UrlCheckerDelegateImpl::ShouldSkipRequestCheck(
    const GURL& original_url,
    int frame_tree_node_id,
    int render_process_id,
    int render_frame_id,
    bool originated_from_service_worker) {
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
