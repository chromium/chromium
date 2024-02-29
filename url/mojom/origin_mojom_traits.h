// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_MOJOM_ORIGIN_MOJOM_TRAITS_H_
#define URL_MOJOM_ORIGIN_MOJOM_TRAITS_H_

#include <optional>

#include "base/component_export.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "mojo/public/cpp/bindings/optional_as_pointer.h"
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
  static mojo::OptionalAsPointer<const base::UnguessableToken> nonce_if_opaque(
      const url::Origin& r) {
    return mojo::OptionalAsPointer(r.GetNonceForSerialization());
  }
  static bool Read(url::mojom::OriginDataView data, url::Origin* out);
};

}  // namespace mojo

#endif  // URL_MOJOM_ORIGIN_MOJOM_TRAITS_H_
