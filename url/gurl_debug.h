// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_GURL_DEBUG_H_
#define URL_GURL_DEBUG_H_

#include "base/component_export.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"

// DEBUG_ALIAS_FOR_GURL(var_name, url) copies |url| into a new stack-allocated
// variable named |<var_name>|.  This helps ensure that the value of |url| gets
// preserved in crash dumps.
#define DEBUG_ALIAS_FOR_GURL(var_name, url) \
  DEBUG_ALIAS_FOR_CSTR(var_name, (url).possibly_invalid_spec().c_str(), 128)

class GURL;

namespace url::debug {

class COMPONENT_EXPORT(URL) ScopedUrlCrashKey {
 public:
  ScopedUrlCrashKey(base::debug::CrashKeyString* crash_key, const GURL& value);
  ~ScopedUrlCrashKey();

  ScopedUrlCrashKey(const ScopedUrlCrashKey&) = delete;
  ScopedUrlCrashKey& operator=(const ScopedUrlCrashKey&) = delete;

 private:
  base::debug::ScopedCrashKeyString scoped_string_value_;
};

}  // namespace url::debug

#endif  // URL_GURL_DEBUG_H_
