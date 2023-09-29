// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/mojom/scheme_host_port_mojom_traits.h"

#include <string_view>

#include "url/mojom/scheme_host_port.mojom-shared.h"
#include "url/scheme_host_port.h"

namespace mojo {

// static
bool StructTraits<url::mojom::SchemeHostPortDataView, url::SchemeHostPort>::
    Read(url::mojom::SchemeHostPortDataView data, url::SchemeHostPort* out) {
  std::string_view scheme, host;
  if (!data.ReadScheme(&scheme) || !data.ReadHost(&host))
    return false;

  *out = url::SchemeHostPort(scheme, host, data.port());

  // Consider it an error if the output SchemeHostPort is not valid, but
  // non-empty values were received over Mojo.
  return out->IsValid() || (scheme.empty() && host.empty() && data.port() == 0);
}

}  // namespace mojo
