// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_ITEM_VIEW_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_ITEM_VIEW_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/themed_vector_icon.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace gfx {
class FontList;
}  // namespace gfx

namespace views {

namespace internal {
class MenuRunnerImpl;
}

class ImageView;
class MenuController;
class MenuControllerTest;
class MenuDelegate;
class Separator;
class SubmenuView;
class TestMenuItemView;

// MenuItemView --------------------------------------------------------------

// MenuItemView represents a single menu item with a label and optional icon.
// Each MenuItemView may also contain a submenu, which in turn may contain
// any number of child MenuItemViews.
//
// To use a menu create an initial MenuItemView using the constructor that
// takes a MenuDelegate, then create any number of child menu items by way
// of the various AddXXX methods.
//
// MenuItemView is itself a View, which means you can add Views to each
// MenuItemView. This is normally NOT want you want, rather add other child
// Views to the submenu of the MenuItemView. Any child views of the MenuItemView
// that are focusable can be navigated to by way of the up/down arrow and can be
// activated by way of space/return keys. Activating a focusable child results
// in |AcceleratorPressed| being invoked. Note, that as menus try not to steal
// focus from the hosting window child views do not actually get focus. Instead
// |SetHotTracked| is used as the user navigates around.
//
// To show the menu use MenuRunner. See MenuRunner for details on how to run
// (show) the menu as well as for details on the life time of the menu.

class VIEWS_EXPORT MenuItemView : public View, public LayoutDelegate {
  METADATA_HEADER(MenuItemView, View)

 public:
  // Padding between child views.
  static constexpr int kChildHorizontalPadding = 8;

  // Different types of menu items.
  enum class Type {
    kNormal,             // Performs an action when selected.
    kSubMenu,            // Presents a submenu within another menu.
    kActionableSubMenu,  // A SubMenu that is also a COMMAND.
    kCheckbox,           // Can be selected/checked to toggle a boolean state.
    kRadio,              // Can be selected/checked among a group of choices.
    kSeparator,          // Shows a horizontal line separator.
    kHighlighted,        // Performs an action when selected, and has a
                         // different colored background that merges with the
                         // menu's rounded corners when placed at the bottom.
    kTitle,              // Title text, does not perform any action.
    kEmpty,              // kEmpty is a special type for empty menus that is
                         // only used internally.
  };

  // Where the menu should be drawn, above or below the bounds (when
  // the bounds is non-empty).  MenuPosition::kBestFit (default) positions
  // the menu below the bounds unless the menu does not fit on the
  // screen and the re is more space above.
  enum class MenuPosition { kBestFit, kAboveBounds, kBelowBounds };

  // The data structure which is used for the menu size
  struct MenuItemDimensions {
    MenuItemDimensions() = default;

    // Width of everything except the accelerator and children views.
    int standard_width = 0;
    // The width of all contained views of the item.
    int children_width = 0;
    // The amount of space needed to accommodate the subtext.
    int minor_text_width = 0;
    // The height of the menu item.
    int height = 0;
  };

  // The data structure which is used to paint a background on the menu item.
  struct MenuItemBackground {
    MenuItemBackground(ui::ColorId background_color_id, int corner_radius)
        : background_color_id(background_color_id),
          corner_radius(corner_radius) {}
    ui::ColorId background_color_id;
    int corner_radius = 0;
  };

  // Constructor for use with the top level menu item. This menu is never
  // shown to the user, rather its use as the parent for all menu items.
  explicit MenuItemView(MenuDelegate* delegate = nullptr);

  MenuItemView(const MenuItemView&) = delete;
  MenuItemView& operator=(const MenuItemView&) = delete;

  ~MenuItemView() override;

  // Overridden from View:
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;
  FocusBehavior GetFocusBehavior() const override;

  // Returns if a given |anchor| is a bubble or not.
  static bool IsBubble(MenuAnchorPosition anchor);

  // Returns the default accessible name to be used with screen readers.
  // Mnemonics are removed and the menu item accelerator text is appended.
  static std::u16string GetAccessibleNameForMenuItem(
      const std::u16string& item_text,
      const std::u16string& accelerator_text,
      bool is_new_feature);

  // Hides and cancels the menu. This does nothing if the menu is not open.
  void Cancel();

