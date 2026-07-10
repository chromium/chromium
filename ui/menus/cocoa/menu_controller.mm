// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/menus/cocoa/menu_controller.h"

#import <AppKit/AppKit.h>
#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/owned_objc.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/mac/mac_util.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/platform_accelerator_cocoa.h"
#include "ui/base/interaction/element_tracker_mac.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/themed_vector_icon.h"
#import "ui/events/event_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

// Whether MenuControllerCocoa uses the new menu icon scheme. See the method
// comment on +initializeWithNewMenuIconScheme: for more details.
static bool g_use_new_menu_icon_scheme = false;

// Called when an empty submenu is created. This inserts a menu item labeled
// "(empty)" into the submenu. Matches Windows behavior.
NSMenu* MakeEmptySubmenu() {
  NSMenu* submenu = [[NSMenu alloc] initWithTitle:@""];
  NSString* empty_menu_title =
      l10n_util::GetNSString(IDS_APP_MENU_EMPTY_SUBMENU);
  [submenu addItemWithTitle:empty_menu_title action:nullptr keyEquivalent:@""];
  [submenu itemAtIndex:0].enabled = NO;
  return submenu;
}

// Called when adding a submenu to the menu and checks if the submenu, via its
// |model|, has visible child items.
bool MenuHasVisibleItems(const ui::MenuModel* model) {
  size_t count = model->GetItemCount();
  for (size_t index = 0; index < count; ++index) {
    if (model->IsVisibleAt(index)) {
      return true;
    }
  }
  return false;
}

void SetMenuItemIcon(NSMenuItem* menu_item,
                     ui::ImageModel icon,
                     BOOL is_context_menu) {
  bool should_use_icons = [is_context_menu] {
    // The question of whether icons should be used is a question of context
    // menus.
    if (!is_context_menu) {
      return true;
    }

    // For new-scheme menu icons, macOS 26+ gets icons; earlier versions don't.
    if (g_use_new_menu_icon_scheme) {
      return base::mac::MacOSMajorVersion() >= 26;
    }

    // For old-scheme menu icons, icons are always shown, though not all icons
    // (as per `should_use_vector_icons`).
    return true;
  }();

  if (!should_use_icons) {
    return;
  }

  bool should_use_vector_icons = [is_context_menu] {
    // The question of whether icons should be used is a question of context
    // menus.
    if (!is_context_menu) {
      return true;
    }

    // Vector icons are only shown with the new icon scheme.
    return g_use_new_menu_icon_scheme;
  }();

  NSImage* menu_item_image;

  if (icon.IsImage()) {
    menu_item_image = icon.GetImage().ToNSImage();
  } else if (icon.IsVectorIcon()) {
    if (should_use_vector_icons) {
      ui::ThemedVectorIcon themed_icon(icon.GetVectorIcon());
      menu_item_image =
          gfx::Image(themed_icon.GetImageSkia(SK_ColorBLACK)).ToNSImage();
      [menu_item_image setTemplate:YES];
    } else {
      menu_item_image = nil;
    }
  } else if (icon.IsEmpty()) {
    menu_item_image = nil;
  } else {
    // A non-empty ui::ImageModel can be one of three types. The "image
    // generator" type isn't currently used for any menu items shown on the Mac,
    // so die if it is encountered. Implement handling it if that changes.
    CHECK(icon.IsImageGenerator());
    NOTREACHED();
  }

  menu_item.image = menu_item_image;
}

NSImage* CreatePaddedImage(CGFloat width, NSImage* image) {
  NSImage* result = [NSImage
       imageWithSize:NSMakeSize(width, image.size.height)
             flipped:NO
      drawingHandler:^BOOL(NSRect dest_rect) {
        if (image) {
          // Draw the image centered in the given space.
          CGFloat extra_width = dest_rect.size.width - image.size.width;
          CGFloat extra_height = dest_rect.size.height - image.size.height;
          dest_rect =
              NSIntegralRect(NSInsetRect(dest_rect, /*dX=*/extra_width / 2,
                                         /*dY=*/extra_height / 2));
          [image drawInRect:dest_rect];
        }
        return YES;
      }];
  [result setTemplate:[image isTemplate]];
  return result;
}

