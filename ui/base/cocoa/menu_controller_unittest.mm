// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#import "testing/gtest_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#import "ui/base/cocoa/menu_controller.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#import "ui/base/test/cocoa_helper.h"
#include "ui/events/test/cocoa_test_event_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/strings/grit/ui_strings.h"

using base::ASCIIToUTF16;

@interface MenuControllerCocoa (TestingAPI)
- (void)itemWillBeSelected:(NSMenuItem*)sender;
- (void)itemSelected:(id)sender;
@end

@interface TestResponsiveMenuController : MenuControllerCocoa
@property(assign, nonatomic) BOOL sawItemEarly;
@end

@implementation TestResponsiveMenuController {
  BOOL sawItemEarly_;
}

@synthesize sawItemEarly = sawItemEarly_;

- (void)itemWillBeSelected:(NSMenuItem*)sender {
  sawItemEarly_ = YES;
  [super itemWillBeSelected:sender];
}

@end

@interface WatchedLifetimeMenuController : MenuControllerCocoa
@property(assign, nonatomic) BOOL* deallocCalled;
@end

@implementation WatchedLifetimeMenuController {
  BOOL* deallocCalled_;
}

@synthesize deallocCalled = deallocCalled_;

- (void)dealloc {
  *deallocCalled_ = YES;
  [super dealloc];
}

@end

@interface NSMenuItem (Private)
// Exposed to simulate in testing.
- (void)_sendItemSelectedNote;
@end

namespace ui {

namespace {

const int kTestLabelResourceId = IDS_APP_SCROLLBAR_CXMENU_SCROLLHERE;

class MenuControllerTest : public CocoaTest {};

class TestSimpleMenuModelVisibility : public SimpleMenuModel {
 public:
  explicit TestSimpleMenuModelVisibility(SimpleMenuModel::Delegate* delegate)
      : SimpleMenuModel(delegate) {}

  // SimpleMenuModel:
  bool IsVisibleAt(int index) const override {
    return items_[ValidateItemIndex(index)].visible;
  }

  void SetVisibility(int command_id, bool visible) {
    int index = SimpleMenuModel::GetIndexOfCommandId(command_id);
    items_[ValidateItemIndex(index)].visible = visible;
  }

  void AddItem(int command_id, const base::string16& label) {
    SimpleMenuModel::AddItem(command_id, label);
    items_.push_back({true, command_id});
  }

  void AddSubMenuWithStringId(int command_id, int string_id, MenuModel* model) {
    SimpleMenuModel::AddSubMenuWithStringId(command_id, string_id, model);
    items_.push_back({true, command_id});
  }

 private:
  struct Item {
    bool visible;
    int command_id;
  };

  typedef std::vector<Item> ItemVector;

  int ValidateItemIndex(int index) const {
    CHECK_GE(index, 0);
    CHECK_LT(static_cast<size_t>(index), items_.size());
    return index;
  }

  ItemVector items_;

  DISALLOW_COPY_AND_ASSIGN(TestSimpleMenuModelVisibility);
};

// A menu delegate that counts the number of times certain things are called
// to make sure things are hooked up properly.
class Delegate : public SimpleMenuModel::Delegate {
 public:
  Delegate() {}

  bool IsCommandIdChecked(int command_id) const override { return false; }
  bool IsCommandIdEnabled(int command_id) const override {
    ++enable_count_;
    return true;
  }
  void ExecuteCommand(int command_id, int event_flags) override {
    ++execute_count_;
  }

  void OnMenuWillShow(SimpleMenuModel* /*source*/) override {
    EXPECT_FALSE(did_show_);
    EXPECT_FALSE(did_close_);
    did_show_ = true;
    if (auto_close_) {
      NSArray* modes = @[ NSEventTrackingRunLoopMode, NSDefaultRunLoopMode ];
      [menu_to_close_ performSelector:@selector(cancelTracking)
                           withObject:nil
                           afterDelay:0.1
                              inModes:modes];
    }
  }

  void MenuClosed(SimpleMenuModel* /*source*/) override {
    EXPECT_TRUE(did_show_);
    EXPECT_FALSE(did_close_);
    DCHECK(!did_close_);
    did_close_ = true;
  }

