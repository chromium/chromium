// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/submenu_view.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/input_method.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_host.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

namespace {

// Height of the drop indicator. This should be an even number.
constexpr int kDropIndicatorHeight = 2;

// Color of the drop indicator.
constexpr SkColor kDropIndicatorColor = SK_ColorBLACK;

}  // namespace

namespace views {

// static
const char SubmenuView::kViewClassName[] = "SubmenuView";

SubmenuView::SubmenuView(MenuItemView* parent)
    : parent_menu_item_(parent),
      host_(NULL),
      drop_item_(NULL),
      drop_position_(MenuDelegate::DROP_NONE),
      scroll_view_container_(NULL),
      max_minor_text_width_(0),
      minimum_preferred_width_(0),
      resize_open_menu_(false),
      scroll_animator_(new ScrollAnimator(this)),
      roundoff_error_(0),
      prefix_selector_(this, this) {
  DCHECK(parent);
  // We'll delete ourselves, otherwise the ScrollView would delete us on close.
  set_owned_by_client();
}

SubmenuView::~SubmenuView() {
  // The menu may not have been closed yet (it will be hidden, but not
  // necessarily closed).
  Close();

  delete scroll_view_container_;
}

bool SubmenuView::HasEmptyMenuItemView() {
  for (int i = 0; i < child_count(); i++) {
    if (child_at(i)->id() == MenuItemView::kEmptyMenuItemViewID)
      return true;
  }
  return false;
}

bool SubmenuView::HasVisibleChildren() {
  for (int i = 0, item_count = GetMenuItemCount(); i < item_count; i++) {
    if (GetMenuItemAt(i)->visible())
      return true;
  }
  return false;
}

int SubmenuView::GetMenuItemCount() const {
  int count = 0;
  for (int i = 0; i < child_count(); ++i) {
    if (child_at(i)->id() == MenuItemView::kMenuItemViewID)
      count++;
  }
  return count;
}

MenuItemView* SubmenuView::GetMenuItemAt(int index) {
  for (int i = 0, count = 0; i < child_count(); ++i) {
    if (child_at(i)->id() == MenuItemView::kMenuItemViewID &&
        count++ == index) {
      return static_cast<MenuItemView*>(child_at(i));
    }
  }
  NOTREACHED();
  return NULL;
}

PrefixSelector* SubmenuView::GetPrefixSelector() {
  return &prefix_selector_;
}

void SubmenuView::ChildPreferredSizeChanged(View* child) {
  if (!resize_open_menu_)
    return;

  MenuItemView *item = GetMenuItem();
  MenuController* controller = item->GetMenuController();

  if (controller) {
    bool dir;
    gfx::Rect bounds = controller->CalculateMenuBounds(item, false, &dir);
    Reposition(bounds);
  }
}

void SubmenuView::Layout() {
  // We're in a ScrollView, and need to set our width/height ourselves.
  if (!parent())
    return;

  // Use our current y, unless it means part of the menu isn't visible anymore.
  int pref_height = GetPreferredSize().height();
  int new_y;
  if (pref_height > parent()->height())
    new_y = std::max(parent()->height() - pref_height, y());
  else
    new_y = 0;
  SetBounds(x(), new_y, parent()->width(), pref_height);

  gfx::Insets insets = GetInsets();
  int x = insets.left();
  int y = insets.top();
  int menu_item_width = width() - insets.width();
  for (int i = 0; i < child_count(); ++i) {
    View* child = child_at(i);
    if (child->visible()) {
      int child_height = child->GetHeightForWidth(menu_item_width);
      child->SetBounds(x, y, menu_item_width, child_height);
      y += child_height;
    }
  }
}

gfx::Size SubmenuView::CalculatePreferredSize() const {
  if (!has_children())
    return gfx::Size();

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
  for (int i = 0; i < child_count(); ++i) {
    const View* child = child_at(i);
    if (!child->visible())
      continue;
    if (child->id() == MenuItemView::kMenuItemViewID) {
      const MenuItemView* menu = static_cast<const MenuItemView*>(child);
      const MenuItemView::MenuItemDimensions& dimensions =
          menu->GetDimensions();
      max_simple_width = std::max(
          max_simple_width, dimensions.standard_width);
      max_minor_text_width_ =
          std::max(max_minor_text_width_, dimensions.minor_text_width);
      max_complex_width = std::max(max_complex_width,
          dimensions.standard_width + dimensions.children_width);
      touchable_minimum_width = dimensions.standard_width;
    } else {
      max_complex_width = std::max(max_complex_width,
                                   child->GetPreferredSize().width());
    }
  }
  if (max_minor_text_width_ > 0)
    max_minor_text_width_ += MenuConfig::instance().item_horizontal_padding;

  // Finish calculating our optimum width.
  gfx::Insets insets = GetInsets();
  int width = std::max(max_complex_width,
                       std::max(max_simple_width + max_minor_text_width_ +
                                    insets.width(),
                                minimum_preferred_width_ - 2 * insets.width()));

  if (GetMenuItem()->GetMenuController() &&
      GetMenuItem()->GetMenuController()->use_touchable_layout()) {
    width = std::max(touchable_minimum_width, width);
  }

  // Then, the height for that width.
  int height = 0;
  int menu_item_width = width - insets.width();
  for (int i = 0; i < child_count(); ++i) {
    const View* child = child_at(i);
    height += child->visible() ? child->GetHeightForWidth(menu_item_width) : 0;
  }

  return gfx::Size(width, height + insets.height());
}

void SubmenuView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // Inherit most of the state from the parent menu item, except the role and
  // the orientation.
  if (GetMenuItem())
    GetMenuItem()->GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kMenuListPopup;
  // Menus in Chrome are always traversed in a vertical direction.
  node_data->AddState(ax::mojom::State::kVertical);
}

