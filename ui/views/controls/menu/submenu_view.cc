// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/submenu_view.h"

#include <algorithm>
#include <numeric>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/owned_window_anchor.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_host.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/controls/menu/menu_separator.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// Height of the drop indicator. This should be an even number.
constexpr int kDropIndicatorHeight = 2;

template <typename MIV, typename V>
std::vector<MIV*> GetMenuItemsFromChildren(const View::Views& children) {
  std::vector<MIV*> menu_items;
  base::ranges::transform(
      children, std::back_inserter(menu_items),
      static_cast<MIV* (*)(V*)>(&AsViewClass<MenuItemView>));
  std::erase_if(menu_items, [](MIV* item) {
    return !item || IsViewClass<EmptyMenuMenuItem>(item);
  });
  return menu_items;
}

}  // namespace

SubmenuView::SubmenuView(MenuItemView* parent) : parent_menu_item_(parent) {
  CHECK(parent_menu_item_);
  // We'll delete ourselves, otherwise the ScrollView would delete us on close.
  set_owned_by_client();

  // Menus in Chrome are always traversed in a vertical direction.
  GetViewAccessibility().SetIsVertical(true);
  GetViewAccessibility().SetRole(ax::mojom::Role::kMenu);
}

SubmenuView::~SubmenuView() {
  // The menu may not have been closed yet (it will be hidden, but not
  // necessarily closed).
  Close();
}

std::vector<MenuItemView*> SubmenuView::GetMenuItems() {
  return GetMenuItemsFromChildren<MenuItemView, View>(children());
}

std::vector<const MenuItemView*> SubmenuView::GetMenuItems() const {
  return GetMenuItemsFromChildren<const MenuItemView, const View>(children());
}

MenuItemView* SubmenuView::GetMenuItemAt(size_t index) {
  const auto menu_items = GetMenuItems();
  CHECK_LT(index, menu_items.size());
  return menu_items[index];
}

int SubmenuView::GetPreferredItemHeight() const {
  EmptyMenuMenuItem menu_item(parent_menu_item_);
  menu_item.set_controller(parent_menu_item_->GetMenuController());
  return menu_item.GetPreferredSize({}).height();
}

PrefixSelector* SubmenuView::GetPrefixSelector() {
  return &prefix_selector_;
}

void SubmenuView::UpdateMenuPartSizes() {
  const MenuConfig& config = MenuConfig::instance();

  const auto get_metrics = [&] {
    return std::tie(icon_area_width_, label_start_, trailing_padding_);
  };
  const auto old_metrics = get_metrics();

  trailing_padding_ = config.item_horizontal_padding +
                      parent_menu_item_->GetItemHorizontalBorder();
  const auto& menu_items = GetMenuItems();
  if (config.reserve_dedicated_arrow_column &&
      base::ranges::any_of(menu_items, &MenuItemView::HasSubmenu)) {
    trailing_padding_ +=
        config.arrow_size +
        (base::Contains(menu_items, MenuItemView::Type::kActionableSubMenu,
                        &MenuItemView::GetType)
             ? config.actionable_submenu_arrow_to_edge_padding
             : config.arrow_to_edge_padding);
  }

  const auto is_check_or_radio = [](const auto* item) {
    const auto type = item->GetType();
    return type == MenuItemView::Type::kCheckbox ||
           type == MenuItemView::Type::kRadio;
  };
  icon_area_width_ = min_icon_height_ =
      (config.always_reserve_check_region ||
       base::ranges::any_of(menu_items, is_check_or_radio))
          ? kMenuCheckSize
          : 0;
  int max_icon_width = 0;
  if (!menu_items.empty()) {
    std::vector<int> widths(menu_items.size());
    base::ranges::transform(
        menu_items, widths.begin(), [&](const MenuItemView* item) {
          const auto icon_size = item->GetIconPreferredSize();
          if (icon_size.IsEmpty()) {
            return 0;
          }
          min_icon_height_ = std::max(min_icon_height_, kMenuCheckSize);
          // If this item has a radio or checkbox, an additional icon will not
          // affect horizontal alignment of other items.
          return (config.icons_in_label || !is_check_or_radio(item))
                     ? icon_size.width()
                     : 0;
        });
    max_icon_width = base::ranges::max(widths);
  }
  if (!config.icons_in_label) {
    icon_area_width_ = std::max(icon_area_width_, max_icon_width);
  }

  label_start_ = parent_menu_item_->GetContentStart() + icon_area_width_;
  if (icon_area_width_) {
    const auto* const controller = parent_menu_item_->GetMenuController();
    label_start_ += (controller && controller->use_ash_system_ui_layout())
                        ? config.touchable_item_horizontal_padding
                        : config.icon_label_spacing;
  }

  if (config.icons_in_label) {
    icon_area_width_ = max_icon_width;
  }

  if (get_metrics() != old_metrics) {
    InvalidateLayout();
  }
}