  int execute_count_ = 0;
  mutable int enable_count_ = 0;
  // The menu on which to call |-cancelTracking| after a short delay in
  // OnMenuWillShow.
  NSMenu* menu_to_close_ = nil;
  bool did_show_ = false;
  bool did_close_ = false;
  bool auto_close_ = true;

 private:
  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

// Just like Delegate, except the items are treated as "dynamic" so updates to
// the label/icon in the model are reflected in the menu.
class DynamicDelegate : public Delegate {
 public:
  DynamicDelegate() {}
  bool IsItemForCommandIdDynamic(int command_id) const override { return true; }
  base::string16 GetLabelForCommandId(int command_id) const override {
    return label_;
  }
  bool GetIconForCommandId(int command_id, gfx::Image* icon) const override {
    if (icon_.IsEmpty()) {
      return false;
    } else {
      *icon = icon_;
      return true;
    }
  }
  void SetDynamicLabel(base::string16 label) { label_ = label; }
  void SetDynamicIcon(const gfx::Image& icon) { icon_ = icon; }

 private:
  base::string16 label_;
  gfx::Image icon_;
};

// A SimpleMenuModel::Delegate that owns the MenuControllerCocoa and deletes
// itself when the command is executed.
class OwningDelegate : public Delegate {
 public:
  OwningDelegate(bool* did_delete, BOOL* did_dealloc)
      : did_delete_(did_delete), model_(this) {
    model_.AddItem(1, ASCIIToUTF16("foo"));
    controller_.reset([[WatchedLifetimeMenuController alloc]
                 initWithModel:&model_
        useWithPopUpButtonCell:NO]);
    [controller_ setDeallocCalled:did_dealloc];
  }

  MenuControllerCocoa* controller() { return controller_; }

  // Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    // Although -[MenuControllerCocoa menuDidClose:] has been invoked,
    // SimpleMenuModel always posts a task to call Delegate::MenuClosed(), to
    // ensure it happens after the command. It uses a weak pointer to |model_|,
    // so the task will expire before being run.
    EXPECT_FALSE(did_close_);

    EXPECT_EQ(0, execute_count_);
    Delegate::ExecuteCommand(command_id, event_flags);
    delete this;
  }

 private:
  ~OwningDelegate() override {
    EXPECT_FALSE(*did_delete_);
    *did_delete_ = true;
  }

  bool* did_delete_;
  SimpleMenuModel model_;
  base::scoped_nsobject<WatchedLifetimeMenuController> controller_;

  DISALLOW_COPY_AND_ASSIGN(OwningDelegate);
};

// Menu model that returns a gfx::FontList object for one of the items in the
// menu.
class FontListMenuModel : public SimpleMenuModel {
 public:
  FontListMenuModel(SimpleMenuModel::Delegate* delegate,
                    const gfx::FontList* font_list,
                    int index)
      : SimpleMenuModel(delegate), font_list_(font_list), index_(index) {}
  ~FontListMenuModel() override {}
  const gfx::FontList* GetLabelFontListAt(int index) const override {
    return (index == index_) ? font_list_ : NULL;
  }