void SubmenuView::PaintChildren(const PaintInfo& paint_info) {
  View::PaintChildren(paint_info);

  bool paint_drop_indicator = false;
  if (drop_item_) {
    switch (drop_position_) {
      case MenuDelegate::DROP_NONE:
      case MenuDelegate::DROP_ON:
        break;
      case MenuDelegate::DROP_UNKNOWN:
      case MenuDelegate::DROP_BEFORE:
      case MenuDelegate::DROP_AFTER:
        paint_drop_indicator = true;
        break;
    }
  }

  if (paint_drop_indicator) {
    gfx::Rect bounds = CalculateDropIndicatorBounds(drop_item_, drop_position_);
    ui::PaintRecorder recorder(paint_info.context(), size());
    recorder.canvas()->FillRect(bounds, kDropIndicatorColor);
  }
}

bool SubmenuView::GetDropFormats(
    int* formats,
    std::set<ui::Clipboard::FormatType>* format_types) {
  DCHECK(GetMenuItem()->GetMenuController());
  return GetMenuItem()->GetMenuController()->GetDropFormats(this, formats,
                                                            format_types);
}

bool SubmenuView::AreDropTypesRequired() {
  DCHECK(GetMenuItem()->GetMenuController());
  return GetMenuItem()->GetMenuController()->AreDropTypesRequired(this);
}

bool SubmenuView::CanDrop(const OSExchangeData& data) {
  DCHECK(GetMenuItem()->GetMenuController());
  return GetMenuItem()->GetMenuController()->CanDrop(this, data);
}

void SubmenuView::OnDragEntered(const ui::DropTargetEvent& event) {
  DCHECK(GetMenuItem()->GetMenuController());
  GetMenuItem()->GetMenuController()->OnDragEntered(this, event);
}

int SubmenuView::OnDragUpdated(const ui::DropTargetEvent& event) {
  DCHECK(GetMenuItem()->GetMenuController());
  return GetMenuItem()->GetMenuController()->OnDragUpdated(this, event);
}

void SubmenuView::OnDragExited() {
  DCHECK(GetMenuItem()->GetMenuController());
  GetMenuItem()->GetMenuController()->OnDragExited(this);
}

int SubmenuView::OnPerformDrop(const ui::DropTargetEvent& event) {
  DCHECK(GetMenuItem()->GetMenuController());
  return GetMenuItem()->GetMenuController()->OnPerformDrop(this, event);
}

