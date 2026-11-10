// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_DRAG_COORDINATOR_H_
#define UI_BASE_COCOA_DRAG_COORDINATOR_H_

#include "base/component_export.h"

@class NSView;

// The macOS drag system is missing API features; this set of functions provides
// them.
//
// Drag tracking
// -------------
// Is a drag in progress? What view is responsible for the drag in progress?
//
// There are situations in which it would be bad to allow a drag to be
// initiated. These are often not accounted for by AppKit, which assumes a
// reasonable and non-adversarial usage of its APIs.
//
// IsDragSessionInitiationAllowed() allows the prevention of the initiation of a
// drag session while:
//   - a drag session is already in progress
//   - a menu tracking session is already in progress
//
// macOS 15+ does prevent the first, but it does not prevent the second. Given
// that these are security concerns, take this into our own hands.
//
// Drag cancellation
// -----------------
// If view becomes an invalid drag source, CancelDragSessionInProgressForView()
// will cancel a drag in progress from that view while not affecting other
// drags.
//
// References
// ----------
// See https://crbug.com/553160293, https://crbug.com/553163770, and
// https://crbug.com/553163820 for the concerns with concurrent drag sessions.

namespace ui {

// Returns whether a drag session may be initiated.
COMPONENT_EXPORT(UI_BASE)
bool IsDragSessionInitiationAllowed();

// Sets the state of whether a drag session is in progress. `responsible_view`
// is the NSView responsible (in a philosophical sense) for the current drag, or
// nil if a drag is ending.
COMPONENT_EXPORT(UI_BASE)
void SetDragSessionInProgress(NSView* responsible_view);

// Cancels the current drag if the `owning_view` or any subview is responsible
// for it.
COMPONENT_EXPORT(UI_BASE)
void CancelDragSessionInProgressForView(NSView* owning_view);

}  // namespace ui

#endif  // UI_BASE_COCOA_DRAG_COORDINATOR_H_