void SubmenuView::ChildPreferredSizeChanged(View* child) {
  if (!resize_open_menu_)
    return;

  MenuItemView* item = parent_menu_item_;
  MenuController* controller = item->GetMenuController();

  if (controller) {
    MenuController::MenuOpenDirection dir;
    ui::OwnedWindowAnchor anchor;
    gfx::Rect bounds = controller->CalculateMenuBounds(
        item, MenuController::MenuOpenDirection::kTrailing, &dir, &anchor);
    Reposition(bounds, anchor);
  }
}

void SubmenuView::Layout(PassKey) {
  // We're in a ScrollView, and need to set our width/height ourselves.
  if (!parent()) {
    return;
  }

  // Use our current y, unless it means part of the menu isn't visible anymore.
  const int pref_height = GetPreferredSize(SizeBounds(size())).height();
  SetBounds(x(),
            (pref_height > parent()->height())
                ? std::max(parent()->height() - pref_height, y())
                : 0,
            parent()->width(), pref_height);

  const gfx::Insets insets = GetInsets();
  const int x = insets.left();
  int y = insets.top();
  const int menu_item_width = width() - insets.width();
  const int between_item_vertical_padding =
      MenuConfig::instance().between_item_vertical_padding;
  bool previous_child_was_lower_separator = false;
  for (View* child : children()) {
    if (child->GetVisible()) {
      const auto* separator = AsViewClass<MenuSeparator>(child);
      if (y != insets.top() && !previous_child_was_lower_separator &&
          (!separator || separator->GetType() != ui::UPPER_SEPARATOR)) {
        y += between_item_vertical_padding;
      }
      child->SetBounds(x, y, menu_item_width,
                       child->GetHeightForWidth(menu_item_width));
      y = child->bounds().bottom();
      previous_child_was_lower_separator =
          separator && separator->GetType() == ui::LOWER_SEPARATOR;
    }
  }
}

gfx::Size SubmenuView::CalculatePreferredSize(
    const SizeBounds& /*available_size*/) const {
  if (children().empty()) {
    return gfx::Size();
  }

  max_minor_text_width_ = 0;
  // The maximum width of items which contain maybe a label and multiple views.
  int max_complex_width = 0;
  // The max. width of items which contain a label and maybe an accelerator.
  int max_simple_width = 0;
  // The minimum width of touchable items.
  int touchable_minimum_width = 0;

  // We perform the size calculation in two passes. In the first pass, we
  // calculate the width of the menu. In the second, we calculate the height
  // using that width. This allows views that have flexible widths to adjust
  // accordingly.
  for (const View* child : children()) {
    if (!child->GetVisible()) {
      continue;
    }
    if (const auto* const menu = AsViewClass<const MenuItemView>(child)) {
      const MenuItemView::MenuItemDimensions& dimensions =
          menu->GetDimensions();
      max_simple_width = std::max(max_simple_width, dimensions.standard_width);
      max_minor_text_width_ =
          std::max(max_minor_text_width_, dimensions.minor_text_width);
      max_complex_width =
          std::max(max_complex_width,
                   dimensions.standard_width + dimensions.children_width);
      touchable_minimum_width = dimensions.standard_width;
    } else {
      max_complex_width =
          std::max(max_complex_width, child->GetPreferredSize({}).width());
    }
  }
  const auto& config = MenuConfig::instance();
  if (max_minor_text_width_ > 0) {
    max_minor_text_width_ += config.item_horizontal_padding;
  }

  // Finish calculating our optimum width.
  const gfx::Insets insets = GetInsets();
  int width = std::max(
      max_complex_width,
      std::max(max_simple_width + max_minor_text_width_ + insets.width(),
               minimum_preferred_width_ - 2 * insets.width()));

  if (parent_menu_item_->GetMenuController() &&
      parent_menu_item_->GetMenuController()->use_ash_system_ui_layout()) {
    width = std::max(touchable_minimum_width, width);
  }

  // Then, the height for that width.
  const int menu_item_width = width - insets.width();
  bool previous_child_was_lower_separator = false;
  const auto get_height = [&](int height, const View* child) {
    if (!child->GetVisible()) {
      return height;
    }
    const auto* separator = AsViewClass<MenuSeparator>(child);
    if (height && !previous_child_was_lower_separator &&
        (!separator || separator->GetType() != ui::UPPER_SEPARATOR)) {
      height += config.between_item_vertical_padding;
    }
    previous_child_was_lower_separator =
        separator && separator->GetType() == ui::LOWER_SEPARATOR;
    return height + child->GetHeightForWidth(menu_item_width);
  };
  const int height =
      std::accumulate(children().cbegin(), children().cend(), 0, get_height);

  return gfx::Size(width, height + insets.height());
}

