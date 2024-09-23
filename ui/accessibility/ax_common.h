// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_COMMON_H_
#define UI_ACCESSIBILITY_AX_COMMON_H_

#include "build/blink_buildflags.h"
#include "build/build_config.h"

#if (!defined(NDEBUG) || defined(ADDRESS_SANITIZER) ||            \
     defined(LEAK_SANITIZER) || defined(MEMORY_SANITIZER) ||      \
     defined(THREAD_SANITIZER) || defined(UNDEFINED_SANITIZER) || \
     DCHECK_IS_ON()) &&                                           \
    BUILDFLAG(USE_BLINK)
// Enable fast fails on clusterfuzz and other builds used to debug Chrome,
// in order to help narrow down illegal states more quickly.
#define AX_FAIL_FAST_BUILD
#endif

// SANITIZER_CHECK's use case is severe, but recoverable situations that need
// priority debugging. They trigger on Clusterfuzz, debug and sanitizer builds.
// Prefer DCHECK() when enabled because it logs messages in the crash tool,
// unlike CHECK().
#if defined(AX_FAIL_FAST_BUILD) && !DCHECK_IS_ON()
#define SANITIZER_CHECK(val) CHECK(val)
#define SANITIZER_CHECK_EQ(val1, val2) CHECK_EQ(val1, val2)
#define SANITIZER_CHECK_NE(val1, val2) CHECK_NE(val1, val2)
#define SANITIZER_CHECK_LE(val1, val2) CHECK_LE(val1, val2)
#define SANITIZER_CHECK_LT(val1, val2) CHECK_LT(val1, val2)
#define SANITIZER_CHECK_GE(val1, val2) CHECK_GE(val1, val2)
#define SANITIZER_CHECK_GT(val1, val2) CHECK_GT(val1, val2)
#define SANITIZER_NOTREACHED() SANITIZER_CHECK(false)
#else
// Fall back on an ordinary DCHECK.
#define SANITIZER_CHECK(val) DCHECK(val)
#define SANITIZER_CHECK_EQ(val1, val2) DCHECK_EQ(val1, val2)
#define SANITIZER_CHECK_NE(val1, val2) DCHECK_NE(val1, val2)
#define SANITIZER_CHECK_LE(val1, val2) DCHECK_LE(val1, val2)
#define SANITIZER_CHECK_LT(val1, val2) DCHECK_LT(val1, val2)
#define SANITIZER_CHECK_GE(val1, val2) DCHECK_GE(val1, val2)
#define SANITIZER_CHECK_GT(val1, val2) DCHECK_GT(val1, val2)
#define SANITIZER_NOTREACHED() DCHECK(false)
#endif  // AX_FAIL_FAST_BUILD && !DCHECK_IS_ON()

#endif  // UI_ACCESSIBILITY_AX_COMMON_H_
