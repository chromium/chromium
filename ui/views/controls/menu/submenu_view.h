// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_SUBMENU_VIEW_H_
#define UI_VIEWS_CONTROLS_MENU_SUBMENU_VIEW_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/views/animation/scroll_animator.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_host.h"
#include "ui/views/controls/prefix_delegate.h"
#include "ui/views/controls/prefix_selector.h"
#include "ui/views/view.h"

namespace ui {
struct OwnedWindowAnchor;
}  // namespace ui

namespace views {

class MenuControllerTest;
class MenuItemView;
class MenuScrollViewContainer;

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
  METADATA_HEADER(SubmenuView, View)

 public:
  // Creates a SubmenuView for the specified menu item.
  explicit SubmenuView(MenuItemView* parent);

  SubmenuView(const SubmenuView&) = delete;
  SubmenuView& operator=(const SubmenuView&) = delete;

  ~SubmenuView() override;

  // Returns the children which are menu items.
  std::vector<MenuItemView*> GetMenuItems();
  std::vector<const MenuItemView*> GetMenuItems() const;

  // Returns the MenuItemView at the specified index.
  MenuItemView* GetMenuItemAt(size_t index);

  // The preferred height, in DIPs, of a "standard" (i.e. empty) menu item.
  int GetPreferredItemHeight() const;

  PrefixSelector* GetPrefixSelector();

  // Sets various menu metrics based on the current children. For example, this
  // reserves space for menu icons iff any children have icons.
  void UpdateMenuPartSizes();

  // Positions and sizes the child views. This tiles the views vertically,
  // giving each child the available width.
  void Layout(PassKey) override;

  // TODO(crbug.com/40232718): Respect `available_size`.
  gfx::Size CalculatePreferredSize(
      const SizeBounds& /*available_size*/) const override;

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
  views::View::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override;

  // Scrolls on menu item boundaries.
  bool OnMouseWheel(const ui::MouseWheelEvent& e) override;

  // Overridden from ui::EventHandler.
  // Scrolls on menu item boundaries.
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Overridden from PrefixDelegate.
  size_t GetRowCount() override;
  std::optional<size_t> GetSelectedRow() override;
  void SetSelectedRow(std::optional<size_t> row) override;
  std::u16string GetTextForRow(size_t row) override;

  // Returns true if the menu is showing.
  virtual bool IsShowing() const;

  // Shows the menu using the specified |init_params|. |init_params.bounds| are
  // in screen coordinates.
  void ShowAt(const MenuHost::InitParams& init_params);

  // Resets the bounds of the submenu to |bounds| and its anchor to |anchor|.
  void Reposition(const gfx::Rect& bounds, const ui::OwnedWindowAnchor& anchor);

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
  const MenuItemView* GetMenuItem() const;
  MenuItemView* GetMenuItem() {
    return const_cast<MenuItemView*>(std::as_const(*this).GetMenuItem());
  }

  // Set the drop item and position.
  void SetDropMenuItem(MenuItemView* item, MenuDelegate::DropPosition position);

  // Returns whether the selection should be shown for the specified item.
  // The selection is NOT shown during drag and drop when the drop is over
  // the menu.
  bool GetShowSelection(const MenuItemView* item) const;

  // Returns the container for the SubmenuView.
  MenuScrollViewContainer* GetScrollViewContainer();

  // Returns the last MenuItemView in this submenu.
  MenuItemView* GetLastItem();

  // Invoked if the menu is prematurely destroyed. This can happen if the window
  // closes while the menu is shown. If invoked the SubmenuView must drop all
  // references to the MenuHost as the MenuHost is about to be deleted.
  void MenuHostDestroyed();

  int icon_area_width() const { return icon_area_width_; }
  int min_icon_height() const { return min_icon_height_; }
  int label_start() const { return label_start_; }
  int trailing_padding() const { return trailing_padding_; }

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
  MenuHost* host() { return host_; }
  const MenuHost* host() const { return host_; }

  void SetBorderColorId(std::optional<ui::ColorId> color_id);

 protected:
  // View method. Overridden to schedule a paint. We do this so that when
  // scrolling occurs, everything is repainted correctly.
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  void ChildPreferredSizeChanged(View* child) override;

 private:
  friend class MenuControllerTest;

  void SchedulePaintForDropIndicator(MenuItemView* item,
                                     MenuDelegate::DropPosition position);

  // Calculates the location of th edrop indicator.
  gfx::Rect CalculateDropIndicatorBounds(MenuItemView* item,
                                         MenuDelegate::DropPosition position);

  // Implementation of ScrollDelegate
  bool OnScroll(float dx, float dy) override;

  // Parent menu item.
  raw_ptr<MenuItemView> parent_menu_item_;

  // Widget subclass used to show the children. This is deleted when we invoke
  // |DestroyMenuHost|, or |MenuHostDestroyed| is invoked back on us.
  raw_ptr<MenuHost> host_ = nullptr;

  // If non-null, indicates a drop is in progress and drop_item is the item
  // the drop is over.
  raw_ptr<MenuItemView> drop_item_ = nullptr;

  // Position of the drop.
  MenuDelegate::DropPosition drop_position_ = MenuDelegate::DropPosition::kNone;

  // Ancestor of the SubmenuView, lazily created.
  std::unique_ptr<MenuScrollViewContainer> scroll_view_container_;

  // Width of a menu icon area.
  int icon_area_width_ = 0;

  // The minimum height items should reserve for icons. If any item has icons,
  // checks, or radios, this is set to kMenuCheckSize, which is also the
  // common-case size for icons. This ensures that
  //   * When no items have icons etc., we don't add unnecessary padding.
  //   * When some items have icons, we make ~all items "the same size"; but --
  //   * If any items have especially large icons, we don't add _too_ much
  //     padding to every item.
  // In other words, this tries to "have roughly consistent height" without
  // incurring a lot of extra padding that makes the menu look spaced-out.
  int min_icon_height_ = 0;

  // X-coordinate of where the label starts.
  int label_start_ = 0;

  // The width of the padding after the minor text. If there is a dedicated
  // submenu arrow column, it fits inside this.
  int trailing_padding_ = 0;

  // See description above getter.
  mutable int max_minor_text_width_ = 0;

  // Minimum width returned in GetPreferredSize().
  int minimum_preferred_width_ = 0;

  // Reposition open menu when contained views change size.
  bool resize_open_menu_ = false;

  // The submenu's scroll animator
  std::unique_ptr<ScrollAnimator> scroll_animator_{
      std::make_unique<ScrollAnimator>(this)};

  // Difference between current position and cumulative deltas passed to
  // OnScroll.
  // TODO(tdresser): This should be removed when raw pixel scrolling for views
  // is enabled. See crbug.com/329354.
  float roundoff_error_ = 0;

  PrefixSelector prefix_selector_{this, this};

  std::optional<ui::ColorId> border_color_id_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_SUBMENU_VIEW_H_