void SubmenuView::PaintChildren(const PaintInfo& paint_info) {
  View::PaintChildren(paint_info);

  bool paint_drop_indicator = false;
  if (drop_item_) {
    switch (drop_position_) {
      case MenuDelegate::DropPosition::kNone:
      case MenuDelegate::DropPosition::kOn:
        break;
      case MenuDelegate::DropPosition::kUnknow:
      case MenuDelegate::DropPosition::kBefore:
      case MenuDelegate::DropPosition::kAfter:
        paint_drop_indicator = true;
        break;
    }
  }

  if (paint_drop_indicator) {
    gfx::Rect bounds = CalculateDropIndicatorBounds(drop_item_, drop_position_);
    ui::PaintRecorder recorder(paint_info.context(), size());
    const SkColor drop_indicator_color =
        GetColorProvider()->GetColor(ui::kColorMenuDropmarker);
    recorder.canvas()->FillRect(bounds, drop_indicator_color);
  }
}

bool SubmenuView::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  DCHECK(parent_menu_item_->GetMenuController());
  return parent_menu_item_->GetMenuController()->GetDropFormats(this, formats,
                                                                format_types);
}

bool SubmenuView::AreDropTypesRequired() {
  DCHECK(parent_menu_item_->GetMenuController());
  return parent_menu_item_->GetMenuController()->AreDropTypesRequired(this);
}

bool SubmenuView::CanDrop(const OSExchangeData& data) {
  DCHECK(parent_menu_item_->GetMenuController());
  return parent_menu_item_->GetMenuController()->CanDrop(this, data);
}

void SubmenuView::OnDragEntered(const ui::DropTargetEvent& event) {
  DCHECK(parent_menu_item_->GetMenuController());
  parent_menu_item_->GetMenuController()->OnDragEntered(this, event);
}

int SubmenuView::OnDragUpdated(const ui::DropTargetEvent& event) {
  DCHECK(parent_menu_item_->GetMenuController());
  return parent_menu_item_->GetMenuController()->OnDragUpdated(this, event);
}

void SubmenuView::OnDragExited() {
  DCHECK(parent_menu_item_->GetMenuController());
  parent_menu_item_->GetMenuController()->OnDragExited(this);
}

views::View::DropCallback SubmenuView::GetDropCallback(
    const ui::DropTargetEvent& event) {
  DCHECK(parent_menu_item_->GetMenuController());
  drop_item_ = nullptr;
  return parent_menu_item_->GetMenuController()->GetDropCallback(this, event);
}

