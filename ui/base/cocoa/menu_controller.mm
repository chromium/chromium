// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/menu_controller.h"

#include "base/bind.h"
#include "base/check_op.h"
#include "base/mac/foundation_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/platform_accelerator_cocoa.h"
#include "ui/base/interaction/element_tracker_mac.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"
#import "ui/events/event_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

// Called when an empty submenu is created. This inserts a menu item labeled
// "(empty)" into the submenu. Matches Windows behavior.
NSMenu* MakeEmptySubmenu() {
  base::scoped_nsobject<NSMenu> submenu([[NSMenu alloc] initWithTitle:@""]);
  NSString* empty_menu_title =
      l10n_util::GetNSString(IDS_APP_MENU_EMPTY_SUBMENU);
  [submenu addItemWithTitle:empty_menu_title action:NULL keyEquivalent:@""];
  [[submenu itemAtIndex:0] setEnabled:NO];
  return submenu.autorelease();
}

// Called when adding a submenu to the menu and checks if the submenu, via its
// |model|, has visible child items.
bool MenuHasVisibleItems(const ui::MenuModel* model) {
  size_t count = model->GetItemCount();
  for (size_t index = 0; index < count; ++index) {
    if (model->IsVisibleAt(index))
      return true;
  }
  return false;
}

}  // namespace

// This class stores a base::WeakPtr<ui::MenuModel> as an Objective-C object,
// which allows it to be stored in the representedObject field of an NSMenuItem.
@interface WeakPtrToMenuModelAsNSObject : NSObject
+ (instancetype)weakPtrForModel:(ui::MenuModel*)model;
+ (ui::MenuModel*)getFrom:(id)instance;
- (instancetype)initWithModel:(ui::MenuModel*)model;
- (ui::MenuModel*)menuModel;
@end

@implementation WeakPtrToMenuModelAsNSObject {
  base::WeakPtr<ui::MenuModel> _model;
}

+ (instancetype)weakPtrForModel:(ui::MenuModel*)model {
  return
      [[[WeakPtrToMenuModelAsNSObject alloc] initWithModel:model] autorelease];
}

+ (ui::MenuModel*)getFrom:(id)instance {
  return [base::mac::ObjCCastStrict<WeakPtrToMenuModelAsNSObject>(instance)
      menuModel];
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
            fromModel:(ui::MenuModel*)model
    withColorProvider:(const ui::ColorProvider*)colorProvider;

// Creates a NSMenu from the given model. If the model has submenus, this can
// be invoked recursively.
- (NSMenu*)menuFromModel:(ui::MenuModel*)model
       withColorProvider:(const ui::ColorProvider*)colorProvider;

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
  base::scoped_nsobject<NSMenu> _menu;
  BOOL _useWithPopUpButtonCell;  // If YES, 0th item is blank
  BOOL _isMenuOpen;
  id<MenuControllerCocoaDelegate> _delegate;
}

@synthesize useWithPopUpButtonCell = _useWithPopUpButtonCell;

- (ui::MenuModel*)model {
  return _model.get();
}

- (void)setModel:(ui::MenuModel*)model {
  _model = model->AsWeakPtr();
}

- (instancetype)init {
  self = [super init];
  return self;
}

- (instancetype)initWithModel:(ui::MenuModel*)model
                     delegate:(id<MenuControllerCocoaDelegate>)delegate
                colorProvider:(const ui::ColorProvider*)colorProvider
       useWithPopUpButtonCell:(BOOL)useWithCell {
  if ((self = [self initWithModel:model
                         delegate:delegate
           useWithPopUpButtonCell:useWithCell])) {
    [self maybeBuildWithColorProvider:colorProvider];
  }
  return self;
}

- (instancetype)initWithModel:(ui::MenuModel*)model
                     delegate:(id<MenuControllerCocoaDelegate>)delegate
       useWithPopUpButtonCell:(BOOL)useWithCell {
  if ((self = [super init])) {
    _model = model->AsWeakPtr();
    _delegate = delegate;
    _useWithPopUpButtonCell = useWithCell;
  }
  return self;
}

- (void)dealloc {
  [_menu setDelegate:nil];

  // Close the menu if it is still open. This could happen if a tab gets closed
  // while its context menu is still open.
  [self cancel];
  _model = nullptr;

  [super dealloc];
}

