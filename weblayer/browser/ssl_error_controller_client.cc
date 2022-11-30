// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/ssl_error_controller_client.h"

#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/content/utils.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/web_contents.h"
#include "weblayer/browser/i18n_util.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/error_page_delegate.h"

namespace weblayer {

SSLErrorControllerClient::SSLErrorControllerClient(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper,
    std::unique_ptr<security_interstitials::SettingsPageHelper>
        settings_page_helper)
    : security_interstitials::SecurityInterstitialControllerClient(
          web_contents,
          std::move(metrics_helper),
          nullptr /*prefs*/,
          i18n::GetApplicationLocale(),
          GURL("about:blank") /*default_safe_page*/,
          std::move(settings_page_helper)),
      cert_error_(cert_error),
      ssl_info_(ssl_info),
      request_url_(request_url) {}

void SSLErrorControllerClient::GoBack() {
  ErrorPageDelegate* delegate =
      TabImpl::FromWebContents(web_contents_)->error_page_delegate();
  if (delegate && delegate->OnBackToSafety())
    return;

  SecurityInterstitialControllerClient::GoBackAfterNavigationCommitted();
}

void SSLErrorControllerClient::Proceed() {
  web_contents_->GetBrowserContext()->GetSSLHostStateDelegate()->AllowCert(
      request_url_.host(), *ssl_info_.cert.get(), cert_error_,
      web_contents_->GetPrimaryMainFrame()->GetStoragePartition());
  Reload();
}

void SSLErrorControllerClient::OpenUrlInNewForegroundTab(const GURL& url) {
  // For now WebLayer doesn't support multiple tabs, so just open the Learn
  // More link in the current tab.
  OpenUrlInCurrentTab(url);
}

bool SSLErrorControllerClient::CanLaunchDateAndTimeSettings() {
  return true;
}

void SSLErrorControllerClient::LaunchDateAndTimeSettings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&security_interstitials::LaunchDateAndTimeSettings));
}

}  // namespace weblayer
