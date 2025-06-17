// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_ORIGIN_DEBUG_H_
#define URL_ORIGIN_DEBUG_H_

#include "base/component_export.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"

// DEBUG_ALIAS_FOR_ORIGIN(var_name, origin) copies `origin` into a new
// stack-allocated variable named `<var_name>`. This helps ensure that the
// value of `origin` gets preserved in crash dumps.
#define DEBUG_ALIAS_FOR_ORIGIN(var_name, origin) \
  DEBUG_ALIAS_FOR_CSTR(var_name, (origin).Serialize().c_str(), 128)

namespace url {

class Origin;

namespace debug {

class COMPONENT_EXPORT(URL) ScopedOriginCrashKey {
 public:
  ScopedOriginCrashKey(base::debug::CrashKeyString* crash_key,
                       const url::Origin* value);
  ~ScopedOriginCrashKey();

  ScopedOriginCrashKey(const ScopedOriginCrashKey&) = delete;
  ScopedOriginCrashKey& operator=(const ScopedOriginCrashKey&) = delete;

 private:
  base::debug::ScopedCrashKeyString scoped_string_value_;
};

}  // namespace debug

}  // namespace url

#endif  // URL_ORIGIN_DEBUG_H_