 private:
  const gfx::FontList* font_list_;
  const int index_;
};

TEST_F(MenuControllerTest, EmptyMenu) {
  Delegate delegate;
  SimpleMenuModel model(&delegate);
  base::scoped_nsobject<MenuControllerCocoa> menu([[MenuControllerCocoa alloc]
               initWithModel:&model
      useWithPopUpButtonCell:NO]);
  EXPECT_EQ(0, [[menu menu] numberOfItems]);
}

TEST_F(MenuControllerTest, BasicCreation) {
  Delegate delegate;
  SimpleMenuModel model(&delegate);
  model.AddItem(1, ASCIIToUTF16("one"));
  model.AddItem(2, ASCIIToUTF16("two"));
  model.AddItem(3, ASCIIToUTF16("three"));
  model.AddSeparator(NORMAL_SEPARATOR);
  model.AddItem(4, ASCIIToUTF16("four"));
  model.AddItem(5, ASCIIToUTF16("five"));

  base::scoped_nsobject<MenuControllerCocoa> menu([[MenuControllerCocoa alloc]
               initWithModel:&model
      useWithPopUpButtonCell:NO]);
  EXPECT_EQ(6, [[menu menu] numberOfItems]);

  // Check the title, tag, and represented object are correct for a random
  // element.
  NSMenuItem* itemTwo = [[menu menu] itemAtIndex:2];
  NSString* title = [itemTwo title];
  EXPECT_EQ(ASCIIToUTF16("three"), base::SysNSStringToUTF16(title));
  EXPECT_EQ(2, [itemTwo tag]);

  EXPECT_TRUE([[[menu menu] itemAtIndex:3] isSeparatorItem]);
}

TEST_F(MenuControllerTest, Submenus) {
  Delegate delegate;
  SimpleMenuModel model(&delegate);
  model.AddItem(1, ASCIIToUTF16("one"));
  SimpleMenuModel submodel(&delegate);
  submodel.AddItem(2, ASCIIToUTF16("sub-one"));
  submodel.AddItem(3, ASCIIToUTF16("sub-two"));
  submodel.AddItem(4, ASCIIToUTF16("sub-three"));
  model.AddSubMenuWithStringId(5, kTestLabelResourceId, &submodel);
  model.AddItem(6, ASCIIToUTF16("three"));

  base::scoped_nsobject<MenuControllerCocoa> menu([[MenuControllerCocoa alloc]
               initWithModel:&model
      useWithPopUpButtonCell:NO]);
  EXPECT_EQ(3, [[menu menu] numberOfItems]);

  // Inspect the submenu to ensure it has correct properties.
  NSMenuItem* menuItem = [[menu menu] itemAtIndex:1];
  EXPECT_TRUE([menuItem isEnabled]);
  NSMenu* submenu = [menuItem submenu];
  EXPECT_TRUE(submenu);
  EXPECT_EQ(3, [submenu numberOfItems]);

  // Inspect one of the items to make sure it has the correct model as its
  // represented object and the proper tag.
  NSMenuItem* submenuItem = [submenu itemAtIndex:1];
  NSString* title = [submenuItem title];
  EXPECT_EQ(ASCIIToUTF16("sub-two"), base::SysNSStringToUTF16(title));
  EXPECT_EQ(1, [submenuItem tag]);

  // Make sure the item after the submenu is correct and its represented
  // object is back to the top model.
  NSMenuItem* item = [[menu menu] itemAtIndex:2];
  title = [item title];
  EXPECT_EQ(ASCIIToUTF16("three"), base::SysNSStringToUTF16(title));
  EXPECT_EQ(2, [item tag]);
}

TEST_F(MenuControllerTest, EmptySubmenu) {
  Delegate delegate;
  SimpleMenuModel model(&delegate);
  model.AddItem(1, ASCIIToUTF16("one"));
  SimpleMenuModel submodel(&delegate);
  model.AddSubMenuWithStringId(2, kTestLabelResourceId, &submodel);

  base::scoped_nsobject<MenuControllerCocoa> menu([[MenuControllerCocoa alloc]
               initWithModel:&model
      useWithPopUpButtonCell:NO]);
  EXPECT_EQ(2, [[menu menu] numberOfItems]);

  // Inspect the submenu to ensure it has one item labeled "(empty)".
  NSMenu* submenu = [[[menu menu] itemAtIndex:1] submenu];
  EXPECT_TRUE(submenu);
  EXPECT_EQ(1, [submenu numberOfItems]);

  EXPECT_NSEQ(@"(empty)", [[submenu itemAtIndex:0] title]);
}

// Tests that an empty menu item, "(empty)", is added to a submenu that contains
// hidden child items.
TEST_F(MenuControllerTest, EmptySubmenuWhenAllChildItemsAreHidden) {
  Delegate delegate;
  TestSimpleMenuModelVisibility model(&delegate);
  model.AddItem(1, ASCIIToUTF16("one"));
  TestSimpleMenuModelVisibility submodel(&delegate);
  // Hide the two child menu items.
  submodel.AddItem(2, ASCIIToUTF16("sub-one"));
  submodel.SetVisibility(2, false);
  submodel.AddItem(3, ASCIIToUTF16("sub-two"));
  submodel.SetVisibility(3, false);
  model.AddSubMenuWithStringId(4, kTestLabelResourceId, &submodel);

  base::scoped_nsobject<MenuControllerCocoa> menu([[MenuControllerCocoa alloc]
               initWithModel:&model
      useWithPopUpButtonCell:NO]);
  EXPECT_EQ(2, [[menu menu] numberOfItems]);

  // Inspect the submenu to ensure it has one item labeled "(empty)".
  NSMenu* submenu = [[[menu menu] itemAtIndex:1] submenu];
  EXPECT_TRUE(submenu);
  EXPECT_EQ(1, [submenu numberOfItems]);

  EXPECT_NSEQ(@"(empty)", [[submenu itemAtIndex:0] title]);
}

// Tests hiding a submenu item. If a submenu item with children is set to
// hidden, then the submenu should hide.
TEST_F(MenuControllerTest, HiddenSubmenu) {
  // SimpleMenuModel posts a task that calls Delegate::MenuClosed.
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);