void CreateSharedIndentsForMenu(NSMenu* menu) {
  if (!g_use_new_menu_icon_scheme || base::mac::MacOSMajorVersion() < 26) {
    return;
  }

  // For the new menu icon scheme, on macOS 26+, what is wanted is for menu
  // items to be indented within a section if any item in that section has an
  // image set. For example:
  //
  // ┌───────────────┐
  // │ [img] Item 1  │
  // │       Item 2  │ ← This is indented due to items 1 and 3.
  // │ [img] Item 3  │
  // │       Item 4  │ ← This is indented due to items 1 and 3.
  // ├───────────────┤
  // │ Item 5        │
  // │ Item 6        │
  // └───────────────┘
  //
  // macOS 26+ has this style as well, but with a huge caveat. Only symbol
  // images will cause this shared indentation. If a non-symbol image is set,
  // then no shared indentation will happen.
  //
  // Earlier versions of this code (see the commit history) resolved this issue
  // by overriding +[NSContextMenuItemView _imageWidthSharingForItem:], and
  // having it return "true" for any image, not just symbol images.
  //
  // However, things changed in macOS 27. That release requires more than half
  // of the items in a section to have an icon for the remaining items in the
  // section to share the indent. In the example above, because only 50% of the
  // items have an image, the other items would not share the indent.
  //
  // The machinery within AppKit's NSContextMenuImpl that manages sharing the
  // indent is complicated. While overriding a function to return "true" to
  // share the indent was simple enough, overriding the "more than 50%" code is
  // too intricate and prone to change to make it worth trying to hack. Instead,
  // do it manually.

  // Make a copy of the menu items for two reasons. First, this makes walking
  // the item array easier, both in terms of syntax as well as not hammering the
  // NSMenu. Second, this allows the appending of a separator item locally, to
  // allow the code below to assume that every section is ended by a separator
  // item, and as a result be simpler.
  NSMutableArray<NSMenuItem*>* items = [menu.itemArray mutableCopy];
  [items addObject:[NSMenuItem separatorItem]];

  CGFloat largest_image_width = 0;
  for (NSMenuItem* item in items) {
    largest_image_width = std::max(largest_image_width, item.image.size.width);
  }

  NSUInteger start_of_section = 0;
  bool section_has_image = false;
  for (NSUInteger i = 0; i < items.count; ++i) {
    NSMenuItem* item = items[i];
    if (!item.separatorItem) {
      section_has_image |= (item.image != nil);
      continue;
    }

    // End of section. Did any item in that section have an image? If so,
    // pad all the images (and create empty pads for those items with no image).
    if (section_has_image) {
      for (NSUInteger j = start_of_section; j < i; ++j) {
        NSMenuItem* section_item = items[j];
        section_item.image =
            CreatePaddedImage(largest_image_width, section_item.image);
      }
    }

    // Reset tracking variables and move on to the next section (if any).
    section_has_image = false;
    start_of_section = i + 1;
  }
}

}  // namespace

// This class stores a base::WeakPtr<ui::MenuModel> as an Objective-C object,
// which allows it to be stored in the representedObject field of an NSMenuItem.
@interface WeakPtrToMenuModelAsNSObject : NSObject
+ (instancetype)weakPtrForModel:(ui::MenuModel*)model;
+ (ui::MenuModel*)menuModelForNSMenuItem:(NSMenuItem*)menuItem;
@property(readonly) ui::MenuModel* menuModel;
@end

@implementation WeakPtrToMenuModelAsNSObject {
  base::WeakPtr<ui::MenuModel> _model;
}

+ (instancetype)weakPtrForModel:(ui::MenuModel*)model {
  return [[WeakPtrToMenuModelAsNSObject alloc] initWithModel:model];
}

+ (ui::MenuModel*)menuModelForNSMenuItem:(NSMenuItem*)menuItem {
  return base::apple::ObjCCastStrict<WeakPtrToMenuModelAsNSObject>(
             menuItem.representedObject)
      .menuModel;
}

- (instancetype)initWithModel:(ui::MenuModel*)model {
  if ((self = [super init])) {
    _model = model->AsWeakPtr();
  }
  return self;
}

- (ui::MenuModel*)menuModel {
  return _model.get();
}

@end

// Internal methods.
@interface MenuControllerCocoa ()
// Called before the menu is to be displayed to update the state (enabled,
// radio, etc) of each item in the menu. Also will update the title if the item
// is marked as "dynamic".
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item;

// Adds the item at |index| in |model| as an NSMenuItem at |index| of |menu|.
// Associates a submenu if the MenuModel::ItemType is TYPE_SUBMENU.
- (void)addItemToMenu:(NSMenu*)menu
              atIndex:(size_t)index
            fromModel:(ui::MenuModel*)model;

// Creates a NSMenu from the given model. If the model has submenus, this can
// be invoked recursively.
- (NSMenu*)menuFromModel:(ui::MenuModel*)model;

