// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_DELEGATE_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_DELEGATE_H_

#include <set>
#include <string>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/font_list.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/views_export.h"

using ui::OSExchangeData;

namespace gfx {
class FontList;
class Point;
}

namespace ui {
class Accelerator;
class DropTargetEvent;
}

namespace views {

class MenuButton;
class MenuItemView;

// MenuDelegate --------------------------------------------------------------

// Delegate for a menu. This class is used as part of MenuItemView, see it
// for details.
// TODO(sky): merge this with ui::MenuModel.
class VIEWS_EXPORT MenuDelegate {
 public:
  // Used during drag and drop to indicate where the drop indicator should
  // be rendered.
  enum class DropPosition {
    kUnknow = -1,

    // Indicates a drop is not allowed here.
    kNone,

    // Indicates the drop should occur before the item.
    kBefore,

    // Indicates the drop should occur after the item.
    kAfter,

    // Indicates the drop should occur on the item.
    kOn
  };

  // Used when indicating the style for a given label.
  struct LabelStyle {
    gfx::FontList font_list;
    SkColor foreground;
  };

  virtual ~MenuDelegate();

  // Whether or not an item should be shown as checked. This is invoked for
  // radio buttons and check buttons.
  virtual bool IsItemChecked(int id) const;

  // The string shown for the menu item. This is only invoked when an item is
  // added with an empty label.
  virtual base::string16 GetLabel(int id) const;

  // The style for the label with the given |id|. Implementations may update any
  // parts of |style| or leave it unmodified.
  virtual void GetLabelStyle(int id, LabelStyle* style) const;

  // The tooltip shown for the menu item. This is invoked when the user
  // hovers over the item, and no tooltip text has been set for that item.
  virtual base::string16 GetTooltipText(int id,
                                        const gfx::Point& screen_loc) const;

  // If there is an accelerator for the menu item with id |id| it is set in
  // |accelerator| and true is returned.
  virtual bool GetAccelerator(int id, ui::Accelerator* accelerator) const;

  // Shows the context menu with the specified id. This is invoked when the
  // user does the appropriate gesture to show a context menu. The id
  // identifies the id of the menu to show the context menu for.
  // is_mouse_gesture is true if this is the result of a mouse gesture.
  // If this is not the result of a mouse gesture |p| is the recommended
  // location to display the content menu at. In either case, |p| is in
  // screen coordinates.
  // Returns true if a context menu was displayed, otherwise false
  virtual bool ShowContextMenu(MenuItemView* source,
                               int id,
                               const gfx::Point& p,
                               ui::MenuSourceType source_type);

  // Controller
  virtual bool SupportsCommand(int id) const;
  virtual bool IsCommandEnabled(int id) const;
  virtual bool IsCommandVisible(int id) const;
  virtual bool GetContextualLabel(int id, base::string16* out) const;
  virtual void ExecuteCommand(int id) {
  }

  // If nested menus are showing (nested menus occur when a menu shows a context
  // menu) this is invoked to determine if all the menus should be closed when
  // the user selects the menu with the command |id|. This returns true to
  // indicate that all menus should be closed. Return false if only the
  // context menu should be closed.
  virtual bool ShouldCloseAllMenusOnExecute(int id);

  // Executes the specified command. mouse_event_flags give the flags of the
  // mouse event that triggered this to be invoked (ui::MouseEvent
  // flags). mouse_event_flags is 0 if this is triggered by a user gesture
  // other than a mouse event.
  virtual void ExecuteCommand(int id, int mouse_event_flags);

  // Returns true if ExecuteCommand() should be invoked while leaving the
  // menu open. Default implementation returns false.
  virtual bool ShouldExecuteCommandWithoutClosingMenu(int id,
                                                      const ui::Event& e);

  // Returns true if the specified event is one the user can use to trigger, or
  // accept, the item. Defaults to left or right mouse buttons or tap.
  virtual bool IsTriggerableEvent(MenuItemView* view, const ui::Event& e);

