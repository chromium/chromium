// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_MOJOM_URL_GURL_MOJOM_TRAITS_H_
#define URL_MOJOM_URL_GURL_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/strings/string_piece.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "url/gurl.h"
#include "url/mojom/url.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(URL_MOJOM_TRAITS)
    StructTraits<url::mojom::UrlDataView, GURL> {
  static base::StringPiece url(const GURL& r);
  static bool Read(url::mojom::UrlDataView data, GURL* out);
};

}  // namespace mojo

#endif  // URL_MOJOM_URL_GURL_MOJOM_TRAITS_H_