// Adds a separator item at the given index. As the separator doesn't need
// anything from the model, this method doesn't need the model index as the
// other method below does.
- (void)addSeparatorToMenu:(NSMenu*)menu atIndex:(size_t)index;

// Called when the user chooses a particular menu item. AppKit sends this only
// after the menu has fully faded out. |sender| is the menu item chosen.
- (void)itemSelected:(id)sender;
@end

@implementation MenuControllerCocoa {
  base::WeakPtr<ui::MenuModel> _model;
  NSMenu* __strong _menu;
  BOOL _isMenuOpen;
  BOOL _isContextMenu;
  id<MenuControllerCocoaDelegate> __weak _delegate;
}

+ (void)initializeWithNewMenuIconScheme:(BOOL)newScheme {
  g_use_new_menu_icon_scheme = newScheme;
}

- (ui::MenuModel*)model {
  return _model.get();
}

- (void)setModel:(ui::MenuModel*)model {
  _model = model->AsWeakPtr();
}

- (instancetype)initWithModel:(ui::MenuModel*)model
                isContextMenu:(BOOL)contextMenu
                     delegate:(id<MenuControllerCocoaDelegate>)delegate {
  if ((self = [super init])) {
    _model = model->AsWeakPtr();
    _isContextMenu = contextMenu;
    _delegate = delegate;
    [self buildMenu];
  }
  return self;
}

- (void)dealloc {
  _menu.delegate = nil;

  // Close the menu if it is still open. This could happen if a tab gets closed
  // while its context menu is still open.
  [self cancel];
  _model = nullptr;
}

- (void)cancel {
  if (_isMenuOpen) {
    [_menu cancelTracking];
    if (_model) {
      _model->MenuWillClose();
    }
    _isMenuOpen = NO;
  }
}

- (NSMenu*)menuFromModel:(ui::MenuModel*)model {
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@""];

  const size_t count = model->GetItemCount();
  for (size_t index = 0; index < count; ++index) {
    if (model->GetTypeAt(index) == ui::MenuModel::TYPE_SEPARATOR) {
      [self addSeparatorToMenu:menu atIndex:index];
    } else {
      [self addItemToMenu:menu atIndex:index fromModel:model];
    }
  }

  CreateSharedIndentsForMenu(menu);
  return menu;
}

- (void)addSeparatorToMenu:(NSMenu*)menu atIndex:(size_t)index {
  NSMenuItem* separator = [NSMenuItem separatorItem];
  [menu insertItem:separator atIndex:base::checked_cast<NSInteger>(index)];
}