- (void)setDelegate:(id<MenuControllerCocoaDelegate>)delegate {
  _delegate = delegate;
}

- (void)cancel {
  if (_isMenuOpen) {
    [_menu cancelTracking];
    if (_model)
      _model->MenuWillClose();
    _isMenuOpen = NO;
  }
}

- (NSMenu*)menuFromModel:(ui::MenuModel*)model
       withColorProvider:(const ui::ColorProvider*)colorProvider {
  NSMenu* menu = [[[NSMenu alloc] initWithTitle:@""] autorelease];

  const size_t count = model->GetItemCount();
  for (size_t index = 0; index < count; ++index) {
    if (model->GetTypeAt(index) == ui::MenuModel::TYPE_SEPARATOR) {
      [self addSeparatorToMenu:menu atIndex:index];
    } else {
      [self addItemToMenu:menu
                    atIndex:index
                  fromModel:model
          withColorProvider:colorProvider];
    }
  }

  return menu;
}

- (void)addSeparatorToMenu:(NSMenu*)menu atIndex:(size_t)index {
  NSMenuItem* separator = [NSMenuItem separatorItem];
  [menu insertItem:separator atIndex:base::checked_cast<NSInteger>(index)];
}

- (void)addItemToMenu:(NSMenu*)menu
              atIndex:(size_t)index
            fromModel:(ui::MenuModel*)model
    withColorProvider:(const ui::ColorProvider*)colorProvider {
  auto rawLabel = model->GetLabelAt(index);
  NSString* label = model->MayHaveMnemonicsAt(index)
                        ? l10n_util::FixUpWindowsStyleLabel(rawLabel)
                        : base::SysUTF16ToNSString(rawLabel);
  base::scoped_nsobject<NSMenuItem> item([[NSMenuItem alloc]
      initWithTitle:label
             action:@selector(itemSelected:)
      keyEquivalent:@""]);

  // If the menu item has an icon, set it.
  ui::ImageModel icon = model->GetIconAt(index);
  if (icon.IsImage())
    [item setImage:icon.GetImage().ToNSImage()];

  ui::MenuModel::ItemType type = model->GetTypeAt(index);
  const NSInteger modelIndex = base::checked_cast<NSInteger>(index);
  if (type == ui::MenuModel::TYPE_SUBMENU && model->IsVisibleAt(index)) {
    ui::MenuModel* submenuModel = model->GetSubmenuModelAt(index);

    // If there are visible items, recursively build the submenu.
    NSMenu* submenu = MenuHasVisibleItems(submenuModel)
                          ? [self menuFromModel:submenuModel
                                withColorProvider:colorProvider]
                          : MakeEmptySubmenu();

    [item setTarget:nil];
    [item setAction:nil];
    [item setSubmenu:submenu];
    // [item setSubmenu] updates target and action which means clicking on a
    // submenu entry will not call [self validateUserInterfaceItem].
    DCHECK_EQ([item action], @selector(submenuAction:));
    DCHECK_EQ([item target], submenu);
    // Set the enabled state here as submenu entries do not call into
    // validateUserInterfaceItem. See crbug.com/981294 and crbug.com/991472.
    [item setEnabled:model->IsEnabledAt(index)];
  } else {
    // The MenuModel works on indexes so we can't just set the command id as the
    // tag like we do in other menus. Also set the represented object to be
    // the model so hierarchical menus check the correct index in the correct
    // model. Setting the target to |self| allows this class to participate
    // in validation of the menu items.
    [item setTag:modelIndex];
    [item setTarget:self];
    [item setRepresentedObject:[WeakPtrToMenuModelAsNSObject
                                   weakPtrForModel:model]];
    // On the Mac, context menus never have accelerators. Menus constructed
    // for context use have useWithPopUpButtonCell_ set to NO.
    if (_useWithPopUpButtonCell) {
      ui::Accelerator accelerator;
      if (model->GetAcceleratorAt(index, &accelerator)) {
        NSString* key_equivalent;
        NSUInteger modifier_mask;
        GetKeyEquivalentAndModifierMaskFromAccelerator(
            accelerator, &key_equivalent, &modifier_mask);
        [item setKeyEquivalent:key_equivalent];
        [item setKeyEquivalentModifierMask:modifier_mask];
      }
    }
  }

  if (_delegate) {
    [_delegate controllerWillAddItem:item
                           fromModel:model
                             atIndex:index
                   withColorProvider:colorProvider];
  }

  [menu insertItem:item atIndex:modelIndex];
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  SEL action = [item action];
  if (action != @selector(itemSelected:))
    return NO;

  ui::MenuModel* model =
      [WeakPtrToMenuModelAsNSObject getFrom:[(id)item representedObject]];
  if (!model)
    return NO;

  const size_t modelIndex = base::checked_cast<size_t>([item tag]);
  BOOL checked = model->IsItemCheckedAt(modelIndex);
  DCHECK([(id)item isKindOfClass:[NSMenuItem class]]);
  [(id)item
      setState:(checked ? NSControlStateValueOn : NSControlStateValueOff)];
  [(id)item setHidden:(!model->IsVisibleAt(modelIndex))];
  if (model->IsItemDynamicAt(modelIndex)) {
    // Update the label and the icon.
    NSString* label =
        l10n_util::FixUpWindowsStyleLabel(model->GetLabelAt(modelIndex));
    [(id)item setTitle:label];

    ui::ImageModel icon = model->GetIconAt(modelIndex);
    [(id)item setImage:icon.IsImage() ? icon.GetImage().ToNSImage() : nil];
  }
  const gfx::FontList* font_list = model->GetLabelFontListAt(modelIndex);
  if (font_list) {
    NSDictionary* attributes =
        @{NSFontAttributeName : font_list->GetPrimaryFont().GetNativeFont()};
    base::scoped_nsobject<NSAttributedString> title([[NSAttributedString alloc]
        initWithString:[(id)item title]
            attributes:attributes]);
    [(id)item setAttributedTitle:title.get()];
  }
  return model->IsEnabledAt(modelIndex);
}