  // Invoked to determine if drops can be accepted for a submenu. This is
  // ONLY invoked for menus that have submenus and indicates whether or not
  // a drop can occur on any of the child items of the item. For example,
  // consider the following menu structure:
  //
  // A
  //   B
  //   C
  //
  // Where A has a submenu with children B and C. This is ONLY invoked for
  // A, not B and C.
  //

  // To restrict which children can be dropped on override GetDropOperation.
  virtual bool CanDrop(MenuItemView* menu, const OSExchangeData& data);

  // See view for a description of this method.
  virtual bool GetDropFormats(MenuItemView* menu,
                              int* formats,
                              std::set<ui::ClipboardFormatType>* format_types);

  // See view for a description of this method.
  virtual bool AreDropTypesRequired(MenuItemView* menu);

  // Returns the drop operation for the specified target menu item. This is
  // only invoked if CanDrop returned true for the parent menu. position
  // is set based on the location of the mouse, reset to specify a different
  // position.
  //
  // If a drop should not be allowed, returned ui::DragDropTypes::DRAG_NONE.
  virtual int GetDropOperation(MenuItemView* item,
                               const ui::DropTargetEvent& event,
                               DropPosition* position);

  // Invoked to perform the drop operation. This is ONLY invoked if CanDrop()
  // returned true for the parent menu item, and GetDropOperation() returned an
  // operation other than ui::DragDropTypes::DRAG_NONE.
  //
  // |menu| is the menu the drop occurred on.
  virtual int OnPerformDrop(MenuItemView* menu,
                            DropPosition position,
                            const ui::DropTargetEvent& event);

  // Invoked to determine if it is possible for the user to drag the specified
  // menu item.
  virtual bool CanDrag(MenuItemView* menu);

  // Invoked to write the data for a drag operation to data. sender is the
  // MenuItemView being dragged.
  virtual void WriteDragData(MenuItemView* sender, OSExchangeData* data);

  // Invoked to determine the drag operations for a drag session of sender.
  // See DragDropTypes for possible values.
  virtual int GetDragOperations(MenuItemView* sender);

  // Returns true if the menu should close upon a drag completing. Defaults to
  // true. This is only invoked for drag and drop operations performed on child
  // Views that are not MenuItemViews.
  virtual bool ShouldCloseOnDragComplete();

  // Notification the menu has closed. This will not be called if MenuRunner is
  // deleted during calls to ExecuteCommand().
  virtual void OnMenuClosed(MenuItemView* menu) {}

  // If the user drags the mouse outside the bounds of the menu the delegate
  // is queried for a sibling menu to show. If this returns non-null the
  // current menu is hidden, and the menu returned from this method is shown.
  //
  // The delegate owns the returned menu, not the controller.
  virtual MenuItemView* GetSiblingMenu(MenuItemView* menu,
                                       const gfx::Point& screen_point,
                                       MenuAnchorPosition* anchor,
                                       bool* has_mnemonics,
                                       MenuButton** button);

  // Returns the max width menus can grow to be.
  virtual int GetMaxWidthForMenu(MenuItemView* menu);

  // Invoked prior to a menu being shown.
  virtual void WillShowMenu(MenuItemView* menu);

  // Invoked prior to a menu being hidden.
  virtual void WillHideMenu(MenuItemView* menu);

  // Returns additional horizontal spacing for the icon of the given item.
  // The |command_id| specifies the item of interest, the |icon_size| tells the
  // function the size of the icon and it will then return |left_margin|
  // and |right_margin| accordingly. Note: Negative values can be returned.
  virtual void GetHorizontalIconMargins(int command_id,
                                        int icon_size,
                                        int* left_margin,
                                        int* right_margin) const;
  // Returns true if the labels should reserve additional spacing for e.g.
  // submenu indicators at the end of the line.
  virtual bool ShouldReserveSpaceForSubmenuIndicator() const;

  // Returns true if menus should fall back to positioning beside the anchor,
  // rather than directly above or below it, when the menu is too tall to fit
  // within the screen.
  virtual bool ShouldTryPositioningBesideAnchor() const;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_DELEGATE_H_
