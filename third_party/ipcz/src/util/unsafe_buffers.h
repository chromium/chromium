// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_UTIL_UNSAFE_BUFFERS_H_
#define IPCZ_SRC_UTIL_UNSAFE_BUFFERS_H_

// Version of safe buffer macros for ipcz.

#if defined(IPCZ_STANDALONE)
#include "standalone/base/unsafe_buffers.h"  // nogncheck
#else  // IPCZ_STANDALONE
#include "base/compiler_specific.h" // nogncheck

#define IPCZ_UNSAFE_BUFFER_USAGE UNSAFE_BUFFER_USAGE
#define IPCZ_UNSAFE_BUFFERS(...) UNSAFE_BUFFERS(__VA_ARGS__)
#define IPCZ_UNSAFE_TODO(...) UNSAFE_TODO(__VA_ARGS__)
#endif  // IPCZ_STANDALONE

#endif  // IPCZ_SRC_UTIL_UNSAFE_BUFFERS_H_
