// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_APPKIT_UTILS_H_
#define UI_BASE_COCOA_APPKIT_UTILS_H_

#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "base/component_export.h"

namespace ui {

// Whether a force-click event on the touchpad should invoke Quick Look.
COMPONENT_EXPORT(UI_BASE) bool ForceClickInvokesQuickLook();

// Returns true if both CGFloat values are equal.
COMPONENT_EXPORT(UI_BASE) bool IsCGFloatEqual(CGFloat a, CGFloat b);

// The NSServicesMenuRequestor protocol does not pass modern NSPasteboardType
// constants in the `types` array, but only obsolete "Pboard" constants. This is
// verified through macOS 15 (FB11838671). These are utility functions to
// rewrite obsolete types to modern types.
//
// TODO(https://crbug.com/395661472): When this FB is fixed at the minimum
// requirement for Chromium, remove these utility functions.

// Converts a single string value of either a modern pasteboard type or an
// obsolete PBoard type to the corresponding UTType. Returns nil if nil is
// specified as the type, or if the type cannot be found.
COMPONENT_EXPORT(UI_BASE) UTType* UTTypeForServicesType(NSString* type);

// Converts an array of string values of either modern pasteboard types or
// obsolete PBoard types to a set of the corresponding UTType values. Invalid
// values are dropped, as NSArrays/NSSets cannot contain nils.
COMPONENT_EXPORT(UI_BASE)
NSSet<UTType*>* UTTypesForServicesTypeArray(NSArray* types);

}  // namespace ui

#endif  // UI_BASE_COCOA_APPKIT_UTILS_H_
