// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBRUNNER_BROWSER_WEBRUNNER_URL_REQUEST_CONTEXT_GETTER_H_
#define WEBRUNNER_BROWSER_WEBRUNNER_URL_REQUEST_CONTEXT_GETTER_H_

#include <memory>

#include "base/macros.h"
#include "content/public/browser/browser_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {
class NetLog;
class ProxyConfigService;
}  // namespace net

namespace webrunner {

class WebRunnerURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  WebRunnerURLRequestContextGetter(
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      net::NetLog* net_log,
      content::ProtocolHandlerMap protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors,
      base::FilePath data_dir_path);

  // net::URLRequestContextGetter overrides.
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

 protected:
  ~WebRunnerURLRequestContextGetter() override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  net::NetLog* net_log_;
  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;

  content::ProtocolHandlerMap protocol_handlers_;
  content::URLRequestInterceptorScopedVector request_interceptors_;
  base::FilePath data_dir_path_;

  DISALLOW_COPY_AND_ASSIGN(WebRunnerURLRequestContextGetter);
};

}  // namespace webrunner

#endif  // WEBRUNNER_BROWSER_WEBRUNNER_URL_REQUEST_CONTEXT_GETTER_H_
