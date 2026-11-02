// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Histogram buckets for UMA tracking of which type of result the History user
 * clicked. Values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
export enum HistoryResultType {
  TRADITIONAL = 0,
  GROUPED = 1,
  // EMBEDDINGS = 2 is obsolete.
  END = 3,
}
