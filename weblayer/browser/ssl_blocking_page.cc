// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/ssl_blocking_page.h"

#include <memory>
#include <utility>

#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/ssl_error_options_mask.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "weblayer/browser/i18n_util.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/error_page_delegate.h"

namespace weblayer {

namespace {

// A stripped-down version of the class by the same name in
// //chrome/browser/ssl, which provides basic functionality for interacting with
// the SSL interstitial.
class SSLErrorControllerClient
    : public security_interstitials::SecurityInterstitialControllerClient {
 public:
  SSLErrorControllerClient(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper)
      : security_interstitials::SecurityInterstitialControllerClient(
            web_contents,
            std::move(metrics_helper),
            nullptr /*prefs*/,
            i18n::GetApplicationLocale(),
            GURL("about:blank") /*default_safe_page*/),
        cert_error_(cert_error),
        ssl_info_(ssl_info),
        request_url_(request_url) {}

  ~SSLErrorControllerClient() override = default;

  void GoBack() override {
    ErrorPageDelegate* delegate =
        TabImpl::FromWebContents(web_contents_)->error_page_delegate();
    if (delegate && delegate->OnBackToSafety())
      return;

    SecurityInterstitialControllerClient::GoBackAfterNavigationCommitted();
  }

  void Proceed() override {
    web_contents_->GetBrowserContext()->GetSSLHostStateDelegate()->AllowCert(
        request_url_.host(), *ssl_info_.cert.get(), cert_error_);
    Reload();
  }

  void OpenUrlInNewForegroundTab(const GURL& url) override {
    // For now WebLayer doesn't support multiple tabs, so just open the Learn
    // More link in the current tab.
    OpenUrlInCurrentTab(url);
  }

 private:
  const int cert_error_;
  const net::SSLInfo ssl_info_;
  const GURL request_url_;

  DISALLOW_COPY_AND_ASSIGN(SSLErrorControllerClient);
};

}  // namespace

// static
const content::InterstitialPageDelegate::TypeID
    SSLBlockingPage::kTypeForTesting = &SSLBlockingPage::kTypeForTesting;

// static
SSLBlockingPage* SSLBlockingPage::Create(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    int options_mask,
    const base::Time& time_triggered,
    const base::Callback<void(content::CertificateRequestResultType)>&
        callback) {
  bool overridable = IsOverridable(options_mask);

  security_interstitials::MetricsHelper::ReportDetails report_details;
  report_details.metric_prefix =
      overridable ? "ssl_overridable" : "ssl_nonoverridable";
  auto metrics_helper = std::make_unique<security_interstitials::MetricsHelper>(
      request_url, report_details, /*history_service=*/nullptr);

  return new SSLBlockingPage(web_contents, cert_error, ssl_info, request_url,
                             options_mask, time_triggered, overridable,
                             std::move(metrics_helper), callback);
}

bool SSLBlockingPage::ShouldCreateNewNavigation() const {
  return true;
}

content::InterstitialPageDelegate::TypeID SSLBlockingPage::GetTypeForTesting() {
  return SSLBlockingPage::kTypeForTesting;
}

SSLBlockingPage::~SSLBlockingPage() = default;

void SSLBlockingPage::PopulateInterstitialStrings(
    base::DictionaryValue* load_time_data) {
  ssl_error_ui_->PopulateStringsForHTML(load_time_data);
}

// Note that we always create a navigation entry with SSL errors.
// No error happening loading a sub-resource triggers an interstitial so far.
SSLBlockingPage::SSLBlockingPage(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    int options_mask,
    const base::Time& time_triggered,
    bool overridable,
    std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper,
    const base::Callback<void(content::CertificateRequestResultType)>& callback)
    : security_interstitials::SecurityInterstitialPage(
          web_contents,
          request_url,
          std::make_unique<SSLErrorControllerClient>(
              web_contents,
              cert_error,
              ssl_info,
              request_url,
              std::move(metrics_helper))),
      ssl_info_(ssl_info),
      ssl_error_ui_(std::make_unique<security_interstitials::SSLErrorUI>(
          request_url,
          cert_error,
          ssl_info,
          options_mask,
          time_triggered,
          /*support_url=*/GURL(),
          controller())) {
  DCHECK(callback.is_null());
}

void SSLBlockingPage::OverrideEntry(content::NavigationEntry* entry) {
  entry->GetSSL() = content::SSLStatus(ssl_info_);
}

// This handles the commands sent from the interstitial JavaScript.
void SSLBlockingPage::CommandReceived(const std::string& command) {
  // content::WaitForRenderFrameReady sends this message when the page
  // load completes. Ignore it.
  if (command == "\"pageLoadComplete\"")
    return;

  int cmd = 0;
  bool retval = base::StringToInt(command, &cmd);
  DCHECK(retval);

  ssl_error_ui_->HandleCommand(
      static_cast<security_interstitials::SecurityInterstitialCommand>(cmd));
}

void SSLBlockingPage::OnInterstitialClosing() {
  // TODO(blundell): Does this need to track metrics analogously to //chrome's
  // SSLBlockingPageBase::OnInterstitialClosing()?
}

// static
bool SSLBlockingPage::IsOverridable(int options_mask) {
  const bool is_overridable =
      (options_mask &
       security_interstitials::SSLErrorOptionsMask::SOFT_OVERRIDE_ENABLED) &&
      !(options_mask &
        security_interstitials::SSLErrorOptionsMask::STRICT_ENFORCEMENT) &&
      !(options_mask &
        security_interstitials::SSLErrorOptionsMask::HARD_OVERRIDE_DISABLED);
  return is_overridable;
}

}  // namespace weblayer
