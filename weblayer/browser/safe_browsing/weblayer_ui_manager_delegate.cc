// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/weblayer_ui_manager_delegate.h"

#include "components/safe_browsing/core/browser/ping_manager.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/i18n_util.h"
#include "weblayer/browser/no_state_prefetch/no_state_prefetch_utils.h"
#include "weblayer/browser/safe_browsing/weblayer_ping_manager_factory.h"
#include "weblayer/browser/weblayer_metrics_service_accessor.h"

namespace weblayer {

WebLayerSafeBrowsingUIManagerDelegate::WebLayerSafeBrowsingUIManagerDelegate() =
    default;
WebLayerSafeBrowsingUIManagerDelegate::
    ~WebLayerSafeBrowsingUIManagerDelegate() = default;

std::string WebLayerSafeBrowsingUIManagerDelegate::GetApplicationLocale() {
  return i18n::GetApplicationLocale();
}

void WebLayerSafeBrowsingUIManagerDelegate::
    TriggerSecurityInterstitialShownExtensionEventIfDesired(
        content::WebContents* web_contents,
        const GURL& page_url,
        const std::string& reason,
        int net_error_code) {}

void WebLayerSafeBrowsingUIManagerDelegate::
    TriggerSecurityInterstitialProceededExtensionEventIfDesired(
        content::WebContents* web_contents,
        const GURL& page_url,
        const std::string& reason,
        int net_error_code) {}

prerender::NoStatePrefetchContents*
WebLayerSafeBrowsingUIManagerDelegate::GetNoStatePrefetchContentsIfExists(
    content::WebContents* web_contents) {
  return NoStatePrefetchContentsFromWebContents(web_contents);
}

bool WebLayerSafeBrowsingUIManagerDelegate::IsHostingExtension(
    content::WebContents* web_contents) {
  return false;
}

PrefService* WebLayerSafeBrowsingUIManagerDelegate::GetPrefs(
    content::BrowserContext* browser_context) {
  return static_cast<BrowserContextImpl*>(browser_context)->pref_service();
}

history::HistoryService*
WebLayerSafeBrowsingUIManagerDelegate::GetHistoryService(
    content::BrowserContext* browser_context) {
  return nullptr;
}

safe_browsing::PingManager*
WebLayerSafeBrowsingUIManagerDelegate::GetPingManager(
    content::BrowserContext* browser_context) {
  return WebLayerPingManagerFactory::GetForBrowserContext(browser_context);
}

bool WebLayerSafeBrowsingUIManagerDelegate::
    IsMetricsAndCrashReportingEnabled() {
  return WebLayerMetricsServiceAccessor::IsMetricsReportingEnabled(
      BrowserProcess::GetInstance()->GetLocalState());
}

bool WebLayerSafeBrowsingUIManagerDelegate::IsSendingOfHitReportsEnabled() {
  // TODO(crbug.com/1232315): Determine whether to enable sending of hit reports
  // in WebLayer.
  return false;
}

}  // namespace weblayer
