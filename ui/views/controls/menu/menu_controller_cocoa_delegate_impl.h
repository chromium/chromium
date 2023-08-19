// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_CONTROLLER_COCOA_DELEGATE_IMPL_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_CONTROLLER_COCOA_DELEGATE_IMPL_H_

#import "ui/base/cocoa/menu_controller.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/views_export.h"

VIEWS_EXPORT
@interface MenuControllerCocoaDelegateImpl
    : NSObject <MenuControllerCocoaDelegate>
- (void)setAnchorRect:(gfx::Rect)rect;
@end

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_CONTROLLER_COCOA_DELEGATE_IMPL_H_
