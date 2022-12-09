// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOLINT(build/header_guard)
// no-include-guard-because-multiply-included

// FN is a macro that appends a suffix to a given name. In the real code (in
// metablock.c), FN is redefined, then this file is included reapeatedly to
// generate the same code for names with different suffixes.

// For this test, HistogramType will expand to HistogramLiteral.
#define HistogramType FN(Histogram)

// For this test, FN(BlockSplitter) will expand to BlockSplitterLiteral.
struct FN(BlockSplitter) {
  // No error expected, as the source location for this field declaration will
  // be "<scratch space>" and the real file path cannot be detected.
  HistogramType* histograms_;
};