  // Create the model.
  Delegate delegate;
  TestSimpleMenuModelVisibility model(&delegate);
  model.AddItem(1, ASCIIToUTF16("one"));
  TestSimpleMenuModelVisibility submodel(&delegate);
  submodel.AddItem(2, ASCIIToUTF16("sub-one"));
  submodel.AddItem(3, ASCIIToUTF16("sub-two"));
  // Set the submenu to be hidden.
  model.AddSubMenuWithStringId(4, kTestLabelResourceId, &submodel);

  model.SetVisibility(4, false);

  // Create the controller.
  base::scoped_nsobject<MenuControllerCocoa> menu_controller(
      [[MenuControllerCocoa alloc] initWithModel:&model
                          useWithPopUpButtonCell:NO]);
  EXPECT_EQ(2, [[menu_controller menu] numberOfItems]);
  delegate.menu_to_close_ = [menu_controller menu];

  // Show the menu.
  CFRunLoopPerformBlock(CFRunLoopGetCurrent(), NSEventTrackingRunLoopMode, ^{
    EXPECT_TRUE([menu_controller isMenuOpen]);
    // Ensure that the submenu is hidden.
    NSMenuItem* item = [[menu_controller menu] itemAtIndex:1];
    EXPECT_TRUE([item isHidden]);
  });

  // Pop open the menu, which will spin an event-tracking run loop.
  [NSMenu popUpContextMenu:[menu_controller menu]
                 withEvent:cocoa_test_event_utils::RightMouseDownAtPoint(
                               NSZeroPoint)
                   forView:[test_window() contentView]];

  EXPECT_FALSE([menu_controller isMenuOpen]);

  // Pump the task that notifies the delegate.
  base::RunLoop().RunUntilIdle();

  // Expect that the delegate got notified properly.
  EXPECT_TRUE(delegate.did_close_);
}

TEST_F(MenuControllerTest, DisabledSubmenu) {
  // SimpleMenuModel posts a task that calls Delegate::MenuClosed.
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);

  // Create the model.
  Delegate delegate;
  SimpleMenuModel model(&delegate);
  model.AddItem(1, ASCIIToUTF16("one"));
  SimpleMenuModel disabled_submodel(&delegate);
  disabled_submodel.AddItem(2, ASCIIToUTF16("disabled_submodel"));
  model.AddSubMenuWithStringId(3, kTestLabelResourceId, &disabled_submodel);
  SimpleMenuModel enabled_submodel(&delegate);
  enabled_submodel.AddItem(4, ASCIIToUTF16("enabled_submodel"));
  model.AddSubMenuWithStringId(5, kTestLabelResourceId, &enabled_submodel);

  // Disable the first submenu entry.
  model.SetEnabledAt(1, false);

  // Create the controller.
  base::scoped_nsobject<MenuControllerCocoa> menu_controller(
      [[MenuControllerCocoa alloc] initWithModel:&model
                          useWithPopUpButtonCell:NO]);
  delegate.menu_to_close_ = [menu_controller menu];