bool SubmenuView::OnMouseWheel(const ui::MouseWheelEvent& e) {
  gfx::Rect vis_bounds = GetVisibleBounds();
  const auto menu_items = GetMenuItems();
  if (vis_bounds.height() == height() || menu_items.empty()) {
    // All menu items are visible, nothing to scroll.
    return true;
  }

  auto i = base::ranges::lower_bound(menu_items, vis_bounds.y(), {},
                                     &MenuItemView::y);
  if (i == menu_items.cend())
    return true;

  // If the first item isn't entirely visible, make it visible, otherwise make
  // the next/previous one entirely visible. If enough wasn't scrolled to show
  // any new rows, then just scroll the amount so that smooth scrolling using
  // the trackpad is possible.
  int delta = abs(e.y_offset() / ui::MouseWheelEvent::kWheelDelta);
  if (delta == 0)
    return OnScroll(0, e.y_offset());

  const auto scrolled_to_top = [&vis_bounds](const MenuItemView* item) {
    return item->y() == vis_bounds.y();
  };
  if (i != menu_items.cbegin() && !scrolled_to_top(*i))
    --i;
  for (bool scroll_up = (e.y_offset() > 0); delta != 0; --delta) {
    int scroll_target;
    if (scroll_up) {
      if (scrolled_to_top(*i)) {
        if (i == menu_items.cbegin())
          break;
        --i;
      }
      scroll_target = (*i)->y();
    } else {
      const auto next_iter = std::next(i);
      if (next_iter == menu_items.cend())
        break;
      scroll_target = (*next_iter)->y();
      if (scrolled_to_top(*i))
        i = next_iter;
    }
    ScrollRectToVisible(
        gfx::Rect(gfx::Point(0, scroll_target), vis_bounds.size()));
    vis_bounds = GetVisibleBounds();
  }

  return true;
}

void SubmenuView::OnGestureEvent(ui::GestureEvent* event) {
  bool handled = true;
  switch (event->type()) {
    case ui::EventType::kGestureScrollBegin:
      scroll_animator_->Stop();
      break;
    case ui::EventType::kGestureScrollUpdate:
      handled = OnScroll(0, event->details().scroll_y());
      break;
    case ui::EventType::kGestureScrollEnd:
      break;
    case ui::EventType::kScrollFlingStart:
      if (event->details().velocity_y() != 0.0f)
        scroll_animator_->Start(0, event->details().velocity_y());
      break;
    case ui::EventType::kGestureTapDown:
    case ui::EventType::kScrollFlingCancel:
      if (scroll_animator_->is_scrolling())
        scroll_animator_->Stop();
      else
        handled = false;
      break;
    default:
      handled = false;
      break;
  }
  if (handled)
    event->SetHandled();
}

size_t SubmenuView::GetRowCount() {
  return GetMenuItems().size();
}

std::optional<size_t> SubmenuView::GetSelectedRow() {
  const auto menu_items = GetMenuItems();
  const auto i = base::ranges::find_if(menu_items, &MenuItemView::IsSelected);
  return (i == menu_items.cend()) ? std::nullopt
                                  : std::make_optional(static_cast<size_t>(
                                        std::distance(menu_items.cbegin(), i)));
}

void SubmenuView::SetSelectedRow(std::optional<size_t> row) {
  parent_menu_item_->GetMenuController()->SetSelection(
      GetMenuItemAt(row.value()), MenuController::SELECTION_DEFAULT);
}

std::u16string SubmenuView::GetTextForRow(size_t row) {
  return MenuItemView::GetAccessibleNameForMenuItem(
      GetMenuItemAt(row)->title(), std::u16string(),
      GetMenuItemAt(row)->ShouldShowNewBadge());
}

bool SubmenuView::IsShowing() const {
  return host_ && host_->IsMenuHostVisible();
}

void SubmenuView::ShowAt(const MenuHost::InitParams& init_params) {
  if (host_) {
    host_->SetMenuHostBounds(init_params.bounds);
    host_->ShowMenuHost(init_params.do_capture);
  } else {
    host_ = new MenuHost(this);
    // Force construction of the scroll view container.
    GetScrollViewContainer();
    // Force a layout since our preferred size may not have changed but our
    // content may have.
    InvalidateLayout();

    MenuHost::InitParams new_init_params = init_params;
    new_init_params.contents_view = scroll_view_container_.get();
    host_->InitMenuHost(new_init_params);
  }

  // Only fire kMenuStart when a top level menu is being shown to notify that
  // menu interaction is about to begin. Note that the ScrollViewContainer
  // is not exposed as a kMenu, but as a kMenuBar for most platforms and a
  // kNone on the Mac. See MenuScrollViewContainer::GetAccessibleNodeData.
  if (!GetMenuItem()->GetParentMenuItem()) {
    GetScrollViewContainer()->NotifyAccessibilityEvent(
        ax::mojom::Event::kMenuStart, true);
  }
  // Fire kMenuPopupStart for each menu/submenu that is shown.
  NotifyAccessibilityEvent(ax::mojom::Event::kMenuPopupStart, true);

  // Announce if the menu/submenu is empty.
  if (GetRowCount() == 0) {
    GetViewAccessibility().AnnouncePolitely(
        l10n_util::GetStringUTF16(IDS_APP_MENU_AX_ANNOUNCE_EMPTY_SUBMENU));
  }
}