  // Add an item to the menu at a specified index.  ChildrenChanged() should
  // called after adding menu items if the menu may be active.
  MenuItemView* AddMenuItemAt(
      size_t index,
      int item_id,
      const std::u16string& label,
      const std::u16string& secondary_label,
      const std::u16string& minor_text,
      const ui::ImageModel& minor_icon,
      const ui::ImageModel& icon,
      Type type,
      ui::MenuSeparatorType separator_style,
      std::optional<ui::ColorId> submenu_background_color = std::nullopt,
      std::optional<ui::ColorId> foreground_color = std::nullopt,
      std::optional<ui::ColorId> selected_color_id = std::nullopt);

  void SetMenuItemBackground(
      std::optional<MenuItemBackground> menu_item_background) {
    menu_item_background_ = menu_item_background;
  }

  std::optional<MenuItemBackground> GetMenuItemBackground() {
    return menu_item_background_;
  }

  void SetSelectedColorId(std::optional<ui::ColorId> selected_color_id) {
    selected_color_id_ = selected_color_id;
  }

  std::optional<ui::ColorId> GetSelectedColorId() { return selected_color_id_; }

  void SetHighlightWhenSelectedWithChildViews(
      bool highlight_when_selected_with_child_views) {
    highlight_when_selected_with_child_views_ =
        highlight_when_selected_with_child_views;
  }

  // Remove the specified item from the menu. |item| will be deleted when
  // ChildrenChanged() is invoked.
  void RemoveMenuItem(View* item);

  // Removes all items from the menu.  The removed MenuItemViews are deleted
  // when ChildrenChanged() is invoked.
  void RemoveAllMenuItems();

  // Appends a normal item to this menu.
  // item_id    The id of the item, used to identify it in delegate callbacks
  //            or (if delegate is NULL) to identify the command associated
  //            with this item with the controller specified in the ctor. Note
  //            that this value should not be 0 as this has a special meaning
  //            ("NULL command, no item selected")
  // label      The text label shown.
  // icon       The icon.
  MenuItemView* AppendMenuItem(int item_id,
                               const std::u16string& label = std::u16string(),
                               const ui::ImageModel& icon = ui::ImageModel());

  MenuItemView* AppendTitle(const std::u16string& label);

  // Append a submenu to this menu.
  // The returned pointer is owned by this menu.
  MenuItemView* AppendSubMenu(int item_id,
                              const std::u16string& label,
                              const ui::ImageModel& icon = ui::ImageModel());

  // Adds a separator to this menu
  void AppendSeparator();

  // Adds a separator to this menu at the specified position.
  void AddSeparatorAt(size_t index);

  // All the AppendXXX methods funnel into this.
  MenuItemView* AppendMenuItemImpl(int item_id,
                                   const std::u16string& label,
                                   const ui::ImageModel& icon,
                                   Type type);

  // Returns the view that contains child menu items. If the submenu has
  // not been created, this creates it.
  SubmenuView* CreateSubmenu();

  // Returns true if this menu item has a submenu.
  bool HasSubmenu() const;

  // Returns the view containing child menu items.
  SubmenuView* GetSubmenu() const;

  // Returns true if this menu item has a submenu and it is showing
  bool SubmenuIsShowing() const;

  // Sets the identifier of the submenu, if there is one, or to a future submenu
  // that would be created.
  void SetSubmenuId(ui::ElementIdentifier submenu_id);

  // Returns the parent menu item.
  MenuItemView* GetParentMenuItem() { return parent_menu_item_; }
  const MenuItemView* GetParentMenuItem() const { return parent_menu_item_; }

  // Sets/Gets the title.
  void SetTitle(const std::u16string& title);
  const std::u16string& title() const { return title_; }

  // Sets/Gets the secondary title. When not empty, they are shown in the line
  // below the title.
  void SetSecondaryTitle(const std::u16string& secondary_title);
  const std::u16string& secondary_title() const { return secondary_title_; }

  // Sets the minor text.
  void SetMinorText(const std::u16string& minor_text);

  // Sets the minor icon.
  void SetMinorIcon(const ui::ImageModel& minor_icon);

  // Returns the type of this menu.
  const Type& GetType() const { return type_; }

  // Sets whether this item is selected. This is invoked as the user moves
  // the mouse around the menu while open.
  void SetSelected(bool selected);

  // Returns true if the item is selected.
  bool IsSelected() const { return selected_; }

  // Adds a callback subscription associated with the above selected property.
  // The callback will be invoked whenever the selected property changes.
  [[nodiscard]] base::CallbackListSubscription AddSelectedChangedCallback(
      PropertyChangedCallback callback);

