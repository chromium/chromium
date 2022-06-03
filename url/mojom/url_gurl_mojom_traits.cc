// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/mojom/url_gurl_mojom_traits.h"

#include "url/url_constants.h"

namespace mojo {

// static
base::StringPiece StructTraits<url::mojom::UrlDataView, GURL>::url(
    const GURL& r) {
  if (r.possibly_invalid_spec().length() > url::kMaxURLChars || !r.is_valid()) {
    return base::StringPiece();
  }

  return base::StringPiece(r.possibly_invalid_spec().c_str(),
                           r.possibly_invalid_spec().length());
}

// static
bool StructTraits<url::mojom::UrlDataView, GURL>::Read(
    url::mojom::UrlDataView data,
    GURL* out) {
  base::StringPiece url_string;
  if (!data.ReadUrl(&url_string))
    return false;

  if (url_string.length() > url::kMaxURLChars)
    return false;

  *out = GURL(url_string);
  if (!url_string.empty() && !out->is_valid())
    return false;

  return true;
}

}  // namespace mojo
