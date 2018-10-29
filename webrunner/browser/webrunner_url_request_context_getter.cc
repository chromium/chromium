// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/browser/webrunner_url_request_context_getter.h"

#include <utility>

#include "base/single_thread_task_runner.h"
#include "content/public/browser/cookie_store_factory.h"
#include "net/cookies/cookie_store.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/ssl/channel_id_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"

namespace webrunner {

WebRunnerURLRequestContextGetter::WebRunnerURLRequestContextGetter(
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    net::NetLog* net_log,
    content::ProtocolHandlerMap protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors,
    base::FilePath data_dir_path)
    : network_task_runner_(std::move(network_task_runner)),
      net_log_(net_log),
      protocol_handlers_(std::move(protocol_handlers)),
      request_interceptors_(std::move(request_interceptors)),
      data_dir_path_(data_dir_path) {}

WebRunnerURLRequestContextGetter::~WebRunnerURLRequestContextGetter() = default;

net::URLRequestContext*
WebRunnerURLRequestContextGetter::GetURLRequestContext() {
  if (!url_request_context_) {
    net::URLRequestContextBuilder builder;
    builder.set_net_log(net_log_);
    builder.set_data_enabled(true);

    for (auto& protocol_handler : protocol_handlers_) {
      builder.SetProtocolHandler(protocol_handler.first,
                                 std::move(protocol_handler.second));
    }
    protocol_handlers_.clear();

    builder.SetInterceptors(std::move(request_interceptors_));

    if (data_dir_path_.empty()) {
      // Set up an in-memory (ephemeral) CookieStore.
      builder.SetCookieAndChannelIdStores(
          content::CreateCookieStore(content::CookieStoreConfig(), nullptr),
          nullptr);
    } else {
      // Set up a persistent CookieStore under |data_dir_path|.
      content::CookieStoreConfig cookie_config(
          data_dir_path_.Append(FILE_PATH_LITERAL("Cookies")), false, false,
          NULL);

      // Platform encryption support is not yet implemented, so store cookies in
      // plaintext for now.
      // TODO(crbug.com/884355): Add OSCrypt impl for Fuchsia and encrypt the
      // cookie store with it.
      NOTIMPLEMENTED() << "Persistent cookie store is NOT encrypted!";
      builder.SetCookieAndChannelIdStores(
          content::CreateCookieStore(cookie_config, nullptr), nullptr);
    }

    url_request_context_ = builder.Build();
  }
  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
WebRunnerURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

}  // namespace webrunner
