// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_INSECURE_FORM_CONTROLLER_CLIENT_H_
#define WEBLAYER_BROWSER_INSECURE_FORM_CONTROLLER_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"

namespace content {
class WebContents;
}

namespace weblayer {

// A stripped-down version of the class by the same name in
// //chrome/browser/ssl, which provides basic functionality for interacting with
// the insecure form interstitial.
class InsecureFormControllerClient
    : public security_interstitials::SecurityInterstitialControllerClient {
 public:
  static std::unique_ptr<security_interstitials::MetricsHelper>
  GetMetricsHelper(const GURL& url);
  static std::unique_ptr<security_interstitials::SettingsPageHelper>
  GetSettingsPageHelper();

  InsecureFormControllerClient(content::WebContents* web_contents,
                               const GURL& form_target_url);
  InsecureFormControllerClient(const InsecureFormControllerClient&) = delete;
  InsecureFormControllerClient& operator=(const InsecureFormControllerClient&) =
      delete;
  ~InsecureFormControllerClient() override;

  // security_interstitials::SecurityInterstitialControllerClient:
  void GoBack() override;
  void Proceed() override;

 private:
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_INSECURE_FORM_CONTROLLER_CLIENT_H_