void SubmenuView::Reposition(const gfx::Rect& bounds,
                             const ui::OwnedWindowAnchor& anchor) {
  if (host_) {
    // Anchor must be updated first.
    host_->SetMenuHostOwnedWindowAnchor(anchor);
    host_->SetMenuHostBounds(bounds);
  }
}

void SubmenuView::Close() {
  if (host_) {
    host_->DestroyMenuHost();
    host_ = nullptr;
  }
}

void SubmenuView::Hide() {
  if (host_) {
    /// -- Fire accessibility events ----
    // Both of these must be fired before HideMenuHost().
    // Only fire kMenuEnd when a top level menu closes, not for each submenu.
    // This is sent before kMenuPopupEnd to allow ViewAXPlatformNodeDelegate to
    // remove its focus override before AXPlatformNodeAuraLinux needs to access
    // the previously-focused node while handling kMenuPopupEnd.
    if (!GetMenuItem()->GetParentMenuItem()) {
      GetScrollViewContainer()->NotifyAccessibilityEvent(
          ax::mojom::Event::kMenuEnd, true);
      GetViewAccessibility().EndPopupFocusOverride();
    }
    // Fire these kMenuPopupEnd for each menu/submenu that closes/hides.
    if (host_->IsVisible())
      NotifyAccessibilityEvent(ax::mojom::Event::kMenuPopupEnd, true);

    host_->HideMenuHost();
  }

  if (scroll_animator_->is_scrolling())
    scroll_animator_->Stop();
}

void SubmenuView::ReleaseCapture() {
  if (host_)
    host_->ReleaseMenuHostCapture();
}

bool SubmenuView::SkipDefaultKeyEventProcessing(const ui::KeyEvent& e) {
  return views::FocusManager::IsTabTraversalKeyEvent(e);
}

const MenuItemView* SubmenuView::GetMenuItem() const {
  return parent_menu_item_;
}

void SubmenuView::SetDropMenuItem(MenuItemView* item,
                                  MenuDelegate::DropPosition position) {
  if (drop_item_ == item && drop_position_ == position)
    return;
  SchedulePaintForDropIndicator(drop_item_, drop_position_);
  MenuItemView* old_drop_item = std::exchange(drop_item_, item);
  drop_position_ = position;
  if (!old_drop_item || !item) {
    // Whether the selection is actually drawn
    // (`MenuItemView:last_paint_as_selected_`) depends upon whether there is a
    // drop item. Find the selected item and have it updates its paint as
    // selected state.
    for (View* child : children()) {
      if (auto* child_menu_item = AsViewClass<MenuItemView>(child);
          child_menu_item && child_menu_item->GetVisible() &&
          child_menu_item->IsSelected()) {
        child_menu_item->OnDropOrSelectionStatusMayHaveChanged();
        // Only one menu item is selected, so no need to continue iterating once
        // the selected item is found.
        break;
      }
    }
  } else {
    if (old_drop_item && old_drop_item != drop_item_)
      old_drop_item->OnDropOrSelectionStatusMayHaveChanged();
    if (drop_item_)
      drop_item_->OnDropOrSelectionStatusMayHaveChanged();
  }
  SchedulePaintForDropIndicator(drop_item_, drop_position_);
}

bool SubmenuView::GetShowSelection(const MenuItemView* item) const {
  return !drop_item_ || (drop_item_ == item &&
                         drop_position_ == MenuDelegate::DropPosition::kOn);
}