  // Show the menu.
  CFRunLoopPerformBlock(CFRunLoopGetCurrent(), NSEventTrackingRunLoopMode, ^{
    EXPECT_TRUE([menu_controller isMenuOpen]);

    // Ensure that the disabled submenu is disabled.
    NSMenuItem* disabled_item = [[menu_controller menu] itemAtIndex:1];
    EXPECT_FALSE([disabled_item isEnabled]);

    // Ensure that the enabled submenu is enabled.
    NSMenuItem* enabled_item = [[menu_controller menu] itemAtIndex:2];
    EXPECT_TRUE([enabled_item isEnabled]);
  });

  // Pop open the menu, which will spin an event-tracking run loop.
  [NSMenu popUpContextMenu:[menu_controller menu]
                 withEvent:cocoa_test_event_utils::RightMouseDownAtPoint(
                               NSZeroPoint)
                   forView:[test_window() contentView]];
  EXPECT_FALSE([menu_controller isMenuOpen]);

  // Pump the task that notifies the delegate.
  base::RunLoop().RunUntilIdle();
  // Expect that the delegate got notified properly.
  EXPECT_TRUE(delegate.did_close_);
}

TEST_F(MenuControllerTest, PopUpButton) {
  Delegate delegate;
  SimpleMenuModel model(&delegate);
  model.AddItem(1, ASCIIToUTF16("one"));
  model.AddItem(2, ASCIIToUTF16("two"));
  model.AddItem(3, ASCIIToUTF16("three"));

  // Menu should have an extra item inserted at position 0 that has an empty
  // title.
  base::scoped_nsobject<MenuControllerCocoa> menu([[MenuControllerCocoa alloc]
               initWithModel:&model
      useWithPopUpButtonCell:YES]);
  EXPECT_EQ(4, [[menu menu] numberOfItems]);
  EXPECT_EQ(base::string16(),
            base::SysNSStringToUTF16([[[menu menu] itemAtIndex:0] title]));

  // Make sure the tags are still correct (the index no longer matches the tag).
  NSMenuItem* itemTwo = [[menu menu] itemAtIndex:2];
  EXPECT_EQ(1, [itemTwo tag]);
}

TEST_F(MenuControllerTest, Execute) {
  Delegate delegate;
  SimpleMenuModel model(&delegate);
  model.AddItem(1, ASCIIToUTF16("one"));
  base::scoped_nsobject<MenuControllerCocoa> menu([[MenuControllerCocoa alloc]
               initWithModel:&model
      useWithPopUpButtonCell:NO]);
  EXPECT_EQ(1, [[menu menu] numberOfItems]);

  // Fake selecting the menu item, we expect the delegate to be told to execute
  // a command.
  NSMenuItem* item = [[menu menu] itemAtIndex:0];
  [[item target] performSelector:[item action] withObject:item];
  EXPECT_EQ(1, delegate.execute_count_);
}

void Validate(MenuControllerCocoa* controller, NSMenu* menu) {
  for (int i = 0; i < [menu numberOfItems]; ++i) {
    NSMenuItem* item = [menu itemAtIndex:i];
    [controller validateUserInterfaceItem:item];
    if ([item hasSubmenu])
      Validate(controller, [item submenu]);
  }
}

TEST_F(MenuControllerTest, Validate) {
  Delegate delegate;
  SimpleMenuModel model(&delegate);
  model.AddItem(1, ASCIIToUTF16("one"));
  model.AddItem(2, ASCIIToUTF16("two"));
  SimpleMenuModel submodel(&delegate);
  submodel.AddItem(2, ASCIIToUTF16("sub-one"));
  model.AddSubMenuWithStringId(3, kTestLabelResourceId, &submodel);

  base::scoped_nsobject<MenuControllerCocoa> menu([[MenuControllerCocoa alloc]
               initWithModel:&model
      useWithPopUpButtonCell:NO]);
  EXPECT_EQ(3, [[menu menu] numberOfItems]);

  Validate(menu.get(), [menu menu]);
}

