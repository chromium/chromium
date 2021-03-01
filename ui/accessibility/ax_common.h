// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_COMMON_H_
#define UI_ACCESSIBILITY_AX_COMMON_H_

#if (!defined(NDEBUG) || defined(ADDRESS_SANITIZER) ||            \
     defined(LEAK_SANITIZER) || defined(MEMORY_SANITIZER) ||      \
     defined(THREAD_SANITIZER) || defined(UNDEFINED_SANITIZER) || \
     DCHECK_IS_ON()) &&                                           \
    !defined(OS_IOS)
// Enable fast fails on clusterfuzz and other builds used to debug Chrome,
// in order to help narrow down illegal states more quickly.
#define AX_FAIL_FAST_BUILD
#endif

#endif  // UI_ACCESSIBILITY_AX_COMMON_H_
