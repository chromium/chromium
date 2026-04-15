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

// LINT.IfChange(VoiceSearchState)

export enum VoiceSearchState {
  VOICE_SEARCH_BUTTON_CLICKED = 0,
  SUCCESSFUL_TRANSCRIPT = 1,
  VOICE_SEARCH_ERROR = 2,
  VOICE_SEARCH_ERROR_AND_CANCELED = 3,
  VOICE_SEARCH_CANCELED = 4,
  MAX_VALUE = VOICE_SEARCH_CANCELED,
}

// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_tasks/enums.xml:VoiceSearchState)
