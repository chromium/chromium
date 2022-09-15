// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

// WARNING: This test uses a platform-specific system header.  This means that
// the test might (expectedly) fail on platforms that don't provide such header.
#include <dlfcn.h>

void foo(const Dl_info& dl_info) {
  // Fields in non-Chromium locations are not affected (e.g. |apply_edits.py|
  // filters out edit directives that apply to files that |git| doesn't know
  // about).  For example, |Dl_info::dli_fbase| won't be rewritten and therefore
  // shouldn't be treated as "affected" in the expression below.
  //
  // No rewrite expected below - the expression below shouldn't turn into
  // |dl_info.dli_fbase.get()|.
  uintptr_t v = reinterpret_cast<uintptr_t>(dl_info.dli_fbase);
}
