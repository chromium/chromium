// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * @fileoverview Shared constants for the composebox, animated-glow-element,
 * searchbox.
 */

export enum GlowAnimationState {
  NONE = '',
  DRAGGING = 'dragging',
  EXPANDING = 'expanding',
  SUBMITTING = 'submitting',
  LISTENING = 'listening',
}

// LINT.IfChange(ContextAddedMethod)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
export const enum ComposeboxContextAddedMethod {
  CONTEXT_MENU = 0,
  COPY_PASTE = 1,
  DRAG_AND_DROP = 2,
  RECENT_TAB_CHIP = 3,
  MAX_VALUE = RECENT_TAB_CHIP,
}

// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_search/enums.xml:ContextAddedMethod)
