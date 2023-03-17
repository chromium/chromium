// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_COCOA_BASE_UTILS_H_
#define UI_BASE_COCOA_COCOA_BASE_UTILS_H_

#import <Cocoa/Cocoa.h>

#include "base/component_export.h"
#include "ui/base/window_open_disposition.h"

namespace ui {

// Retrieves the WindowOpenDisposition used to open a link from a user gesture
// represented by |event|. For example, a Cmd+Click would mean open the
// associated link in a background tab.
COMPONENT_EXPORT(UI_BASE)
WindowOpenDisposition WindowOpenDispositionFromNSEvent(NSEvent* event);

// Retrieves the WindowOpenDisposition used to open a link from a user gesture
// represented by |event|, but instead use the modifier flags given by |flags|,
// which is the same format as |-NSEvent modifierFlags|. This allows
// substitution of the modifiers without having to create a new event from
// scratch.
COMPONENT_EXPORT(UI_BASE)
WindowOpenDisposition WindowOpenDispositionFromNSEventWithFlags(
    NSEvent* event,
    NSUInteger flags);

}  // namespace ui

#endif  // UI_BASE_COCOA_COCOA_BASE_UTILS_H_
