// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEBLAYER_SECURITY_BLOCKING_PAGE_FACTORY_H_
#define WEBLAYER_BROWSER_WEBLAYER_SECURITY_BLOCKING_PAGE_FACTORY_H_

#include "build/build_config.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/security_interstitials/content/bad_clock_blocking_page.h"
#include "components/security_interstitials/content/blocked_interception_blocking_page.h"
#include "components/security_interstitials/content/captive_portal_blocking_page.h"
#include "components/security_interstitials/content/https_only_mode_blocking_page.h"
#include "components/security_interstitials/content/insecure_form_blocking_page.h"
#include "components/security_interstitials/content/mitm_software_blocking_page.h"
#include "components/security_interstitials/content/security_blocking_page_factory.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "components/security_interstitials/content/ssl_blocking_page_base.h"

namespace weblayer {

// //weblayer's implementation of the SecurityBlockingPageFactory interface.
class WebLayerSecurityBlockingPageFactory : public SecurityBlockingPageFactory {
 public:
  WebLayerSecurityBlockingPageFactory() = default;
  ~WebLayerSecurityBlockingPageFactory() override = default;
  WebLayerSecurityBlockingPageFactory(
      const WebLayerSecurityBlockingPageFactory&) = delete;
  WebLayerSecurityBlockingPageFactory& operator=(
      const WebLayerSecurityBlockingPageFactory&) = delete;

  // SecurityBlockingPageFactory:
  std::unique_ptr<SSLBlockingPage> CreateSSLPage(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      int options_mask,
      const base::Time& time_triggered,
      const GURL& support_url,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter) override;
  std::unique_ptr<CaptivePortalBlockingPage> CreateCaptivePortalBlockingPage(
      content::WebContents* web_contents,
      const GURL& request_url,
      const GURL& login_url,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
      const net::SSLInfo& ssl_info,
      int cert_error) override;
  std::unique_ptr<BadClockBlockingPage> CreateBadClockBlockingPage(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      const base::Time& time_triggered,
      ssl_errors::ClockState clock_state,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter) override;
  std::unique_ptr<MITMSoftwareBlockingPage> CreateMITMSoftwareBlockingPage(
      content::WebContents* web_contents,
      int cert_error,
      const GURL& request_url,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
      const net::SSLInfo& ssl_info,
      const std::string& mitm_software_name) override;
  std::unique_ptr<BlockedInterceptionBlockingPage>
  CreateBlockedInterceptionBlockingPage(
      content::WebContents* web_contents,
      int cert_error,
      const GURL& request_url,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
      const net::SSLInfo& ssl_info) override;
  std::unique_ptr<security_interstitials::InsecureFormBlockingPage>
  CreateInsecureFormBlockingPage(content::WebContents* web_contents,
                                 const GURL& request_url) override;
  std::unique_ptr<security_interstitials::HttpsOnlyModeBlockingPage>
  CreateHttpsOnlyModeBlockingPage(content::WebContents* web_contents,
                                  const GURL& request_url) override;

#if BUILDFLAG(IS_ANDROID)
  // Returns the URL that will be navigated to when the user clicks on the
  // "Connect" button of the captive portal interstitial. Used by tests to
  // verify this flow.
  static GURL GetCaptivePortalLoginPageUrlForTesting();
#endif
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEBLAYER_SECURITY_BLOCKING_PAGE_FACTORY_H_
