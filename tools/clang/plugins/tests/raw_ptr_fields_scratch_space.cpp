// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// // Based on third_party/brotli/enc/metablock_inc.h

class HistogramLiteral;

#define FN(X) X##Literal
#include "raw_ptr_fields_scratch_space_inc.h" /* NOLINT(build/include) */
#undef FN