- (void)addItemToMenu:(NSMenu*)menu
              atIndex:(size_t)index
            fromModel:(ui::MenuModel*)model {
  auto rawLabel = model->GetLabelAt(index);
  NSString* label = model->MayHaveMnemonicsAt(index)
                        ? l10n_util::FixUpWindowsStyleLabel(rawLabel)
                        : base::SysUTF16ToNSString(rawLabel);
  NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:label
                                                action:@selector(itemSelected:)
                                         keyEquivalent:@""];

  SetMenuItemIcon(item, model->GetIconAt(index), _isContextMenu);

  ui::MenuModel::ItemType type = model->GetTypeAt(index);
  const NSInteger modelIndex = base::checked_cast<NSInteger>(index);
  if (type == ui::MenuModel::TYPE_SUBMENU && model->IsVisibleAt(index)) {
    ui::MenuModel* submenuModel = model->GetSubmenuModelAt(index);

    // If there are visible items, recursively build the submenu.
    NSMenu* submenu = MenuHasVisibleItems(submenuModel)
                          ? [self menuFromModel:submenuModel]
                          : MakeEmptySubmenu();

    item.target = nil;
    item.action = nil;
    item.submenu = submenu;
    // [item setSubmenu:] updates target and action which means clicking on a
    // submenu entry will not call [self validateUserInterfaceItem:].
    DCHECK_EQ(item.action, @selector(submenuAction:));
    DCHECK_EQ(item.target, submenu);
    // Set the enabled state here as submenu entries do not call into
    // validateUserInterfaceItem. See https://crbug.com/40634897 and
    // https://crbug.com/41474827.
    [item setEnabled:model->IsEnabledAt(index)];
  } else {
    // The MenuModel works on indexes so we can't just set the command id as the
    // tag like we do in other menus. Also set the represented object to be
    // the model so hierarchical menus check the correct index in the correct
    // model. Setting the target to |self| allows this class to participate
    // in validation of the menu items.
    item.tag = modelIndex;
    item.target = self;
    item.representedObject =
        [WeakPtrToMenuModelAsNSObject weakPtrForModel:model];
    // On the Mac, context menus do not have accelerators except when
    // |force_show_accelerator_for_item| is set to true. Consult with the Mac
    // team before using the flag!
    if (model->GetForceShowAcceleratorForItemAt(index)) {
      ui::Accelerator accelerator;
      if (model->GetAcceleratorAt(index, &accelerator)) {
        KeyEquivalentAndModifierMask* equivalent =
            GetKeyEquivalentAndModifierMaskFromAccelerator(accelerator);
        item.keyEquivalent = equivalent.keyEquivalent;
        item.keyEquivalentModifierMask = equivalent.modifierMask;
      }
    }
  }

  if (_delegate) {
    [_delegate controllerWillAddItem:item fromModel:model atIndex:index];
  }

  [menu insertItem:item atIndex:modelIndex];
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  NSMenuItem* menuItem = base::apple::ObjCCastStrict<NSMenuItem>(item);

  SEL action = menuItem.action;
  if (action != @selector(itemSelected:)) {
    return NO;
  }

  ui::MenuModel* model =
      [WeakPtrToMenuModelAsNSObject menuModelForNSMenuItem:menuItem];
  if (!model) {
    return NO;
  }

  const size_t modelIndex = base::checked_cast<size_t>(menuItem.tag);
  BOOL checked = model->IsItemCheckedAt(modelIndex);
  menuItem.state = checked ? NSControlStateValueOn : NSControlStateValueOff;
  menuItem.hidden = !model->IsVisibleAt(modelIndex);

  if (model->IsItemDynamicAt(modelIndex)) {
    // Update the label and the icon.
    NSString* label =
        l10n_util::FixUpWindowsStyleLabel(model->GetLabelAt(modelIndex));
    menuItem.title = label;

    SetMenuItemIcon(menuItem, model->GetIconAt(modelIndex), _isContextMenu);
  }

  const gfx::FontList* font_list = model->GetLabelFontListAt(modelIndex);
  if (font_list) {
    CTFontRef font = font_list->GetPrimaryFont().GetCTFont();
    NSDictionary* attributes =
        @{NSFontAttributeName : base::apple::CFToNSPtrCast(font)};
    NSAttributedString* title =
        [[NSAttributedString alloc] initWithString:menuItem.title
                                        attributes:attributes];
    menuItem.attributedTitle = title;
  }
  return model->IsEnabledAt(modelIndex);
}

- (void)itemSelected:(id)sender {
  NSMenuItem* menuItem = base::apple::ObjCCastStrict<NSMenuItem>(sender);

  ui::MenuModel* model =
      [WeakPtrToMenuModelAsNSObject menuModelForNSMenuItem:menuItem];
  DCHECK(model);
  const size_t modelIndex = base::checked_cast<size_t>(menuItem.tag);
  const ui::ElementIdentifier identifier =
      model->GetElementIdentifierAt(modelIndex);
  if (identifier) {
    ui::ElementTrackerMac::GetInstance()->NotifyMenuItemActivated(menuItem.menu,
                                                                  identifier);
  }
  model->ActivatedAt(
      modelIndex,
      ui::EventFlagsFromNative(base::apple::OwnedNSEvent(NSApp.currentEvent)));
  // Note: |self| may be destroyed by the call to ActivatedAt().
}

- (void)buildMenu {
  // The menu is eagerly built in the initializer, so the model cannot be null
  // at this point.
  CHECK(_model);

  _menu = [self menuFromModel:_model.get()];
  _menu.delegate = self;

  // TODO(dfried): Ideally we'd do this after each submenu is created.
  // However, the way we currently hook menu events only supports the root
  // menu. Therefore we call this method here and submenus are not supported
  // for auto-highlighting or ElementTracker events.
  if (_delegate) {
    [_delegate controllerWillAddMenu:_menu fromModel:_model.get()];
  }
}

- (NSMenu*)menu {
  return _menu;
}

- (BOOL)isMenuOpen {
  return _isMenuOpen;
}

- (void)menuWillOpen:(NSMenu*)menu {
  _isMenuOpen = YES;
  if (_model) {
    _model->MenuWillShow();  // Note: |model_| may trigger -[self dealloc].
  }
}

- (void)menuDidClose:(NSMenu*)menu {
  if (_isMenuOpen) {
    _isMenuOpen = NO;
    if (_model) {
      _model->MenuWillClose();  // Note: |model_| may trigger -[self dealloc].
    }
  }
}

@end