// Tests that items which have a font set actually use that font.
TEST_F(MenuControllerTest, LabelFontList) {
  Delegate delegate;
  const gfx::FontList& bold =
      ResourceBundle::GetSharedInstance().GetFontListWithDelta(
          0, gfx::Font::NORMAL, gfx::Font::Weight::BOLD);
  FontListMenuModel model(&delegate, &bold, 0);
  model.AddItem(1, ASCIIToUTF16("one"));
  model.AddItem(2, ASCIIToUTF16("two"));

  base::scoped_nsobject<MenuControllerCocoa> menu([[MenuControllerCocoa alloc]
               initWithModel:&model
      useWithPopUpButtonCell:NO]);
  EXPECT_EQ(2, [[menu menu] numberOfItems]);

  Validate(menu.get(), [menu menu]);

  EXPECT_TRUE([[[menu menu] itemAtIndex:0] attributedTitle] != nil);
  EXPECT_TRUE([[[menu menu] itemAtIndex:1] attributedTitle] == nil);
}

TEST_F(MenuControllerTest, DefaultInitializer) {
  Delegate delegate;
  SimpleMenuModel model(&delegate);
  model.AddItem(1, ASCIIToUTF16("one"));
  model.AddItem(2, ASCIIToUTF16("two"));
  model.AddItem(3, ASCIIToUTF16("three"));

  base::scoped_nsobject<MenuControllerCocoa> menu(
      [[MenuControllerCocoa alloc] init]);
  EXPECT_FALSE([menu menu]);

  [menu setModel:&model];
  [menu setUseWithPopUpButtonCell:NO];
  EXPECT_TRUE([menu menu]);
  EXPECT_EQ(3, [[menu menu] numberOfItems]);

  // Check immutability.
  model.AddItem(4, ASCIIToUTF16("four"));
  EXPECT_EQ(3, [[menu menu] numberOfItems]);
}

// Test that menus with dynamic labels actually get updated.
TEST_F(MenuControllerTest, Dynamic) {
  DynamicDelegate delegate;

  // Create a menu containing a single item whose label is "initial" and who has
  // no icon.
  base::string16 initial = ASCIIToUTF16("initial");
  delegate.SetDynamicLabel(initial);
  SimpleMenuModel model(&delegate);
  model.AddItem(1, ASCIIToUTF16("foo"));
  base::scoped_nsobject<MenuControllerCocoa> menu([[MenuControllerCocoa alloc]
               initWithModel:&model
      useWithPopUpButtonCell:NO]);
  EXPECT_EQ(1, [[menu menu] numberOfItems]);
  // Validate() simulates opening the menu - the item label/icon should be
  // initialized after this so we can validate the menu contents.
  Validate(menu.get(), [menu menu]);
  NSMenuItem* item = [[menu menu] itemAtIndex:0];
  // Item should have the "initial" label and no icon.
  EXPECT_EQ(initial, base::SysNSStringToUTF16([item title]));
  EXPECT_EQ(nil, [item image]);

  // Now update the item to have a label of "second" and an icon.
  base::string16 second = ASCIIToUTF16("second");
  delegate.SetDynamicLabel(second);
  const gfx::Image& icon = gfx::test::CreateImage(32, 32);
  delegate.SetDynamicIcon(icon);
  // Simulate opening the menu and validate that the item label + icon changes.
  Validate(menu.get(), [menu menu]);
  EXPECT_EQ(second, base::SysNSStringToUTF16([item title]));
  EXPECT_TRUE([item image] != nil);

  // Now get rid of the icon and make sure it goes away.
  delegate.SetDynamicIcon(gfx::Image());
  Validate(menu.get(), [menu menu]);
  EXPECT_EQ(second, base::SysNSStringToUTF16([item title]));
  EXPECT_EQ(nil, [item image]);
}

