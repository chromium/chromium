// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/net_http/http_service_impl.h"

#include "net/url_request/url_request_context_builder.h"
#include "webrunner/net_http/url_loader_impl.h"

namespace net_http {

HttpServiceImpl::HttpServiceImpl() {
  // TODO: Set the right options in the URLRequestContextBuilder.
}

HttpServiceImpl::~HttpServiceImpl() = default;

void HttpServiceImpl::CreateURLLoader(
    fidl::InterfaceRequest<fuchsia::net::oldhttp::URLLoader> request) {
  // The URLLoaderImpl objects lifespan is tied to their binding, which is set
  // in their constructor.
  net::URLRequestContextBuilder builder;
  new URLLoaderImpl(builder.Build(), std::move(request));
}

}  // namespace net_http
