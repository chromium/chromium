// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_TOOL_TIP_BASE_VIEW_H_
#define UI_BASE_COCOA_TOOL_TIP_BASE_VIEW_H_

#include "base/memory/raw_ptr.h"

#import <AppKit/AppKit.h>

#include "base/component_export.h"
#import "ui/base/cocoa/base_view.h"

// An NSiew that allows tooltip text to be set at the current mouse location. It
// can take effect immediately, but won't appear unless the tooltip delay has
// elapsed.
COMPONENT_EXPORT(UI_BASE)
@interface ToolTipBaseView : BaseView {
 @private
  // These are part of the magic tooltip code from WebKit's WebHTMLView:
  id _trackingRectOwner;  // (not retained)
  raw_ptr<void> _trackingRectUserData;
  NSTrackingRectTag _lastToolTipTag;
  base::scoped_nsobject<NSString> _toolTip;
}

// Set the current tooltip. It is the responsibility of the caller to set a nil
// tooltip when the mouse cursor leaves the appropriate region.
- (void)setToolTipAtMousePoint:(NSString*)string;

@end

#endif  // UI_BASE_COCOA_TOOL_TIP_BASE_VIEW_H_
