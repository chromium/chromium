// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBRUNNER_NET_HTTP_HTTP_SERVICE_IMPL_H_
#define WEBRUNNER_NET_HTTP_HTTP_SERVICE_IMPL_H_

#include <fuchsia/net/oldhttp/cpp/fidl.h>
#include <lib/fidl/cpp/interface_request.h>

#include "net/url_request/url_request_context.h"

namespace net_http {

// Implements the Fuchsia HttpService API, using the //net library.
class HttpServiceImpl : public ::fuchsia::net::oldhttp::HttpService {
 public:
  HttpServiceImpl();
  ~HttpServiceImpl() override;

  // HttpService methods:
  void CreateURLLoader(
      fidl::InterfaceRequest<::fuchsia::net::oldhttp::URLLoader> request)
      override;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpServiceImpl);
};

}  // namespace net_http

#endif  // WEBRUNNER_NET_HTTP_HTTP_SERVICE_IMPL_H_
