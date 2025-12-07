// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_APPKIT_UTILS_H_
#define UI_BASE_COCOA_APPKIT_UTILS_H_

#ifdef __OBJC__
#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#endif  // __OBJC__

#include "base/component_export.h"

namespace ui {

// Whether a force-click event on the touchpad should invoke Quick Look.
COMPONENT_EXPORT(UI_BASE) bool ForceClickInvokesQuickLook();

// Returns true if both CGFloat values are equal.
COMPONENT_EXPORT(UI_BASE) bool IsCGFloatEqual(CGFloat a, CGFloat b);

// Returns true if the current application is the active application.
COMPONENT_EXPORT(UI_BASE) bool IsActiveApplication();

// Returns true if it is possible that accessing pasteboard contents
// programmatically will block with a Pasteboard Privacy alert. Returns false if
// it is known for sure that it will not, either because Chromium has been
// granted an exception to Pasteboard Privacy, or because Chromium has been
// locked down with regard to Pasteboard Privacy and it's certain that the
// access would fail.
//
// TODO(https://crbug.com/419266152): Remove this, and use -[NSPasteboard
// detectPatternsForPatterns:completionHandler:] to accurately convey to the
// user what will happen.
COMPONENT_EXPORT(UI_BASE) bool PasteMightBlockWithPrivacyAlert();

// The NSServicesMenuRequestor protocol does not pass modern NSPasteboardType
// constants in the `types` array, but only obsolete "Pboard" constants. This is
// verified through macOS 15 (FB11838671). These are utility functions to
// rewrite obsolete types to modern types.
//
// TODO(https://crbug.com/395661472): When this FB is fixed at the minimum
// requirement for Chromium, remove these utility functions.

#ifdef __OBJC__
// Converts a single string value of either a modern pasteboard type or an
// obsolete PBoard type to the corresponding UTType. Returns nil if nil is
// specified as the type, or if the type cannot be found.
COMPONENT_EXPORT(UI_BASE) UTType* UTTypeForServicesType(NSString* type);

// Converts an array of string values of either modern pasteboard types or
// obsolete PBoard types to a set of the corresponding UTType values. Invalid
// values are dropped, as NSArrays/NSSets cannot contain nils.
COMPONENT_EXPORT(UI_BASE)
NSSet<UTType*>* UTTypesForServicesTypeArray(NSArray* types);
#endif  // __OBJC__

}  // namespace ui

#endif  // UI_BASE_COCOA_APPKIT_UTILS_H_
