// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_DRAG_PERMISSION_H_
#define UI_BASE_COCOA_DRAG_PERMISSION_H_

#include "base/component_export.h"

// There are several times when it would be bad to allow a drag to be initiated.
// These are often not accounted for by AppKit, which assumes a reasonable and
// non-adversarial usage of its APIs. This file handles coordination of defense
// against those attacks.
//
// This class is meant to prevent the initiation of a drag session while:
//   - a drag session is already in progress
//   - a menu tracking session is already in progress
//
// macOS 15+ does prevent the first, but it does not prevent the second. Given
// that these are security concerns, take this into our own hands.
//
// See https://crbug.com/553160293, https://crbug.com/553163770, and
// https://crbug.com/553163820 for the concerns with concurrent drag sessions.

namespace ui {

// Returns whether a drag session may be initiated.
COMPONENT_EXPORT(UI_BASE)
bool IsDragSessionInitiationAllowed();

// Sets the state of whether a drag session is in progress.
COMPONENT_EXPORT(UI_BASE)
void SetDragSessionInProgress(bool in_progress);

}  // namespace ui

#endif  // UI_BASE_COCOA_DRAG_PERMISSION_H_
