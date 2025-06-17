// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/gurl_debug.h"

#include "url/gurl.h"

namespace url::debug {

ScopedUrlCrashKey::ScopedUrlCrashKey(base::debug::CrashKeyString* crash_key,
                                     const GURL& url)
    : scoped_string_value_(
          crash_key,
          url.is_empty() ? "<empty url>" : url.possibly_invalid_spec()) {}

ScopedUrlCrashKey::~ScopedUrlCrashKey() = default;

}  // namespace url::debug
