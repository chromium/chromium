// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SSL_ERROR_HANDLER_H_
#define WEBLAYER_BROWSER_SSL_ERROR_HANDLER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

class SSLCertReporter;

namespace weblayer {

using BlockingPageReadyCallback = base::OnceCallback<void(
    std::unique_ptr<security_interstitials::SecurityInterstitialPage>)>;

// This code is responsible for deciding what type of interstitial to display
// for an SSL validation error and actually displaying it. It is a greatly
// simplified version of //chrome's SSLErrorHandler; in the long run that class
// should be componentized and WebLayer should replace its usage of this
// simplified version with usage of that class.

// Entrypoint for handling SSL errors. All parameters except
// |blocking_page_ready_callback| are the same as SSLBlockingPage constructor.
// This function creates an interstitial and passes it to
// |blocking_page_ready_callback|.
// |blocking_page_ready_callback| is guaranteed not to be called
// synchronously.
void HandleSSLError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    const base::Callback<void(content::CertificateRequestResultType)>&
        decision_callback,
    BlockingPageReadyCallback blocking_page_ready_callback);

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SSL_ERROR_HANDLER_H_