MenuScrollViewContainer* SubmenuView::GetScrollViewContainer() {
  // Perform null checks for scroll_view_container and MenuController since
  // MenuScrollViewContainer constructor is invoked later, which uses
  // menucontroller to determine the value of the use_ash_system_ui_layout_
  // variable.
  if (!scroll_view_container_ && !parent_menu_item_->GetMenuController()) {
    return nullptr;
  }

  if (!scroll_view_container_) {
    scroll_view_container_ = std::make_unique<MenuScrollViewContainer>(this);
    // Otherwise MenuHost would delete us.
    scroll_view_container_->set_owned_by_client();
    scroll_view_container_->SetBorderColorId(border_color_id_);
  }
  return scroll_view_container_.get();
}

MenuItemView* SubmenuView::GetLastItem() {
  const auto menu_items = GetMenuItems();
  return menu_items.empty() ? nullptr : menu_items.back();
}

void SubmenuView::MenuHostDestroyed() {
  host_ = nullptr;
  MenuController* controller = parent_menu_item_->GetMenuController();
  if (controller)
    controller->Cancel(MenuController::ExitType::kDestroyed);
}

void SubmenuView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  SchedulePaint();
}

void SubmenuView::SchedulePaintForDropIndicator(
    MenuItemView* item,
    MenuDelegate::DropPosition position) {
  if (item == nullptr)
    return;

  if (position == MenuDelegate::DropPosition::kOn) {
    item->SchedulePaint();
  } else if (position != MenuDelegate::DropPosition::kNone) {
    SchedulePaintInRect(CalculateDropIndicatorBounds(item, position));
  }
}

gfx::Rect SubmenuView::CalculateDropIndicatorBounds(
    MenuItemView* item,
    MenuDelegate::DropPosition position) {
  DCHECK(position != MenuDelegate::DropPosition::kNone);
  gfx::Rect item_bounds = item->bounds();
  switch (position) {
    case MenuDelegate::DropPosition::kBefore:
      item_bounds.Offset(0, -kDropIndicatorHeight / 2);
      item_bounds.set_height(kDropIndicatorHeight);
      return item_bounds;

    case MenuDelegate::DropPosition::kAfter:
      item_bounds.Offset(0, item_bounds.height() - kDropIndicatorHeight / 2);
      item_bounds.set_height(kDropIndicatorHeight);
      return item_bounds;

    default:
      // Don't render anything for on.
      return gfx::Rect();
  }
}

bool SubmenuView::OnScroll(float dx, float dy) {
  const gfx::Rect& vis_bounds = GetVisibleBounds();
  const gfx::Rect& full_bounds = bounds();
  int x = vis_bounds.x();
  float y_f = vis_bounds.y() - dy - roundoff_error_;
  int y = base::ClampRound(y_f);
  roundoff_error_ = y - y_f;

  // Ensure that we never try to scroll outside the actual child view.
  // Note: the old code here was effectively:
  //   std::clamp(y, 0, full_bounds.height() - vis_bounds.height() - 1)
  // but the -1 there prevented fully scrolling to the bottom here. As a
  // worked example, suppose that:
  //   full_bounds = { x = 0, y = 0, w = 100, h = 1000 }
  //   vis_bounds = { x = 0, y = 450, w = 100, h = 500 }
  // and dy = 50. It should be the case that the new vis_bounds are:
  //   new_vis_bounds = { x = 0, y = 500, w = 100, h = 500 }
  // because full_bounds.height() - vis_bounds.height() == 500. Intuitively,
  // this makes sense - the bottom 500 pixels of this view, starting with y =
  // 500, are shown.
  //
  // With the clamp set to full_bounds.height() - vis_bounds.height() - 1,
  // this code path instead would produce:
  //   new_vis_bounds = { x = 0, y = 499, w = 100, h = 500 }
  // so pixels y=499 through y=998 of this view are drawn, and pixel y=999 is
  // hidden - oops.
  y = std::clamp(y, 0, full_bounds.height() - vis_bounds.height());

  gfx::Rect new_vis_bounds(x, y, vis_bounds.width(), vis_bounds.height());
  if (new_vis_bounds != vis_bounds) {
    ScrollRectToVisible(new_vis_bounds);
    return true;
  }
  return false;
}

void SubmenuView::SetBorderColorId(std::optional<ui::ColorId> color_id) {
  if (scroll_view_container_) {
    scroll_view_container_->SetBorderColorId(color_id);
  }

  border_color_id_ = color_id;
}

BEGIN_METADATA(SubmenuView)
END_METADATA

}  // namespace views
