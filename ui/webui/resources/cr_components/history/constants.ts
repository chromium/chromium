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

/**
 * Histogram buckets for UMA tracking of Embeddings-related UMA actions. They
 * are defined here rather than in the history_embeddings component, because
 * History component itself needs to call this to provide a proper comparison
 * for users that don't have Embeddings enabled.
 */
export enum HistoryEmbeddingsUserActions {
  NON_EMPTY_QUERY_HISTORY_SEARCH = 0,

  // Intermediate values are omitted because they are never used from WebUI.
  // This is a total count, not the "last" usable enum value. It should be
  // updated to `HistoryEmbeddingsUserActions::kMaxValue + 1` if that changes.
  // See related comment in
  // chrome/browser/ui/webui/cr_components/history_embeddings/history_embeddings_handler.h

  // HistoryEmbeddingsUserActions::kMaxValue = 6
  END = 7,
}

// Unclicked query results that live for less than this amount of milliseconds
// are ignored from the metrics perspective. This is to account for the fact
// that new query results are fetched per user keystroke.
export const QUERY_RESULT_MINIMUM_AGE = 2000;
