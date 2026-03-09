// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is a stubbed-out version of the header from upstream. Upstream's
// version doesn't compile on all Chromium platforms, and given the choice
// of (1) carrying a patch until perhaps fixing upstream, (2) narrowing the
// set of platforms we build this target for, or (3) patching out the error-
// injection behavior of `nalloc.h`, we chose (3).

#ifndef NALLOC_H_
#define NALLOC_H_

#define nalloc_init(x)
#define nalloc_restrict_file_prefix(x)
#define nalloc_start(x, y)
#define nalloc_end()

#endif  // NALLOC_H_