  // Sets whether the submenu area of an ACTIONABLE_SUBMENU is selected.
  void SetSelectionOfActionableSubmenu(
      bool submenu_area_of_actionable_submenu_selected);

  // Whether the submenu area of an ACTIONABLE_SUBMENU is selected.
  bool IsSubmenuAreaOfActionableSubmenuSelected() const {
    return submenu_area_of_actionable_submenu_selected_;
  }

  // Sets the |tooltip| for a menu item view with |item_id| identifier.
  void SetTooltip(const std::u16string& tooltip, int item_id);

  // Sets the icon of this menu item.
  void SetIcon(const ui::ImageModel& icon);
  const ui::ImageModel GetIcon() const;

  // Sets the view used to render the icon. This clobbers any icon set via
  // SetIcon(). MenuItemView takes ownership of |icon_view|.
  void SetIconView(std::unique_ptr<ImageView> icon_view);

  ImageView* icon_view() { return icon_view_; }

  // Returns the preferred size of the icon view if any, or gfx::Size() if none.
  gfx::Size GetIconPreferredSize() const;

  // Sets the command id of this menu item.
  void SetCommand(int command);

  // Returns the command id of this item.
  int GetCommand() const { return command_; }

  void set_is_new(bool is_new) { is_new_ = is_new; }
  bool is_new() const { return is_new_; }

  void set_may_have_mnemonics(bool may_have_mnemonics) {
    may_have_mnemonics_ = may_have_mnemonics;
    UpdateAccessibleKeyShortcuts();
  }
  bool may_have_mnemonics() const { return may_have_mnemonics_; }

  // Called when the drop or selection status (as determined by SubmenuView) may
  // have changed.
  void OnDropOrSelectionStatusMayHaveChanged();

  // Paints the menu item.
  void OnPaint(gfx::Canvas* canvas) override;

  // Returns the preferred size of this item.
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override;

  // Returns the bounds of the submenu part of the ACTIONABLE_SUBMENU.
  gfx::Rect GetSubmenuAreaOfActionableSubmenu() const;

  // Return the preferred dimensions of the item in pixel.
  const MenuItemDimensions& GetDimensions() const;

  // Returns the earliest horizontal position where content may appear.
  int GetContentStart() const;

  void set_controller(MenuController* controller) {
    if (controller) {
      controller_ = controller->AsWeakPtr();
    } else {
      controller_.reset();
    }
  }
  MenuController* GetMenuController();
  const MenuController* GetMenuController() const;

  // Returns the delegate. This returns the delegate of the root menu item.
  MenuDelegate* GetDelegate();
  const MenuDelegate* GetDelegate() const;
  void set_delegate(MenuDelegate* delegate) { delegate_ = delegate; }

  // Returns the root parent, or this if this has no parent.
  MenuItemView* GetRootMenuItem();
  const MenuItemView* GetRootMenuItem() const;

  // Returns the mnemonic for this MenuItemView, or 0 if this MenuItemView
  // doesn't have a mnemonic.
  char16_t GetMnemonic();

  // Returns the descendant with the specified command.
  MenuItemView* GetMenuItemByID(int id);

  // Invoke if you remove/add children to the menu while it's showing. This
  // recalculates the bounds.
  void ChildrenChanged();

  // Overridden from LayoutDelegate:
  ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const override;

  // Returns true if the menu has mnemonics. This only useful on the root menu
  // item.
  bool has_mnemonics() const { return has_mnemonics_; }

  void set_vertical_margin(int vertical_margin) {
    vertical_margin_ = vertical_margin;
    invalidate_dimensions();
  }

  void set_children_use_full_width(bool children_use_full_width) {
    children_use_full_width_ = children_use_full_width;
  }

  // Controls whether this menu has a forced visual selection state. This is
  // used when animating item acceptance on Mac. Note that once this is set
  // there's no way to unset it for this MenuItemView!
  void SetForcedVisualSelection(bool selected);

  // For items of type HIGHLIGHTED only: sets the radius of the item's
  // background. This makes the menu item's background fit its container's
  // border radius, if they are both the same value.
  void SetCornerRadius(int radius);

  // Shows an alert on this menu item. An alerted menu item is rendered
  // differently to draw attention to it. This must be called before the menu is
  // run.
  void SetAlerted();
  bool is_alerted() const { return is_alerted_; }

