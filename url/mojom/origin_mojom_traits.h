// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_MOJO_ORIGIN_MOJOM_TRAITS_H_
#define URL_MOJO_ORIGIN_MOJOM_TRAITS_H_

#include "base/strings/string_piece.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "url/mojom/origin.mojom.h"
#include "url/origin.h"

namespace mojo {

template <>
struct StructTraits<url::mojom::OriginDataView, url::Origin> {
  static const std::string& scheme(const url::Origin& r) {
    return r.GetTupleOrPrecursorTupleIfOpaque().scheme();
  }
  static const std::string& host(const url::Origin& r) {
    return r.GetTupleOrPrecursorTupleIfOpaque().host();
  }
  static uint16_t port(const url::Origin& r) {
    return r.GetTupleOrPrecursorTupleIfOpaque().port();
  }
  static const base::Optional<base::UnguessableToken> nonce_if_opaque(
      const url::Origin& r) {
    // TODO(nasko): Consider returning a const reference here.
    return r.GetNonceForSerialization();
  }
  static bool Read(url::mojom::OriginDataView data, url::Origin* out) {
    base::StringPiece scheme, host;
    base::Optional<base::UnguessableToken> nonce_if_opaque;
    if (!data.ReadScheme(&scheme) || !data.ReadHost(&host) ||
        !data.ReadNonceIfOpaque(&nonce_if_opaque))
      return false;

    base::Optional<url::Origin> creation_result =
        nonce_if_opaque
            ? url::Origin::UnsafelyCreateOpaqueOriginWithoutNormalization(
                  scheme, host, data.port(),
                  url::Origin::Nonce(*nonce_if_opaque))
            : url::Origin::UnsafelyCreateTupleOriginWithoutNormalization(
                  scheme, host, data.port());
    if (!creation_result)
      return false;

    *out = std::move(creation_result.value());
    return true;
  }
};

}  // namespace mojo

#endif  // URL_MOJO_ORIGIN_MOJOM_TRAITS_H_
