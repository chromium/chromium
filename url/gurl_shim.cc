// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/gurl_shim.h"

#include <string_view>

namespace url {

std::unique_ptr<GURL> CreateGURLFromStr(rust::Str spec) {
  return std::make_unique<GURL>(std::string_view(spec));
}

rust::Str GURLPossiblyInvalidSpec(const GURL& gurl) {
  return rust::Str(gurl.possibly_invalid_spec());
}

rust::Str GURLContent(const GURL& gurl) {
  std::string_view content = gurl.GetContentPiece();
  return rust::Str(content.data(), content.size());
}

rust::Str GURLScheme(const GURL& gurl) {
  std::string_view scheme = gurl.scheme();
  return rust::Str(scheme.data(), scheme.size());
}

bool GURLSchemeIs(const GURL& gurl, rust::Str lower_ascii_scheme) {
  return gurl.SchemeIs(std::string_view(lower_ascii_scheme));
}

}  // namespace url