TEST_F(MenuControllerTest, OpenClose) {
  // SimpleMenuModel posts a task that calls Delegate::MenuClosed.
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);

  // Create the model.
  Delegate delegate;
  SimpleMenuModel model(&delegate);
  model.AddItem(1, ASCIIToUTF16("allays"));
  model.AddItem(2, ASCIIToUTF16("i"));
  model.AddItem(3, ASCIIToUTF16("bf"));

  // Create the controller.
  base::scoped_nsobject<MenuControllerCocoa> menu([[MenuControllerCocoa alloc]
               initWithModel:&model
      useWithPopUpButtonCell:NO]);
  delegate.menu_to_close_ = [menu menu];

  EXPECT_FALSE([menu isMenuOpen]);

  // In the event tracking run loop mode of the menu, verify that the controller
  // resports the menu as open.
  CFRunLoopPerformBlock(CFRunLoopGetCurrent(), NSEventTrackingRunLoopMode, ^{
    EXPECT_TRUE([menu isMenuOpen]);
  });

  // Pop open the menu, which will spin an event-tracking run loop.
  [NSMenu popUpContextMenu:[menu menu]
                 withEvent:cocoa_test_event_utils::RightMouseDownAtPoint(
                               NSZeroPoint)
                   forView:[test_window() contentView]];

  EXPECT_FALSE([menu isMenuOpen]);

  // When control returns back to here, the menu will have finished running its
  // loop and will have closed itself (see Delegate::OnMenuWillShow).
  EXPECT_TRUE(delegate.did_show_);

  // When the menu tells the Model it closed, the Model posts a task to notify
  // the delegate. But since this is a test and there's no running MessageLoop,
  // |did_close_| will remain false until we pump the task manually.
  EXPECT_FALSE(delegate.did_close_);

  // Pump the task that notifies the delegate.
  base::RunLoop().RunUntilIdle();

  // Expect that the delegate got notified properly.
  EXPECT_TRUE(delegate.did_close_);
}

// Verify that the private API used by MenuControllerCocoa's
// ResponsiveNSMenuItem exists in the runtime. It's not a disaster if it
// disappears, (or AppKit stops invoking it) but consumers will stop receiving
// opportunities to -processItemSelectedEarly:.
TEST_F(MenuControllerTest, SendItemSelectedNoteExists) {
  // -_sendItemSelectedNote doesn't exist on 10.9 or 10.10. NSPopUpButton menus
  // on 10.9 don't animate out, and always suffer from the brief "flash" of the
  // old selection when the menu disappears.
  // TODO(tapted): Find a hook on 10.10 if we deem it necessary.
  if (base::mac::IsAtMostOS10_10())
    return;

  EXPECT_TRUE(
      [NSMenuItem instancesRespondToSelector:@selector(_sendItemSelectedNote)]);
}

