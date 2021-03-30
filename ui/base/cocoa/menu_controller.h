// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_MENU_CONTROLLER_H_
#define UI_BASE_COCOA_MENU_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include <string>

#include "base/component_export.h"
#include "base/mac/scoped_nsobject.h"

namespace ui {
class MenuModel;
}

COMPONENT_EXPORT(UI_BASE)
@protocol MenuControllerCocoaDelegate
- (void)controllerWillAddItem:(NSMenuItem*)menuItem
                    fromModel:(ui::MenuModel*)model
                      atIndex:(NSInteger)index;
@end

// A controller for the cross-platform menu model. The menu that's created
// has the tag and represented object set for each menu item. The object is a
// NSValue holding a pointer to the model for that level of the menu (to
// allow for hierarchical menus). The tag is the index into that model for
// that particular item. It is important that the model outlives this object
// as it only maintains weak references.
COMPONENT_EXPORT(UI_BASE)
@interface MenuControllerCocoa
    : NSObject<NSMenuDelegate, NSUserInterfaceValidations>

// Note that changing this will have no effect if you use
// |-initWithModel:useWithPopUpButtonCell:| or after the first call to |-menu|.
@property(nonatomic, assign) BOOL useWithPopUpButtonCell;

// NIB-based initializer. This does not create a menu. Clients can set the
// properties of the object and the menu will be created upon the first call to
// |-menu|. Note that the menu will be immutable after creation.
- (instancetype)init;

// Builds a NSMenu from the pre-built model (must not be nil). Changes made
// to the contents of the model after calling this will not be noticed. If
// the menu will be displayed by a NSPopUpButtonCell, it needs to be of a
// slightly different form (0th item is empty). Note this attribute of the menu
// cannot be changed after it has been created.
- (instancetype)initWithModel:(ui::MenuModel*)model
                     delegate:(id<MenuControllerCocoaDelegate>)delegate
       useWithPopUpButtonCell:(BOOL)useWithCell;

// Programmatically close the constructed menu.
- (void)cancel;

- (ui::MenuModel*)model;
- (void)setModel:(ui::MenuModel*)model;

// Access to the constructed menu if the complex initializer was used. If the
// default initializer was used, then this will create the menu on first call.
- (NSMenu*)menu;

// Whether the menu is currently open.
- (BOOL)isMenuOpen;

@end

#endif  // UI_BASE_COCOA_MENU_CONTROLLER_H_