bool SubmenuView::OnMouseWheel(const ui::MouseWheelEvent& e) {
  gfx::Rect vis_bounds = GetVisibleBounds();
  int menu_item_count = GetMenuItemCount();
  if (vis_bounds.height() == height() || !menu_item_count) {
    // All menu items are visible, nothing to scroll.
    return true;
  }

  // Find the index of the first menu item whose y-coordinate is >= visible
  // y-coordinate.
  int i = 0;
  while ((i < menu_item_count) && (GetMenuItemAt(i)->y() < vis_bounds.y()))
    ++i;
  if (i == menu_item_count)
    return true;
  int first_vis_index = std::max(0,
      (GetMenuItemAt(i)->y() == vis_bounds.y()) ? i : i - 1);

  // If the first item isn't entirely visible, make it visible, otherwise make
  // the next/previous one entirely visible. If enough wasn't scrolled to show
  // any new rows, then just scroll the amount so that smooth scrolling using
  // the trackpad is possible.
  int delta = abs(e.y_offset() / ui::MouseWheelEvent::kWheelDelta);
  if (delta == 0)
    return OnScroll(0, e.y_offset());
  for (bool scroll_up = (e.y_offset() > 0); delta != 0; --delta) {
    int scroll_target;
    if (scroll_up) {
      if (GetMenuItemAt(first_vis_index)->y() == vis_bounds.y()) {
        if (first_vis_index == 0)
          break;
        first_vis_index--;
      }
      scroll_target = GetMenuItemAt(first_vis_index)->y();
    } else {
      if (first_vis_index + 1 == menu_item_count)
        break;
      scroll_target = GetMenuItemAt(first_vis_index + 1)->y();
      if (GetMenuItemAt(first_vis_index)->y() == vis_bounds.y())
        first_vis_index++;
    }
    ScrollRectToVisible(gfx::Rect(gfx::Point(0, scroll_target),
                                  vis_bounds.size()));
    vis_bounds = GetVisibleBounds();
  }

  return true;
}

