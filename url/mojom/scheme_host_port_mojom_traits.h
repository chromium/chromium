// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_MOJOM_SCHEME_HOST_PORT_MOJOM_TRAITS_H_
#define URL_MOJOM_SCHEME_HOST_PORT_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "url/mojom/scheme_host_port.mojom-shared.h"
#include "url/scheme_host_port.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(URL_MOJOM_TRAITS)
    StructTraits<url::mojom::SchemeHostPortDataView, url::SchemeHostPort> {
  static const std::string& scheme(const url::SchemeHostPort& r) {
    return r.scheme();
  }
  static const std::string& host(const url::SchemeHostPort& r) {
    return r.host();
  }
  static uint16_t port(const url::SchemeHostPort& r) { return r.port(); }
  static bool Read(url::mojom::SchemeHostPortDataView data,
                   url::SchemeHostPort* out);
};

}  // namespace mojo

#endif  // URL_MOJOM_SCHEME_HOST_PORT_MOJOM_TRAITS_H_
