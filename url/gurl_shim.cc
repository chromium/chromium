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

}  // namespace url