// Emulate the flow for -[MenuControllerCocoa itemWillBeSelected:] and
// processing the action via posted task during menu fade out.
TEST_F(MenuControllerTest, EmulateItemSelectedEarly) {
  if (![NSMenuItem instancesRespondToSelector:@selector(_sendItemSelectedNote)])
    return;

  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);

  Delegate delegate;
  delegate.auto_close_ = false;

  SimpleMenuModel model(&delegate);
  model.AddItem(1, ASCIIToUTF16("foo"));

  base::scoped_nsobject<TestResponsiveMenuController> controller(
      [[TestResponsiveMenuController alloc] initWithModel:&model
                                   useWithPopUpButtonCell:NO]);

  auto ResetWithPostTask = [&](BOOL post) {
    // Flush calls to OnMenuClosed() Posted by SimpleMenuModel.
    base::RunLoop().RunUntilIdle();

    [controller setPostItemSelectedAsTask:post];
    [controller setSawItemEarly:NO];
    delegate.execute_count_ = 0;
    delegate.did_show_ = delegate.did_close_ = false;
  };

  ResetWithPostTask(YES);
  NSMenuItem* item = [[controller menu] itemAtIndex:0];
  EXPECT_TRUE(item);

  [controller menuWillOpen:[controller menu]];

  // Pretend the first item got clicked. AppKit sends _sendItemSelectedNote to
  // the menu item, then performs its action.
  EXPECT_FALSE([controller sawItemEarly]);
  EXPECT_EQ(0, delegate.execute_count_);
  [item _sendItemSelectedNote];

  EXPECT_TRUE([controller sawItemEarly]);

  // Task is posted at this point, but not executed.
  EXPECT_EQ(0, delegate.execute_count_);

  // Pretend the menu is fading out, which spins a RunLoop.
  base::RunLoop().RunUntilIdle();

  // Item gets executed early.
  EXPECT_EQ(1, delegate.execute_count_);

  // Simulate dismissal. This happens before the action.
  [controller menuDidClose:[controller menu]];

  // Perform the action normally. Shouldn't get executed again.
  [[item target] performSelector:[item action] withObject:item];
  EXPECT_EQ(1, delegate.execute_count_);

  // Repeat, simulating the condition where the private API hook fails.
  ResetWithPostTask(YES);
  [controller menuWillOpen:[controller menu]];
  [controller menuDidClose:[controller menu]];
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE([controller sawItemEarly]);
  EXPECT_EQ(0, delegate.execute_count_);
  [[item target] performSelector:[item action] withObject:item];
  EXPECT_FALSE([controller sawItemEarly]);
  EXPECT_EQ(1, delegate.execute_count_);

  // Repeat, simulating the condition where events do not pump during fade out.
  ResetWithPostTask(YES);
  [controller menuWillOpen:[controller menu]];
  EXPECT_FALSE([controller sawItemEarly]);
  EXPECT_EQ(0, delegate.execute_count_);
  [item _sendItemSelectedNote];
  EXPECT_TRUE([controller sawItemEarly]);
  EXPECT_EQ(0, delegate.execute_count_);
  // No pump.
  [controller menuDidClose:[controller menu]];
  EXPECT_EQ(0, delegate.execute_count_);
  [[item target] performSelector:[item action] withObject:item];
  EXPECT_TRUE([controller sawItemEarly]);
  EXPECT_EQ(1, delegate.execute_count_);
  base::RunLoop().RunUntilIdle();  // Back the main loop.
  EXPECT_EQ(1, delegate.execute_count_);

  // Repeat, without processing early.
  ResetWithPostTask(NO);

  [controller menuWillOpen:[controller menu]];
  [item _sendItemSelectedNote];

  // Saw it, but didn't execute.
  EXPECT_TRUE([controller sawItemEarly]);
  EXPECT_EQ(0, delegate.execute_count_);

  // Even after spinning a RunLoop.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, delegate.execute_count_);

  [controller menuDidClose:[controller menu]];

  // Perform the action normally. Now executes.
  [[item target] performSelector:[item action] withObject:item];
  EXPECT_EQ(1, delegate.execute_count_);
}

// Tests invoking a menu action on a delegate that immediately releases the
// MenuControllerCocoa and destroys itself. Note this usually needs asan to
// actually crash (before it was fixed).
TEST_F(MenuControllerTest, OwningDelegate) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);
  bool did_delete = false;
  BOOL did_dealloc = NO;
  OwningDelegate* delegate;
  NSMenuItem* item;

  // The final action is a task posted to the runloop, which drains the
  // autorelease pool, so ensure that happens in the test.
  @autoreleasepool {
    delegate = new OwningDelegate(&did_delete, &did_dealloc);  // Self deleting.
    delegate->auto_close_ = false;

    // Unretained reference to the controller.
    MenuControllerCocoa* controller = delegate->controller();

    item = [[controller menu] itemAtIndex:0];
    EXPECT_TRUE(item);

    // Simulate opening the menu and selecting an item. Without setting
    // -setPostItemSelectedAsTask:YES, methods are always invoked by AppKit in
    // the following order.
    [controller menuWillOpen:[controller menu]];
    [controller menuDidClose:[controller menu]];
  }
  EXPECT_FALSE(did_dealloc);
  EXPECT_FALSE(did_delete);

  // On 10.15+, [NSMenuItem target] indirectly causes an extra
  // retain+autorelease of the target. That avoids bugs caused by the
  // NSMenuItem's action causing destruction of the target, but also causes the
  // NSMenuItem to get cleaned up later than this test expects. Deal with that
  // by creating an explicit autorelease pool here.
  @autoreleasepool {
    [[item target] performSelector:[item action] withObject:item];
  }
  EXPECT_TRUE(did_dealloc);
  EXPECT_TRUE(did_delete);
}

}  // namespace

}  // namespace ui