  // Returns whether or not a "new" badge should be shown on this menu item.
  bool ShouldShowNewBadge() const;

  // Returns whether keyboard navigation through the menu should stop on this
  // item.
  bool IsTraversableByKeyboard() const;

  // Returns the corresponding border padding from the `MenuConfig`.
  int GetItemHorizontalBorder() const;

  virtual void UpdateAccessibleCheckedState();

  void SetTriggerActionWithNonIconChildViews(
      bool trigger_action_with_non_icon_child_views) {
    trigger_action_with_non_icon_child_views_ =
        trigger_action_with_non_icon_child_views;
  }

  bool GetTriggerActionWithNonIconChildViews() const {
    return trigger_action_with_non_icon_child_views_;
  }

  bool last_paint_as_selected_for_testing() const {
    return last_paint_as_selected_;
  }

  static std::u16string GetNewBadgeAccessibleDescription();

 protected:
  // Creates a MenuItemView. This is used by the various AddXXX methods.
  MenuItemView(MenuItemView* parent, int command, Type type);

  // View:
  void ChildPreferredSizeChanged(View* child) override;
  void OnThemeChanged() override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

  // Returns the preferred size (and padding) of any children.
  virtual gfx::Size GetChildPreferredSize() const;

  // Returns the various margins.
  int GetTopMargin() const;
  int GetBottomMargin() const;

 private:
  friend class MenuController;
  friend class internal::MenuRunnerImpl;
  friend class MenuControllerTest;
  friend class TestMenuItemView;
  FRIEND_TEST_ALL_PREFIXES(MenuControllerTest, RepostEventToEmptyMenuItem);

  enum class PaintMode { kNormal, kForDrag };

  // The set of colors used in painting the MenuItemView.
  struct Colors {
    SkColor fg_color = SK_ColorTRANSPARENT;
    SkColor icon_color = SK_ColorTRANSPARENT;
    SkColor minor_fg_color = SK_ColorTRANSPARENT;
  };

  MenuItemView(MenuItemView* parent,
               int command,
               Type type,
               MenuDelegate* delegate);

  const SubmenuView* GetContainingSubmenu() const {
    return parent_menu_item_->GetSubmenu();
  }

  // The RunXXX methods call into this to set up the necessary state before
  // running.
  void PrepareForRun(bool has_mnemonics, bool show_mnemonics);

  // Returns the flags passed to DrawStringRect.
  int GetDrawStringFlags() const;

  // Returns the font list and font color to use for menu text.
  const gfx::FontList GetFontList() const;
  const std::optional<SkColor> GetMenuLabelColor() const;

  // Ensures the submenu has an empty menu item iff it needs one, then updates
  // its metrics.
  void UpdateEmptyMenusAndMetrics();

  // Given bounds within our View, this helper routine mirrors the bounds if
  // necessary.
  void AdjustBoundsForRTLUI(gfx::Rect* rect) const;

  // Paints the MenuItemView for a drag operation.
  void PaintForDrag(gfx::Canvas* canvas);

  // Actual paint implementation.
  void OnPaintImpl(gfx::Canvas* canvas, PaintMode mode);

  // Helper function for OnPaintImpl() that is responsible for drawing the
  // background.
  void PaintBackground(gfx::Canvas* canvas,
                       PaintMode mode,
                       bool paint_as_selected);

  // Paints the right-side icon and text.
  void PaintMinorIconAndText(gfx::Canvas* canvas, SkColor color);

  // Destroys the window used to display this menu and recursively destroys
  // the windows used to display all descendants.
  void DestroyAllMenuHosts();

  // Returns the text that should be displayed on the end (right) of the menu
  // item. This will be the accelerator (if one exists).
  std::u16string GetMinorText() const;

  // Returns the icon that should be displayed to the left of the minor text.
  ui::ImageModel GetMinorIcon() const;

  // Returns the text color for the current state.  |minor| specifies if the
  // minor text or the normal text is desired.
  SkColor GetTextColor(bool minor, bool paint_as_selected) const;

  // Returns the colors used in painting.
  Colors CalculateColors(bool paint_as_selected) const;

  // Calculates and returns the accessible name for this menu item.
  std::u16string CalculateAccessibleName() const;

  // Calculates and returns the MenuItemDimensions.
  MenuItemDimensions CalculateDimensions() const;

