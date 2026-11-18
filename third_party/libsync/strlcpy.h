// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_LIBSYNC_STRLCPY_H_
#define THIRD_PARTY_LIBSYNC_STRLCPY_H_

#include <stddef.h>

// Declaration for the strlcpy() implementation in strlcpy.c. src/sync.c calls
// strlcpy(), but glibc only declares it since 2.38 and Chromium's Linux sysroot
// has an older glibc. BUILD.gn force-includes this header on Linux and
// ChromeOS. Remove it once the sysroot has glibc 2.38 or newer.
size_t strlcpy(char* dst, const char* src, size_t siz);

#endif  // THIRD_PARTY_LIBSYNC_STRLCPY_H_