void SubmenuView::OnGestureEvent(ui::GestureEvent* event) {
  bool handled = true;
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      scroll_animator_->Stop();
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      handled = OnScroll(0, event->details().scroll_y());
      break;
    case ui::ET_GESTURE_SCROLL_END:
      break;
    case ui::ET_SCROLL_FLING_START:
      if (event->details().velocity_y() != 0.0f)
        scroll_animator_->Start(0, event->details().velocity_y());
      break;
    case ui::ET_GESTURE_TAP_DOWN:
    case ui::ET_SCROLL_FLING_CANCEL:
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

int SubmenuView::GetRowCount() {
  return GetMenuItemCount();
}

int SubmenuView::GetSelectedRow() {
  int row = 0;
  for (int i = 0; i < child_count(); ++i) {
    if (child_at(i)->id() != MenuItemView::kMenuItemViewID)
      continue;

    if (static_cast<MenuItemView*>(child_at(i))->IsSelected())
      return row;

    row++;
  }

  return -1;
}

void SubmenuView::SetSelectedRow(int row) {
  GetMenuItem()->GetMenuController()->SetSelection(
      GetMenuItemAt(row),
      MenuController::SELECTION_DEFAULT);
}

base::string16 SubmenuView::GetTextForRow(int row) {
  return MenuItemView::GetAccessibleNameForMenuItem(GetMenuItemAt(row)->title(),
                                                    base::string16());
}

bool SubmenuView::IsShowing() {
  return host_ && host_->IsMenuHostVisible();
}

void SubmenuView::ShowAt(Widget* parent,
                         const gfx::Rect& bounds,
                         bool do_capture) {
  if (host_) {
    host_->SetMenuHostBounds(bounds);
    host_->ShowMenuHost(do_capture);
  } else {
    host_ = new MenuHost(this);
    // Force construction of the scroll view container.
    GetScrollViewContainer();
    // Force a layout since our preferred size may not have changed but our
    // content may have.
    InvalidateLayout();
    host_->InitMenuHost(parent, bounds, scroll_view_container_, do_capture);
  }

  GetScrollViewContainer()->NotifyAccessibilityEvent(
      ax::mojom::Event::kMenuStart, true);
  NotifyAccessibilityEvent(ax::mojom::Event::kMenuPopupStart, true);
}

void SubmenuView::Reposition(const gfx::Rect& bounds) {
  if (host_)
    host_->SetMenuHostBounds(bounds);
}

void SubmenuView::Close() {
  if (host_) {
    NotifyAccessibilityEvent(ax::mojom::Event::kMenuPopupEnd, true);
    GetScrollViewContainer()->NotifyAccessibilityEvent(
        ax::mojom::Event::kMenuEnd, true);

    host_->DestroyMenuHost();
    host_ = NULL;
  }
}

void SubmenuView::Hide() {
  if (host_)
    host_->HideMenuHost();
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

MenuItemView* SubmenuView::GetMenuItem() const {
  return parent_menu_item_;
}

void SubmenuView::SetDropMenuItem(MenuItemView* item,
                                  MenuDelegate::DropPosition position) {
  if (drop_item_ == item && drop_position_ == position)
    return;
  SchedulePaintForDropIndicator(drop_item_, drop_position_);
  drop_item_ = item;
  drop_position_ = position;
  SchedulePaintForDropIndicator(drop_item_, drop_position_);
}

bool SubmenuView::GetShowSelection(MenuItemView* item) {
  if (drop_item_ == NULL)
    return true;
  // Something is being dropped on one of this menus items. Show the
  // selection if the drop is on the passed in item and the drop position is
  // ON.
  return (drop_item_ == item && drop_position_ == MenuDelegate::DROP_ON);
}

MenuScrollViewContainer* SubmenuView::GetScrollViewContainer() {
  if (!scroll_view_container_) {
    scroll_view_container_ = new MenuScrollViewContainer(this);
    if (GetMenuItem()->GetParentMenuItem() == nullptr) {
      // Top-level menu, this may have a footnote. Submenus can't have a
      // footnote, because they share the |MenuDelegate| with their parent.
      View* footnote_view = GetMenuItem()->GetDelegate()->CreateFootnoteView();
      if (footnote_view)
        scroll_view_container_->SetFootnoteView(footnote_view);
    }
    // Otherwise MenuHost would delete us.
    scroll_view_container_->set_owned_by_client();
  }
  return scroll_view_container_;
}

void SubmenuView::MenuHostDestroyed() {
  host_ = NULL;
  MenuController* controller = GetMenuItem()->GetMenuController();
  if (controller)
    controller->Cancel(MenuController::EXIT_DESTROYED);
}

const char* SubmenuView::GetClassName() const {
  return kViewClassName;
}

void SubmenuView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  SchedulePaint();
}

void SubmenuView::SchedulePaintForDropIndicator(
    MenuItemView* item,
    MenuDelegate::DropPosition position) {
  if (item == NULL)
    return;

  if (position == MenuDelegate::DROP_ON) {
    item->SchedulePaint();
  } else if (position != MenuDelegate::DROP_NONE) {
    SchedulePaintInRect(CalculateDropIndicatorBounds(item, position));
  }
}

gfx::Rect SubmenuView::CalculateDropIndicatorBounds(
    MenuItemView* item,
    MenuDelegate::DropPosition position) {
  DCHECK(position != MenuDelegate::DROP_NONE);
  gfx::Rect item_bounds = item->bounds();
  switch (position) {
    case MenuDelegate::DROP_BEFORE:
      item_bounds.Offset(0, -kDropIndicatorHeight / 2);
      item_bounds.set_height(kDropIndicatorHeight);
      return item_bounds;

    case MenuDelegate::DROP_AFTER:
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
  int y = gfx::ToRoundedInt(y_f);
  roundoff_error_ = y - y_f;
  // clamp y to [0, full_height - vis_height)
  y = std::min(y, full_bounds.height() - vis_bounds.height() - 1);
  y = std::max(y, 0);
  gfx::Rect new_vis_bounds(x, y, vis_bounds.width(), vis_bounds.height());
  if (new_vis_bounds != vis_bounds) {
    ScrollRectToVisible(new_vis_bounds);
    return true;
  }
  return false;
}

}  // namespace views