- (void)itemSelected:(id)sender {
  ui::MenuModel* model =
      [WeakPtrToMenuModelAsNSObject getFrom:[sender representedObject]];
  DCHECK(model);
  const size_t modelIndex = base::checked_cast<size_t>([sender tag]);
  const ui::ElementIdentifier identifier =
      model->GetElementIdentifierAt(modelIndex);
  if (identifier) {
    ui::ElementTrackerMac::GetInstance()->NotifyMenuItemActivated([sender menu],
                                                                  identifier);
  }
  model->ActivatedAt(modelIndex,
                     ui::EventFlagsFromNative([NSApp currentEvent]));
  // Note: |self| may be destroyed by the call to ActivatedAt().
}

- (void)maybeBuildWithColorProvider:(const ui::ColorProvider*)colorProvider {
  if (_menu || !_model)
    return;

  _menu.reset([[self menuFromModel:_model.get()
                 withColorProvider:colorProvider] retain]);
  [_menu setDelegate:self];

  // TODO(dfried): Ideally we'd do this after each submenu is created.
  // However, the way we currently hook menu events only supports the root
  // menu. Therefore we call this method here and submenus are not supported
  // for auto-highlighting or ElementTracker events.
  if (_delegate)
    [_delegate controllerWillAddMenu:_menu fromModel:_model.get()];

  // If this is to be used with a NSPopUpButtonCell, add an item at the 0th
  // position that's empty. Doing it after the menu has been constructed won't
  // complicate creation logic, and since the tags are model indexes, they
  // are unaffected by the extra item.
  if (_useWithPopUpButtonCell) {
    base::scoped_nsobject<NSMenuItem> blankItem(
        [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""]);
    [_menu insertItem:blankItem atIndex:0];
  }
}

- (NSMenu*)menu {
  [self maybeBuildWithColorProvider:nullptr];
  return _menu.get();
}

- (BOOL)isMenuOpen {
  return _isMenuOpen;
}

- (void)menuWillOpen:(NSMenu*)menu {
  _isMenuOpen = YES;
  if (_model)
    _model->MenuWillShow();  // Note: |model_| may trigger -[self dealloc].
}

- (void)menuDidClose:(NSMenu*)menu {
  if (_isMenuOpen) {
    _isMenuOpen = NO;
    if (_model)
      _model->MenuWillClose();  // Note: |model_| may trigger -[self dealloc].
  }
}

@end

@implementation MenuControllerCocoa (TestingAPI)

- (BOOL)isMenuBuiltForTesting {
  return _menu != nil;
}

@end
