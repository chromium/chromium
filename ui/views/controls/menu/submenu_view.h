// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_SUBMENU_VIEW_H_
#define UI_VIEWS_CONTROLS_MENU_SUBMENU_VIEW_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/views/animation/scroll_animator.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/prefix_delegate.h"
#include "ui/views/controls/prefix_selector.h"
#include "ui/views/view.h"

namespace views {

class MenuHost;
class MenuItemView;
class MenuScrollViewContainer;

namespace test {
class MenuControllerTest;
}  // test

// SubmenuView is the parent of all menu items.
//
// SubmenuView has the following responsibilities:
// . It positions and sizes all child views (any type of View may be added,
//   not just MenuItemViews).
// . Forwards the appropriate events to the MenuController. This allows the
//   MenuController to update the selection as the user moves the mouse around.
// . Renders the drop indicator during a drop operation.
// . Shows and hides the window (a NativeWidget) when the menu is shown on
//   screen.
//
// SubmenuView is itself contained in a MenuScrollViewContainer.
// MenuScrollViewContainer handles showing as much of the SubmenuView as the
// screen allows. If the SubmenuView is taller than the screen, scroll buttons
// are provided that allow the user to see all the menu items.
class VIEWS_EXPORT SubmenuView : public View,
                                 public PrefixDelegate,
                                 public ScrollDelegate {
 public:
  METADATA_HEADER(SubmenuView);

  using MenuItems = std::vector<MenuItemView*>;

  // Creates a SubmenuView for the specified menu item.
  explicit SubmenuView(MenuItemView* parent);
  ~SubmenuView() override;

  // Returns true if the submenu has at least one empty menu item.
  bool HasEmptyMenuItemView() const;

  // Returns true if the submenu has at least one visible child item.
  bool HasVisibleChildren() const;

  // Returns the children which are menu items.
  MenuItems GetMenuItems() const;

  // Returns the MenuItemView at the specified index.
  MenuItemView* GetMenuItemAt(int index);

  PrefixSelector* GetPrefixSelector();

  // Positions and sizes the child views. This tiles the views vertically,
  // giving each child the available width.
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;

  // Override from View.
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // Painting.
  void PaintChildren(const PaintInfo& paint_info) override;

  // Drag and drop methods. These are forwarded to the MenuController.
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const OSExchangeData& data) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;

  // Scrolls on menu item boundaries.
  bool OnMouseWheel(const ui::MouseWheelEvent& e) override;

  // Overridden from ui::EventHandler.
  // Scrolls on menu item boundaries.
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Overridden from PrefixDelegate.
  int GetRowCount() override;
  int GetSelectedRow() override;
  void SetSelectedRow(int row) override;
  base::string16 GetTextForRow(int row) override;

  // Returns true if the menu is showing.
  virtual bool IsShowing() const;

  // Shows the menu at the specified location. Coordinates are in screen
  // coordinates. max_width gives the max width the view should be.
  void ShowAt(Widget* parent, const gfx::Rect& bounds, bool do_capture);

  // Resets the bounds of the submenu to |bounds|.
  void Reposition(const gfx::Rect& bounds);

  // Closes the menu, destroying the host.
  void Close();

  // Hides the hosting window.
  //
  // The hosting window is hidden first, then deleted (Close) when the menu is
  // done running. This is done to avoid deletion ordering dependencies. In
  // particular, during drag and drop (and when a modal dialog is shown as
  // a result of choosing a context menu) it is possible that an event is
  // being processed by the host, so that host is on the stack when we need to
  // close the window. If we closed the window immediately (and deleted it),
  // when control returned back to host we would crash as host was deleted.
  void Hide();

  // If mouse capture was grabbed, it is released. Does nothing if mouse was
  // not captured.
  void ReleaseCapture();

  // Overriden from View to prevent tab from doing anything.
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& e) override;

  // Returns the parent menu item we're showing children for.
  MenuItemView* GetMenuItem();

  // Set the drop item and position.
  void SetDropMenuItem(MenuItemView* item,
                       MenuDelegate::DropPosition position);

  // Returns whether the selection should be shown for the specified item.
  // The selection is NOT shown during drag and drop when the drop is over
  // the menu.
  bool GetShowSelection(MenuItemView* item);

  // Returns the container for the SubmenuView.
  MenuScrollViewContainer* GetScrollViewContainer();

  // Returns the last MenuItemView in this submenu.
  MenuItemView* GetLastItem();

  // Invoked if the menu is prematurely destroyed. This can happen if the window
  // closes while the menu is shown. If invoked the SubmenuView must drop all
  // references to the MenuHost as the MenuHost is about to be deleted.
  void MenuHostDestroyed();

  // Max width of minor text (accelerator or subtitle) in child menu items. This
  // doesn't include children's children, only direct children.
  int max_minor_text_width() const { return max_minor_text_width_; }

  // Minimum width of menu in pixels (default 0).  This becomes the smallest
  // width returned by GetPreferredSize().
  void set_minimum_preferred_width(int minimum_preferred_width) {
    minimum_preferred_width_ = minimum_preferred_width;
  }

  // Automatically resize menu if a subview's preferred size changes.
  bool resize_open_menu() const { return resize_open_menu_; }
  void set_resize_open_menu(bool resize_open_menu) {
    resize_open_menu_ = resize_open_menu;
  }

 protected:

  // View method. Overridden to schedule a paint. We do this so that when
  // scrolling occurs, everything is repainted correctly.
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  void ChildPreferredSizeChanged(View* child) override;

 private:
  friend class test::MenuControllerTest;

  void SchedulePaintForDropIndicator(MenuItemView* item,
                                     MenuDelegate::DropPosition position);

  // Calculates the location of th edrop indicator.
  gfx::Rect CalculateDropIndicatorBounds(MenuItemView* item,
                                         MenuDelegate::DropPosition position);

  // Implementation of ScrollDelegate
  bool OnScroll(float dx, float dy) override;

  // Parent menu item.
  MenuItemView* parent_menu_item_;

  // Widget subclass used to show the children. This is deleted when we invoke
  // |DestroyMenuHost|, or |MenuHostDestroyed| is invoked back on us.
  MenuHost* host_;

  // If non-null, indicates a drop is in progress and drop_item is the item
  // the drop is over.
  MenuItemView* drop_item_;

  // Position of the drop.
  MenuDelegate::DropPosition drop_position_;

  // Ancestor of the SubmenuView, lazily created.
  MenuScrollViewContainer* scroll_view_container_;

  // See description above getter.
  mutable int max_minor_text_width_;

  // Minimum width returned in GetPreferredSize().
  int minimum_preferred_width_;

  // Reposition open menu when contained views change size.
  bool resize_open_menu_;

  // The submenu's scroll animator
  std::unique_ptr<ScrollAnimator> scroll_animator_;

  // Difference between current position and cumulative deltas passed to
  // OnScroll.
  // TODO(tdresser): This should be removed when raw pixel scrolling for views
  // is enabled. See crbug.com/329354.
  float roundoff_error_;

  PrefixSelector prefix_selector_;

  DISALLOW_COPY_AND_ASSIGN(SubmenuView);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_SUBMENU_VIEW_H_