  // Imposes MenuConfig's minimum sizes, if any, on the supplied
  // dimensions and returns the new dimensions. It is guaranteed that:
  //    ApplyMinimumDimensions(x).standard_width >= x.standard_width
  //    ApplyMinimumDimensions(x).children_width == x.children_width
  //    ApplyMinimumDimensions(x).minor_text_width == x.minor_text_width
  //    ApplyMinimumDimensions(x).height >= x.height
  void ApplyMinimumDimensions(MenuItemDimensions* dims) const;

  // Given a proposed `height` for this item, returns the height after ensuring
  // it reserves sufficient icon height.
  int ApplyMinIconHeight(int height) const;

  // Get the horizontal position at which to draw the menu item's label.
  int GetLabelStartForThisItem() const;

  // Used by MenuController to cache the menu position in use by the
  // active menu.
  MenuPosition actual_menu_position() const { return actual_menu_position_; }
  void set_actual_menu_position(MenuPosition actual_menu_position) {
    actual_menu_position_ = actual_menu_position;
  }

  // Returns true if this MenuItemView contains a single child
  // that is responsible for rendering the content.
  bool IsContainer() const;

  // Gets the child view margins. Should only be called when |IsContainer()| is
  // true.
  gfx::Insets GetContainerMargins() const;

  // Returns number of child views excluding icon_view.
  int NonIconChildViewsCount() const;

  void invalidate_dimensions() { dimensions_.height = 0; }
  bool is_dimensions_valid() const { return dimensions_.height > 0; }

  // Calls UpdateSelectionBasedState() if the the selection state changed.
  void UpdateSelectionBasedStateIfChanged(PaintMode mode);

  // Updates any state that may changed based on the selection state.
  void UpdateSelectionBasedState(bool should_paint_as_selected);

  // Returns true if the MenuItemView should be painted as selected.
  bool ShouldPaintAsSelected(PaintMode mode) const;

  // Returns true if this item or any anscestor menu items have been removed and
  // are currently scheduled for deletion. Items can exist in this state after
  // they have been removed from the menu but before ChildrenChanged() has been
  // called.
  // Menu items scheduled for deletion may not accurately reflect the state of
  // the menu model and this should be checked when performing actions that
  // could interact with model state.
  bool IsScheduledForDeletion() const;

  void SetForegroundColorId(std::optional<ui::ColorId> foreground_color_id) {
    foreground_color_id_ = foreground_color_id;
  }

  // Returns the corresponding margin from the `MenuConfig` if
  // `vertical_margin_` is not set.
  int GetVerticalMargin() const;

  ViewAccessibility* GetSubmenuViewAccessibility();
  ViewAccessibility* GetScrollViewContainerViewAccessibility();
  void UpdateAccessibleRole();
  void UpdateAccessibleHasPopup();
  void UpdateAccessibleName();
  void UpdateAccessibleSelection();
  void UpdateAccessibleKeyShortcuts();

  // The delegate. This is only valid for the root menu item. You shouldn't
  // use this directly, instead use GetDelegate() which walks the tree as
  // as necessary.
  raw_ptr<MenuDelegate> delegate_ = nullptr;

  // The controller for the run operation, or NULL if the menu isn't showing.
  base::WeakPtr<MenuController> controller_;

  // Used to detect when Cancel was invoked.
  bool canceled_ = false;

  // Our parent.
  const raw_ptr<MenuItemView> parent_menu_item_ = nullptr;

  // Type of menu. NOTE: MenuItemView doesn't itself represent SEPARATOR,
  // that is handled by an entirely different view class.
  const Type type_;

  // Whether we're selected.
  bool selected_ = false;

  bool last_paint_as_selected_ = false;

  // Whether the submenu area of an ACTIONABLE_SUBMENU is selected.
  bool submenu_area_of_actionable_submenu_selected_ = false;

  // Command id.
  int command_ = 0;

  // Whether the menu item should be badged as "New" as a way to highlight a new
  // feature for users.
  bool is_new_ = false;

  // Whether the menu item contains user-created text.
  bool may_have_mnemonics_ = true;

  // Submenu, created via `CreateSubmenu`.
  std::unique_ptr<SubmenuView> submenu_;

  // Identifier to assign to a submenu if one is created.
  ui::ElementIdentifier submenu_id_;

  std::u16string title_;
  std::u16string secondary_title_;
  std::u16string minor_text_;
  ui::ImageModel minor_icon_;

  // Does the title have a mnemonic? Only useful on the root menu item.
  bool has_mnemonics_ = false;

