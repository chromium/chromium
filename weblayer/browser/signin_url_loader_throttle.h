// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SIGNIN_URL_LOADER_THROTTLE_H_
#define WEBLAYER_BROWSER_SIGNIN_URL_LOADER_THROTTLE_H_

#include "base/memory/raw_ptr.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_request_headers.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace net {
class HttpResponseHeaders;
}

namespace weblayer {

// Exposed for testing.
extern const char kSignOutPath[];

class SigninURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  ~SigninURLLoaderThrottle() override;

  static std::unique_ptr<SigninURLLoaderThrottle> Create(
      content::BrowserContext* browser_context,
      content::WebContents::Getter web_contents_getter);

  // blink::URLLoaderThrottle
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* headers_to_remove,
      net::HttpRequestHeaders* modified_headers,
      net::HttpRequestHeaders* modified_cors_exempt_request_headers) override;
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;

 private:
  SigninURLLoaderThrottle(content::BrowserContext* browser_context,
                          content::WebContents::Getter web_contents_getter);

  void ProcessRequest(const GURL& url,
                      net::HttpRequestHeaders* original_headers,
                      std::vector<std::string>* headers_to_remove,
                      net::HttpRequestHeaders* modified_headers);
  void ProcessResponse(const net::HttpResponseHeaders* headers);

  raw_ptr<content::BrowserContext> browser_context_;
  content::WebContents::Getter web_contents_getter_;
  net::HttpRequestHeaders request_headers_;
  GURL request_url_;
  bool is_main_frame_ = false;
  bool response_header_processed_ = false;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SIGNIN_URL_LOADER_THROTTLE_H_
