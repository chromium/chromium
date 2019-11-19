// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SSL_BLOCKING_PAGE_H_
#define WEBLAYER_BROWSER_SSL_BLOCKING_PAGE_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

namespace security_interstitials {
class MetricsHelper;
class SSLErrorUI;
}  // namespace security_interstitials

namespace weblayer {

// A stripped-down version of the class of the same name in
// //chrome/browser/ssl. TODO(estade,blundell): componentize and share instead
// of copying.
class SSLBlockingPage
    : public security_interstitials::SecurityInterstitialPage {
 public:
  // Interstitial type, used in tests.
  static const InterstitialPageDelegate::TypeID kTypeForTesting;

  ~SSLBlockingPage() override;

  // Creates an SSL blocking page. If the blocking page isn't shown, the caller
  // is responsible for cleaning up the blocking page, otherwise the
  // interstitial takes ownership when shown. |options_mask| must be a bitwise
  // mask of SSLErrorUI::SSLErrorOptionsMask values.
  // This is static because the constructor uses expensive to compute parameters
  // more than once (e.g. overrideable).
  static SSLBlockingPage* Create(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      int options_mask,
      const base::Time& time_triggered,
      const base::Callback<void(content::CertificateRequestResultType)>&
          callback);

  // InterstitialPageDelegate:
  InterstitialPageDelegate::TypeID GetTypeForTesting() override;

  // Returns true if |options_mask| refers to a soft-overridable SSL error and
  // if SSL error overriding is allowed by policy.
  static bool IsOverridable(int options_mask);

  // InterstitialPageDelegate.
  void CommandReceived(const std::string& command) override;
  void OverrideEntry(content::NavigationEntry* entry) override;
  void OnInterstitialClosing() override;

  // SecurityInterstitialPage:
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(
      base::DictionaryValue* load_time_data) override;

 protected:
  SSLBlockingPage(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      int options_mask,
      const base::Time& time_triggered,
      bool overrideable,
      std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper,
      const base::Callback<void(content::CertificateRequestResultType)>&
          callback);

 private:
  base::Callback<void(content::CertificateRequestResultType)> callback_;
  const net::SSLInfo ssl_info_;

  const std::unique_ptr<security_interstitials::SSLErrorUI> ssl_error_ui_;

  DISALLOW_COPY_AND_ASSIGN(SSLBlockingPage);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SSL_BLOCKING_PAGE_H_