  // Should we show the mnemonic? Mnemonics are shown if this is true or
  // MenuConfig says mnemonics should be shown. Only used on the root menu item.
  bool show_mnemonics_ = false;

  // Pointer to a view with a menu icon.
  raw_ptr<ImageView> icon_view_ = nullptr;

  // The tooltip to show on hover for this menu item.
  std::u16string tooltip_;

  // Cached dimensions. This is cached as text sizing calculations are quite
  // costly.
  mutable MenuItemDimensions dimensions_;

  // Removed items to be deleted in ChildrenChanged().
  std::vector<raw_ptr<View, VectorExperimental>> removed_items_;

  std::optional<int> vertical_margin_;

  // Corner radius in pixels, for HIGHLIGHTED items placed at the end of a menu.
  int corner_radius_ = 0;

  // |menu_position_| is the requested position with respect to the bounds.
  // |actual_menu_position_| is used by the controller to cache the
  // position of the menu being shown.
  MenuPosition requested_menu_position_ = MenuPosition::kBestFit;
  MenuPosition actual_menu_position_ = MenuPosition::kBestFit;

  // If set to true, children beyond the normal icon/labels/arrow will be laid
  // out taking the full width of the menu, instead of stopping at any arrow
  // column.
  bool children_use_full_width_ = false;

  // Default implementation will not trigger a MenuItemAction if there are child
  // views other than the icon view. `trigger_action_with_non_icon_child_views_`
  // specifies that we should still trigger the action even if we have non icon
  // child views if other conditions are met as well.
  bool trigger_action_with_non_icon_child_views_ = false;

  // Contains an image for the checkbox or radio icon.
  raw_ptr<ImageView> radio_check_image_view_ = nullptr;

  // The submenu indicator arrow icon in case the menu item has a Submenu.
  raw_ptr<ImageView> submenu_arrow_image_view_ = nullptr;

  // The forced visual selection state of this item, if any.
  std::optional<bool> forced_visual_selection_;

  // The vertical separator that separates the actionable and submenu regions of
  // an ACTIONABLE_SUBMENU.
  raw_ptr<Separator> vertical_separator_ = nullptr;

  // Whether this menu item is rendered differently to draw attention to it.
  bool is_alerted_ = false;

  // Legacy implementation for menu items is that if a MenuItemView has a child
  // view then the item will not be highlighted when selected. This new boolean
  // will control whether or not the MenuItemView is highlighted when there are
  // child views.
  bool highlight_when_selected_with_child_views_ = false;

  // If true, ViewHierarchyChanged() will call
  // UpdateSelectionBasedStateIfChanged().
  // UpdateSelectionBasedStateIfChanged() calls to NonIconChildViewsCount().
  // NonIconChildViewsCount() accesses fields of type View as part of the
  // implementation. A common pattern for assigning a field is:
  // icon_view_ = AddChildView(icon_view);
  // The problem is ViewHierarchyChanged() is called during AddChildView() and
  // before `icon_view_` is set. This means NonIconChildViewsCount() may return
  // the wrong thing. In this case
  // `update_selection_based_state_in_view_herarchy_changed_` is set to false
  // and SetIconView() explicitly calls UpdateSelectionBasedStateIfChanged().
  bool update_selection_based_state_in_view_herarchy_changed_ = true;

  const std::u16string new_badge_text_ =
      l10n_util::GetStringUTF16(IDS_NEW_BADGE);

  std::optional<ui::ColorId> foreground_color_id_;
  std::optional<MenuItemBackground> menu_item_background_;
  std::optional<ui::ColorId> selected_color_id_;

  base::CallbackListSubscription visible_changed_callback_;
  base::CallbackListSubscription enabled_changed_callback_;
};

// EmptyMenuMenuItem ----------------------------------------------------------

// EmptyMenuMenuItem is used when a menu has no menu items.

class VIEWS_EXPORT EmptyMenuMenuItem : public MenuItemView {
  METADATA_HEADER(EmptyMenuMenuItem, MenuItemView)

 public:
  explicit EmptyMenuMenuItem(MenuItemView* parent);
  EmptyMenuMenuItem(const EmptyMenuMenuItem&) = delete;
  EmptyMenuMenuItem& operator=(const EmptyMenuMenuItem&) = delete;
  ~EmptyMenuMenuItem() override = default;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_ITEM_VIEW_H_
