// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MENUS_COCOA_MENU_CONTROLLER_H_
#define UI_MENUS_COCOA_MENU_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/component_export.h"

namespace ui {
class MenuModel;
}

COMPONENT_EXPORT(UI_MENUS)
@protocol MenuControllerCocoaDelegate
// Called as each item is created during menu or submenu creation.
- (void)controllerWillAddItem:(NSMenuItem*)menuItem
                    fromModel:(ui::MenuModel*)model
                      atIndex:(size_t)index;
// Called after all menu items in a menu or submenu are created.
- (void)controllerWillAddMenu:(NSMenu*)menu fromModel:(ui::MenuModel*)model;
@end

// A controller for the cross-platform menu model. The menu that's created
// has the tag and represented object set for each menu item. The object is a
// NSValue holding a pointer to the model for that level of the menu (to
// allow for hierarchical menus). The tag is the index into that model for
// that particular item. It is important that the model outlives this object
// as it only maintains weak references.
COMPONENT_EXPORT(UI_MENUS)
@interface MenuControllerCocoa
    : NSObject <NSMenuDelegate, NSUserInterfaceValidations>

- (instancetype)init NS_UNAVAILABLE;

// Builds a NSMenu from the model which must not be null. Changes made to the
// contents of the model after calling this will not be noticed.
- (instancetype)initWithModel:(ui::MenuModel*)model
                     delegate:(id<MenuControllerCocoaDelegate>)delegate;

// Programmatically close the constructed menu.
- (void)cancel;

@property(readwrite) ui::MenuModel* model;

// Access to the constructed menu.
@property(readonly) NSMenu* menu;

// Whether the menu is currently open.
@property(readonly, getter=isMenuOpen) BOOL menuOpen;

@end

#endif  // UI_MENUS_COCOA_MENU_CONTROLLER_H_
