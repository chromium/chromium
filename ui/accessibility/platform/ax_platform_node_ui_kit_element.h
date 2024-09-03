// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_UI_KIT_ELEMENT_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_UI_KIT_ELEMENT_H_

#import <UIKit/UIKit.h>

#include "ui/accessibility/platform/ax_platform_node_ios.h"

// AXPlatformNodeUIKitElement is a wrapper around AXPlatformNodeIOS.
// This class converts the cross-platform a11y tree into a format UIKit can
// query.
@interface AXPlatformNodeUIKitElement : UIAccessibilityElement

// The accessibility tree node associated with this wrapper.
@property(nonatomic, readonly) ui::AXPlatformNodeIOS* node;

// This creates a UIKit accessibility element around the given
// AXPlatformNodeIOS.
- (instancetype)initWithPlatformNode:(ui::AXPlatformNodeIOS*)platformNode;

// Invalidate children for a non-ignored ancestor (including self).
- (void)childrenChanged;

// Disconnect this wrapper node from its associated node in the accessibility
// tree.
- (void)detach;

@end

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_UI_KIT_ELEMENT_H_
