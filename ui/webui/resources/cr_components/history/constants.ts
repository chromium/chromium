// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Histogram buckets for UMA tracking of which type of result the History user
 * clicked.
 */
export enum HistoryResultType {
  TRADITIONAL = 0,
  GROUPED = 1,
  EMBEDDINGS = 2,
  END = 3,
}

// Unclicked query results that live for less than this amount of milliseconds
// are ignored from the metrics perspective. This is to account for the fact
// that new query results are fetched per user keystroke.
export const QUERY_RESULT_MINIMUM_AGE = 2000;
