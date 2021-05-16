// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_MOJO_ORIGIN_MOJOM_TRAITS_H_
#define URL_MOJO_ORIGIN_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/mojom/origin.mojom-shared.h"
#include "url/origin.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(URL_MOJOM_TRAITS)
    StructTraits<url::mojom::OriginDataView, url::Origin> {
  static const std::string& scheme(const url::Origin& r) {
    return r.GetTupleOrPrecursorTupleIfOpaque().scheme();
  }
  static const std::string& host(const url::Origin& r) {
    return r.GetTupleOrPrecursorTupleIfOpaque().host();
  }
  static uint16_t port(const url::Origin& r) {
    return r.GetTupleOrPrecursorTupleIfOpaque().port();
  }
  static const absl::optional<base::UnguessableToken> nonce_if_opaque(
      const url::Origin& r) {
    // TODO(nasko): Consider returning a const reference here.
    return r.GetNonceForSerialization();
  }
  static bool Read(url::mojom::OriginDataView data, url::Origin* out);
};

}  // namespace mojo

#endif  // URL_MOJO_ORIGIN_MOJOM_TRAITS_H_
