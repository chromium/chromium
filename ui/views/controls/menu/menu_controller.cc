// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_controller.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/numerics/ranges.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller_delegate.h"
#include "ui/views/controls/menu/menu_host_root_view.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_pre_target_handler.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/drag_utils.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/view.h"
#include "ui/views/view_constants.h"
#include "ui/views/view_tracker.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/aura/window_tree_host.h"
#include "ui/base/win/internal_constants.h"
#include "ui/display/win/screen_win.h"
#include "ui/views/win/hwnd_util.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#endif

using base::TimeDelta;
using ui::OSExchangeData;

namespace views {

namespace {

#if defined(OS_MACOSX)
bool AcceleratorShouldCancelMenu(const ui::Accelerator& accelerator) {
  // Since AcceleratorShouldCancelMenu() is called quite early in key
  // event handling, it is actually invoked for modifier keys themselves
  // changing. In that case, the key code reflects that the modifier key is
  // being pressed/released. We should never treat those presses as
  // accelerators, so bail out here.
  //
  // Also, we have to check for VKEY_SHIFT here even though we don't check
  // IsShiftDown() - otherwise this sequence of keypresses will dismiss the
  // menu:
  //   Press Cmd
  //   Press Shift
  // Which makes it impossible to use menus that have Cmd-Shift accelerators.
  if (accelerator.key_code() == ui::VKEY_CONTROL ||
      accelerator.key_code() == ui::VKEY_MENU ||  // aka Alt
      accelerator.key_code() == ui::VKEY_COMMAND ||
      accelerator.key_code() == ui::VKEY_SHIFT) {
    return false;
  }

  // Using an accelerator on Mac closes any open menu. Note that Mac behavior is
  // different between context menus (which block use of accelerators) and other
  // types of menus, which close when an accelerator is sent and do repost the
  // accelerator. In MacViews, this happens naturally because context menus are
  // (modal) Cocoa menus and other menus are Views menus, which will go through
  // this code path.
  return accelerator.IsCtrlDown() || accelerator.IsAltDown() ||
         accelerator.IsCmdDown();
}
#endif

// The amount of time the mouse should be down before a mouse release is
// considered intentional. This is to prevent spurious mouse releases from
// activating controls, especially when some UI element is revealed under the
// source of the activation (ex. menus showing underneath menu buttons).
base::TimeDelta menu_selection_hold_time =
    base::TimeDelta::FromMilliseconds(200);

// Amount of time from when the drop exits the menu and the menu is hidden.
constexpr int kCloseOnExitTime = 1200;

// If a context menu is invoked by touch, we shift the menu by this offset so
// that the finger does not obscure the menu.
constexpr int kTouchYPadding = 15;

// The spacing offset for the bubble tip.
constexpr int kBubbleTipSizeLeftRight = 12;
constexpr int kBubbleTipSizeTopBottom = 11;

// The maximum distance (in DIPS) that the mouse can be moved before it should
// trigger a mouse menu item activation (regardless of how long the menu has
// been showing).
constexpr float kMaximumLengthMovedToActivate = 4.0f;

// Time to complete a cycle of the menu item alert animation.
constexpr base::TimeDelta kAlertAnimationThrobDuration =
    base::TimeDelta::FromMilliseconds(1000);

// Returns true if the mnemonic of |menu| matches key.
bool MatchesMnemonic(MenuItemView* menu, base::char16 key) {
  return key != 0 && menu->GetMnemonic() == key;
}

// Returns true if |menu| doesn't have a mnemonic and first character of the its
// title is |key|.
bool TitleMatchesMnemonic(MenuItemView* menu, base::char16 key) {
  if (menu->GetMnemonic())
    return false;

  base::string16 lower_title = base::i18n::ToLower(menu->title());
  return !lower_title.empty() && lower_title[0] == key;
}

// Returns the first descendant of |view| that is hot tracked.
Button* GetFirstHotTrackedView(View* view) {
  if (!view)
    return nullptr;
  Button* button = Button::AsButton(view);
  if (button && button->IsHotTracked())
    return button;

  for (View* child : view->children()) {
    Button* hot_view = GetFirstHotTrackedView(child);
    if (hot_view)
      return hot_view;
  }
  return nullptr;
}

// Recurses through the child views of |view| returning the first view starting
// at |pos| that is focusable. Children are considered first to last.
// TODO(https://crbug.com/942358): This can also return |view|, which seems
// incorrect.
View* GetFirstFocusableViewForward(View* view,
                                   View::Views::const_iterator pos) {
  for (auto i = pos; i != view->children().cend(); ++i) {
    View* deepest = GetFirstFocusableViewForward(*i, (*i)->children().cbegin());
    if (deepest)
      return deepest;
  }
  return view->IsFocusable() ? view : nullptr;
}

// As GetFirstFocusableViewForward(), but children are considered last to first.
View* GetFirstFocusableViewBackward(View* view,
                                    View::Views::const_reverse_iterator pos) {
  for (auto i = pos; i != view->children().crend(); ++i) {
    View* deepest =
        GetFirstFocusableViewBackward(*i, (*i)->children().crbegin());
    if (deepest)
      return deepest;
  }
  return view->IsFocusable() ? view : nullptr;
}

// Returns the first child of |start| that is focusable.
View* GetInitialFocusableView(View* start, bool forward) {
  const auto& children = start->children();
  return forward ? GetFirstFocusableViewForward(start, children.cbegin())
                 : GetFirstFocusableViewBackward(start, children.crbegin());
}

// Returns the next view after |start_at| that is focusable. Returns null if
// there are no focusable children of |ancestor| after |start_at|.
View* GetNextFocusableView(View* ancestor, View* start_at, bool forward) {
  DCHECK(ancestor->Contains(start_at));
  View* parent = start_at;
  do {
    View* new_parent = parent->parent();
    const auto pos = new_parent->FindChild(parent);
    // Subtle: make_reverse_iterator() will result in an iterator that refers to
    // the element before its argument, which is what we want.
    View* next = forward
                     ? GetFirstFocusableViewForward(new_parent, std::next(pos))
                     : GetFirstFocusableViewBackward(
                           new_parent, std::make_reverse_iterator(pos));
    if (next)
      return next;
    parent = new_parent;
  } while (parent != ancestor);
  return nullptr;
}

#if defined(OS_WIN)
// Determines the correct coordinates and window to repost |event| to, if it is
// a mouse or touch event.
static void RepostEventImpl(const ui::LocatedEvent* event,
                            const gfx::Point& screen_loc,
                            gfx::NativeView native_view,
                            gfx::NativeWindow window) {
  if (!event->IsMouseEvent() && !event->IsTouchEvent()) {
    // TODO(rbyers): Gesture event repost is tricky to get right
    // crbug.com/170987.
    DCHECK(event->IsGestureEvent());
    return;
  }

  if (!native_view)
    return;

#if defined(OS_WIN)
  gfx::Point screen_loc_pixels =
      display::win::ScreenWin::DIPToScreenPoint(screen_loc);
  HWND target_window = ::WindowFromPoint(screen_loc_pixels.ToPOINT());
  // If we don't find a native window for the HWND at the current location,
  // then attempt to find a native window from its parent if one exists.
  // There are HWNDs created outside views, which don't have associated
  // native windows.
  if (!window) {
    HWND parent = ::GetParent(target_window);
    if (parent) {
      aura::WindowTreeHost* host =
          aura::WindowTreeHost::GetForAcceleratedWidget(parent);
      if (host) {
        target_window = parent;
        window = host->window();
      }
    }
  }
  // Convert screen_loc to pixels for the Win32 API's like WindowFromPoint,
  // PostMessage/SendMessage to work correctly. These API's expect the
  // coordinates to be in pixels.
  if (event->IsMouseEvent()) {
    HWND source_window = HWNDForNativeView(native_view);
    if (!target_window || !source_window ||
        GetWindowThreadProcessId(source_window, nullptr) !=
            GetWindowThreadProcessId(target_window, nullptr)) {
      // Even though we have mouse capture, windows generates a mouse event if
      // the other window is in a separate thread. Only repost an event if
      // |target_window| and |source_window| were created on the same thread,
      // else double events can occur and lead to bad behavior.
      return;
    }

    // Determine whether the click was in the client area or not.
    // NOTE: WM_NCHITTEST coordinates are relative to the screen.
    LPARAM coords = MAKELPARAM(screen_loc_pixels.x(), screen_loc_pixels.y());
    LRESULT nc_hit_result = SendMessage(target_window, WM_NCHITTEST, 0, coords);
    const bool client_area = nc_hit_result == HTCLIENT;

    // TODO(sky): this isn't right. The event to generate should correspond with
    // the event we just got. MouseEvent only tells us what is down, which may
    // differ. Need to add ability to get changed button from MouseEvent.
    int event_type;
    int flags = event->flags();
    if (flags & ui::EF_LEFT_MOUSE_BUTTON) {
      event_type = client_area ? WM_LBUTTONDOWN : WM_NCLBUTTONDOWN;
    } else if (flags & ui::EF_MIDDLE_MOUSE_BUTTON) {
      event_type = client_area ? WM_MBUTTONDOWN : WM_NCMBUTTONDOWN;
    } else if (flags & ui::EF_RIGHT_MOUSE_BUTTON) {
      event_type = client_area ? WM_RBUTTONDOWN : WM_NCRBUTTONDOWN;
    } else {
      NOTREACHED();
      return;
    }

    int window_x = screen_loc_pixels.x();
    int window_y = screen_loc_pixels.y();
    if (client_area) {
      POINT pt = {window_x, window_y};
      ScreenToClient(target_window, &pt);
      window_x = pt.x;
      window_y = pt.y;
    }

    WPARAM target = client_area ? event->native_event().wParam : nc_hit_result;
    LPARAM window_coords = MAKELPARAM(window_x, window_y);
    PostMessage(target_window, event_type, target, window_coords);
    return;
  }
#endif  // defined(OS_WIN)

#if defined(USE_AURA)
  if (!window)
    return;

  aura::Window* root = window->GetRootWindow();
  aura::client::ScreenPositionClient* spc =
      aura::client::GetScreenPositionClient(root);
  if (!spc)
    return;

  gfx::Point root_loc(screen_loc);
  spc->ConvertPointFromScreen(root, &root_loc);

  std::unique_ptr<ui::Event> clone = ui::Event::Clone(*event);
  std::unique_ptr<ui::LocatedEvent> located_event(
      static_cast<ui::LocatedEvent*>(clone.release()));
  located_event->set_location(root_loc);
  located_event->set_root_location(root_loc);

  root->GetHost()->dispatcher()->RepostEvent(located_event.get());
#endif  // defined(USE_AURA)
}
#endif  // defined(OS_WIN)

}  // namespace

// MenuScrollTask --------------------------------------------------------------

// MenuScrollTask is used when the SubmenuView does not all fit on screen and
// the mouse is over the scroll up/down buttons. MenuScrollTask schedules
// itself with a RepeatingTimer. When Run is invoked MenuScrollTask scrolls
// appropriately.

class MenuController::MenuScrollTask {
 public:
  MenuScrollTask() {
    pixels_per_second_ = MenuItemView::pref_menu_height() * 20;
  }

  void Update(const MenuController::MenuPart& part) {
    if (!part.is_scroll()) {
      StopScrolling();
      return;
    }
    DCHECK(part.submenu);
    SubmenuView* new_menu = part.submenu;
    bool new_is_up = (part.type == MenuController::MenuPart::SCROLL_UP);
    if (new_menu == submenu_ && is_scrolling_up_ == new_is_up)
      return;

    start_scroll_time_ = base::Time::Now();
    start_y_ = part.submenu->GetVisibleBounds().y();
    submenu_ = new_menu;
    is_scrolling_up_ = new_is_up;

    if (!scrolling_timer_.IsRunning()) {
      scrolling_timer_.Start(FROM_HERE, TimeDelta::FromMilliseconds(30), this,
                             &MenuScrollTask::Run);
    }
  }

  void StopScrolling() {
    if (scrolling_timer_.IsRunning()) {
      scrolling_timer_.Stop();
      submenu_ = nullptr;
    }
  }

  // The menu being scrolled. Returns null if not scrolling.
  SubmenuView* submenu() const { return submenu_; }

 private:
  void Run() {
    DCHECK(submenu_);
    gfx::Rect vis_rect = submenu_->GetVisibleBounds();
    const int delta_y = static_cast<int>(
        (base::Time::Now() - start_scroll_time_).InMilliseconds() *
        pixels_per_second_ / 1000);
    vis_rect.set_y(is_scrolling_up_
                       ? std::max(0, start_y_ - delta_y)
                       : std::min(submenu_->height() - vis_rect.height(),
                                  start_y_ + delta_y));
    submenu_->ScrollRectToVisible(vis_rect);
  }

  // SubmenuView being scrolled.
  SubmenuView* submenu_ = nullptr;

  // Direction scrolling.
  bool is_scrolling_up_ = false;

  // Timer to periodically scroll.
  base::RepeatingTimer scrolling_timer_;

  // Time we started scrolling at.
  base::Time start_scroll_time_;

  // How many pixels to scroll per second.
  int pixels_per_second_;

  // Y-coordinate of submenu_view_ when scrolling started.
  int start_y_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MenuScrollTask);
};

// MenuController:SelectByCharDetails ----------------------------------------

struct MenuController::SelectByCharDetails {
  SelectByCharDetails() = default;

  // Index of the first menu with the specified mnemonic.
  int first_match = -1;

  // If true there are multiple menu items with the same mnemonic.
  bool has_multiple = false;

  // Index of the selected item; may remain -1.
  int index_of_item = -1;

  // If there are multiple matches this is the index of the item after the
  // currently selected item whose mnemonic matches. This may remain -1 even
  // though there are matches.
  int next_match = -1;
};

// MenuController:State ------------------------------------------------------

MenuController::State::State() = default;

MenuController::State::State(const State& other) = default;

MenuController::State::~State() = default;

// MenuController ------------------------------------------------------------

// static
MenuController* MenuController::active_instance_ = nullptr;

// static
MenuController* MenuController::GetActiveInstance() {
  return active_instance_;
}

void MenuController::Run(Widget* parent,
                         MenuButtonController* button_controller,
                         MenuItemView* root,
                         const gfx::Rect& bounds,
                         MenuAnchorPosition position,
                         bool context_menu,
                         bool is_nested_drag) {
  exit_type_ = ExitType::kNone;
  possible_drag_ = false;
  drag_in_progress_ = false;
  did_initiate_drag_ = false;
  closing_event_time_ = base::TimeTicks();
  menu_start_time_ = base::TimeTicks::Now();
  menu_start_mouse_press_loc_ = gfx::Point();

  ui::Event* event = nullptr;
  if (parent) {
    View* root_view = parent->GetRootView();
    if (root_view) {
      event = static_cast<internal::RootView*>(root_view)->current_event();
      if (event && event->type() == ui::ET_MOUSE_PRESSED) {
        gfx::Point screen_loc(
            static_cast<const ui::MouseEvent*>(event)->location());
        View::ConvertPointToScreen(static_cast<View*>(event->target()),
                                   &screen_loc);
        menu_start_mouse_press_loc_ = screen_loc;
      }
    }
  }

  // If we are already showing, this new menu is being nested. Such as context
  // menus on top of normal menus.
  if (showing_) {
    // Nesting (context menus) is not used for drag and drop.
    DCHECK(!for_drop_);

    state_.hot_button = hot_button_;
    hot_button_ = nullptr;
    // We're already showing, push the current state.
    menu_stack_.emplace_back(state_, std::move(pressed_lock_));

    // The context menu should be owned by the same parent.
    DCHECK_EQ(owner_, parent);
  } else {
    showing_ = true;

    if (owner_)
      owner_->RemoveObserver(this);
    owner_ = parent;
    if (owner_)
      owner_->AddObserver(this);

    // Only create a MenuPreTargetHandler for non-nested menus. Nested menus
    // will use the existing one.
    menu_pre_target_handler_ = MenuPreTargetHandler::Create(this, owner_);
  }

#if defined(OS_MACOSX)
  menu_cocoa_watcher_ = std::make_unique<MenuCocoaWatcherMac>(base::BindOnce(
      &MenuController::Cancel, this->AsWeakPtr(), ExitType::kAll));
#endif

  // Reset current state.
  pending_state_ = State();
  state_ = State();
  UpdateInitialLocation(bounds, position, context_menu);

  // Set the selection, which opens the initial menu.
  SetSelection(root, SELECTION_OPEN_SUBMENU | SELECTION_UPDATE_IMMEDIATELY);

  if (button_controller) {
    pressed_lock_ = button_controller->TakeLock(
        false, ui::LocatedEvent::FromIfValid(event));
  }

  if (for_drop_) {
    if (!is_nested_drag) {
      // Start the timer to hide the menu. This is needed as we get no
      // notification when the drag has finished.
      StartCancelAllTimer();
    }
    return;
  }

  // Make sure Chrome doesn't attempt to shut down while the menu is showing.
  ViewsDelegate::GetInstance()->AddRef();
}

void MenuController::Cancel(ExitType type) {
#if defined(OS_MACOSX)
  menu_closure_animation_.reset();
#endif

  // If the menu has already been destroyed, no further cancellation is
  // needed.  We especially don't want to set the |exit_type_| to a lesser
  // value.
  if (exit_type_ == ExitType::kDestroyed || exit_type_ == type)
    return;

  if (!showing_) {
    // This occurs if we're in the process of notifying the delegate for a drop
    // and the delegate cancels us. Or if the releasing of ViewsDelegate causes
    // an immediate shutdown.
    return;
  }

  MenuItemView* selected = state_.item;
  SetExitType(type);

  SendMouseCaptureLostToActiveView();

  // Hide windows immediately.
  SetSelection(nullptr, SELECTION_UPDATE_IMMEDIATELY | SELECTION_EXIT);

  if (for_drop_) {
    // If we didn't block the caller we need to notify the menu, which
    // triggers deleting us.
    DCHECK(selected);
    showing_ = false;
    delegate_->OnMenuClosed(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
                            selected->GetRootMenuItem(), accept_event_flags_);
    // WARNING: the call to MenuClosed deletes us.
    return;
  }

  // If |type| is ExitType::kAll we update the state of the menu to not showing.
  // For dragging this ensures that the correct visual state is reported until
  // the drag operation completes. For non-dragging cases it is possible that
  // the release of ViewsDelegate leads immediately to shutdown, which can
  // trigger nested calls to Cancel. We want to reject these to prevent
  // attempting a nested tear down of this and |delegate_|.
  if (type == ExitType::kAll)
    showing_ = false;

  // On Windows and Linux the destruction of this menu's Widget leads to the
  // teardown of the platform specific drag-and-drop Widget. Do not shutdown
  // while dragging, leave the Widget hidden until drag-and-drop has completed,
  // at which point all menus will be destroyed.
  if (!drag_in_progress_)
    ExitMenu();
}

void MenuController::AddNestedDelegate(
    internal::MenuControllerDelegate* delegate) {
  delegate_stack_.push_back(delegate);
  delegate_ = delegate;
}

bool MenuController::IsCombobox() const {
  return IsEditableCombobox() || IsReadonlyCombobox();
}

bool MenuController::IsEditableCombobox() const {
  return combobox_type_ == ComboboxType::kEditable;
}

bool MenuController::IsReadonlyCombobox() const {
  return combobox_type_ == ComboboxType::kReadonly;
}

bool MenuController::IsContextMenu() const {
  return state_.context_menu;
}

bool MenuController::OnMousePressed(SubmenuView* source,
                                    const ui::MouseEvent& event) {
  // We should either have no current_mouse_event_target_, or should have a
  // pressed state stored.
  DCHECK(!current_mouse_event_target_ || current_mouse_pressed_state_);

  // Find the root view to check. If any buttons were previously pressed, this
  // is the same root view we've been forwarding to. Otherwise, it's the root
  // view of the target.
  MenuHostRootView* forward_to_root =
      current_mouse_pressed_state_ ? current_mouse_event_target_
                                   : GetRootView(source, event.location());

  current_mouse_pressed_state_ |= event.changed_button_flags();

  if (forward_to_root) {
    ui::MouseEvent event_for_root(event);
    // Reset hot-tracking if a different view is getting a mouse press.
    ConvertLocatedEventForRootView(source, forward_to_root, &event_for_root);
    View* view =
        forward_to_root->GetEventHandlerForPoint(event_for_root.location());
    Button* button = Button::AsButton(view);
    if (hot_button_ != button)
      SetHotTrackedButton(button);

    // Empty menu items are always handled by the menu controller.
    if (!view || view->GetID() != MenuItemView::kEmptyMenuItemViewID) {
      base::WeakPtr<MenuController> this_ref = AsWeakPtr();
      bool processed = forward_to_root->ProcessMousePressed(event_for_root);
      // This object may be destroyed as a result of a mouse press event (some
      // item may close the menu).
      if (!this_ref)
        return true;

      // If the event was processed, the root view becomes our current mouse
      // handler...
      if (processed && !current_mouse_event_target_) {
        current_mouse_event_target_ = forward_to_root;
      }

      // ...and we always return the result of the current handler.
      if (current_mouse_event_target_)
        return processed;
    }
  }

  // Otherwise, the menu handles this click directly.
  SetSelectionOnPointerDown(source, &event);
  return true;
}

bool MenuController::OnMouseDragged(SubmenuView* source,
                                    const ui::MouseEvent& event) {
  if (current_mouse_event_target_) {
    ui::MouseEvent event_for_root(event);
    ConvertLocatedEventForRootView(source, current_mouse_event_target_,
                                   &event_for_root);
    return current_mouse_event_target_->ProcessMouseDragged(event_for_root);
  }

  MenuPart part = GetMenuPart(source, event.location());
  UpdateScrolling(part);

  if (for_drop_)
    return false;

  if (possible_drag_) {
    if (View::ExceededDragThreshold(event.location() - press_pt_))
      StartDrag(source, press_pt_);
    return true;
  }
  MenuItemView* mouse_menu = nullptr;
  if (part.type == MenuPart::MENU_ITEM) {
    // If there is no menu target, but a submenu target, then we are interacting
    // with an empty menu item within a submenu. These cannot become selection
    // targets for mouse interaction, so do not attempt to update selection.
    if (part.menu || !part.submenu) {
      if (!part.menu)
        part.menu = source->GetMenuItem();
      else
        mouse_menu = part.menu;
      SetSelection(part.menu ? part.menu : state_.item, SELECTION_OPEN_SUBMENU);
    }
  } else if (part.type == MenuPart::NONE) {
    // If there is a sibling menu, show it. Otherwise, if the user has selected
    // a menu item with no accompanying sibling menu or submenu, move selection
    // back to the parent menu item.
    if (!ShowSiblingMenu(source, event.location())) {
      if (!part.is_scroll() && pending_state_.item &&
          pending_state_.item->GetParentMenuItem() &&
          !pending_state_.item->SubmenuIsShowing()) {
        SetSelection(pending_state_.item->GetParentMenuItem(),
                     SELECTION_OPEN_SUBMENU);
      }
    }
  }
  UpdateActiveMouseView(source, event, mouse_menu);

  return true;
}

void MenuController::OnMouseReleased(SubmenuView* source,
                                     const ui::MouseEvent& event) {
  current_mouse_pressed_state_ &= ~event.changed_button_flags();

  if (current_mouse_event_target_) {
    // If this was the final mouse button, then remove the forwarding target.
    // We need to do this *before* dispatching the event to the root view
    // because there's a chance that the event will open a nested (and blocking)
    // menu, and we need to not have a forwarded root view.
    MenuHostRootView* cached_event_target = current_mouse_event_target_;
    if (!current_mouse_pressed_state_)
      current_mouse_event_target_ = nullptr;
    ui::MouseEvent event_for_root(event);
    ConvertLocatedEventForRootView(source, cached_event_target,
                                   &event_for_root);
    cached_event_target->ProcessMouseReleased(event_for_root);
    return;
  }

  if (for_drop_)
    return;

  DCHECK(state_.item);
  possible_drag_ = false;
  MenuPart part = GetMenuPart(source, event.location());
  if (event.IsRightMouseButton() && part.type == MenuPart::MENU_ITEM) {
    MenuItemView* menu = part.menu;
    // |menu| is null means this event is from an empty menu or a separator.
    // If it is from an empty menu, use parent context menu instead of that.
    if (!menu && part.submenu->children().size() == 1 &&
        part.submenu->children().front()->GetID() ==
            MenuItemView::kEmptyMenuItemViewID) {
      menu = part.parent;
    }

    if (menu) {
      gfx::Point screen_location(event.location());
      View::ConvertPointToScreen(source->GetScrollViewContainer(),
                                 &screen_location);
      if (ShowContextMenu(menu, screen_location, ui::MENU_SOURCE_MOUSE))
        return;
    }
  }

  // A plain left click on a folder that has children serves to open that folder
  // by setting the selection, rather than executing a command via the delegate
  // or doing anything else.
  // TODO(ellyjones): Why isn't a case needed here for EF_CONTROL_DOWN?
  bool plain_left_click_with_children =
      part.should_submenu_show && part.menu && part.menu->HasSubmenu() &&
      (event.flags() & ui::EF_LEFT_MOUSE_BUTTON) &&
      !(event.flags() & ui::EF_COMMAND_DOWN);
  if (!part.is_scroll() && part.menu && !plain_left_click_with_children) {
    if (active_mouse_view_tracker_->view()) {
      SendMouseReleaseToActiveView(source, event);
      return;
    }
    // If a mouse release was received quickly after showing.
    base::TimeDelta time_shown = base::TimeTicks::Now() - menu_start_time_;
    if (time_shown < menu_selection_hold_time) {
      // And it wasn't far from the mouse press location.
      gfx::Point screen_loc(event.location());
      View::ConvertPointToScreen(source->GetScrollViewContainer(), &screen_loc);
      gfx::Vector2d moved = screen_loc - menu_start_mouse_press_loc_;
      if (moved.Length() < kMaximumLengthMovedToActivate) {
        // Ignore the mouse release as it was likely this menu was shown under
        // the mouse and the action was just a normal click.
        return;
      }
    }
    if (part.menu->GetDelegate()->ShouldExecuteCommandWithoutClosingMenu(
            part.menu->GetCommand(), event)) {
      part.menu->GetDelegate()->ExecuteCommand(part.menu->GetCommand(),
                                               event.flags());
      return;
    }
    if (!part.menu->NonIconChildViewsCount() &&
        part.menu->GetDelegate()->IsTriggerableEvent(part.menu, event)) {
      base::TimeDelta shown_time = base::TimeTicks::Now() - menu_start_time_;
      if (!state_.context_menu || !View::ShouldShowContextMenuOnMousePress() ||
          shown_time > menu_selection_hold_time) {
        Accept(part.menu, event.flags());
      }
      return;
    }
  } else if (part.type == MenuPart::MENU_ITEM) {
    // User either clicked on empty space, or a menu that has children.
    SetSelection(part.menu ? part.menu : state_.item,
                 SELECTION_OPEN_SUBMENU | SELECTION_UPDATE_IMMEDIATELY);
  }
  SendMouseCaptureLostToActiveView();
}

void MenuController::OnMouseMoved(SubmenuView* source,
                                  const ui::MouseEvent& event) {
  if (current_mouse_event_target_) {
    ui::MouseEvent event_for_root(event);
    ConvertLocatedEventForRootView(source, current_mouse_event_target_,
                                   &event_for_root);
    current_mouse_event_target_->ProcessMouseMoved(event_for_root);
    return;
  }

  // Ignore mouse move events whose location is the same as where the mouse
  // was when a menu was opened. This fixes the issue of opening a menu
  // with the keyboard and having the menu item under the current mouse
  // position incorrectly selected.
  if (menu_open_mouse_loc_ && *menu_open_mouse_loc_ == event.location())
    return;

  menu_open_mouse_loc_.reset();
  MenuHostRootView* root_view = GetRootView(source, event.location());
  if (root_view) {
    root_view->ProcessMouseMoved(event);

    // Update hot-tracked button when a button state is changed with a mouse
    // event. It is necessary to track it for accurate hot-tracking when both
    // mouse and keyboard are used to navigate the menu.
    ui::MouseEvent event_for_root(event);
    ConvertLocatedEventForRootView(source, root_view, &event_for_root);
    View* view = root_view->GetEventHandlerForPoint(event_for_root.location());
    Button* button = Button::AsButton(view);
    if (button)
      SetHotTrackedButton(button);
  }

  HandleMouseLocation(source, event.location());
}

void MenuController::OnMouseEntered(SubmenuView* source,
                                    const ui::MouseEvent& event) {
  // MouseEntered is always followed by a mouse moved, so don't need to
  // do anything here.
}

bool MenuController::OnMouseWheel(SubmenuView* source,
                                  const ui::MouseWheelEvent& event) {
  MenuPart part = GetMenuPart(source, event.location());

  SetSelection(part.menu ? part.menu : state_.item,
               SELECTION_OPEN_SUBMENU | SELECTION_UPDATE_IMMEDIATELY);

  return part.submenu && part.submenu->OnMouseWheel(event);
}

void MenuController::OnGestureEvent(SubmenuView* source,
                                    ui::GestureEvent* event) {
  if (owner_ && send_gesture_events_to_owner()) {
#if defined(OS_MACOSX)
    NOTIMPLEMENTED();
#else   // !defined(OS_MACOSX)
    event->ConvertLocationToTarget(source->GetWidget()->GetNativeWindow(),
                                   owner()->GetNativeWindow());
#endif  // defined(OS_MACOSX)
    owner()->OnGestureEvent(event);
    // Reset |send_gesture_events_to_owner_| when the first gesture ends.
    if (event->type() == ui::ET_GESTURE_END)
      send_gesture_events_to_owner_ = false;
    return;
  }

  MenuHostRootView* root_view = GetRootView(source, event->location());
  if (root_view) {
    // Reset hot-tracking if a different view is getting a touch event.
    ui::GestureEvent event_for_root(*event);
    ConvertLocatedEventForRootView(source, root_view, &event_for_root);
    View* view = root_view->GetEventHandlerForPoint(event_for_root.location());
    Button* button = Button::AsButton(view);
    if (hot_button_ && hot_button_ != button)
      SetHotTrackedButton(nullptr);
  }

  MenuPart part = GetMenuPart(source, event->location());
  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    SetSelectionOnPointerDown(source, event);
    event->StopPropagation();
  } else if (event->type() == ui::ET_GESTURE_LONG_PRESS) {
    if (part.type == MenuPart::MENU_ITEM && part.menu) {
      gfx::Point screen_location(event->location());
      View::ConvertPointToScreen(source->GetScrollViewContainer(),
                                 &screen_location);
      if (ShowContextMenu(part.menu, screen_location, ui::MENU_SOURCE_TOUCH))
        event->StopPropagation();
    }
  } else if (event->type() == ui::ET_GESTURE_TAP) {
    if (!part.is_scroll() && part.menu &&
        !(part.should_submenu_show && part.menu->HasSubmenu())) {
      if (part.menu->GetDelegate()->IsTriggerableEvent(part.menu, *event)) {
        item_selected_by_touch_ = true;
        Accept(part.menu, event->flags());
      }
      event->StopPropagation();
    } else if (part.type == MenuPart::MENU_ITEM) {
      // User either tapped on empty space, or a menu that has children.
      SetSelection(part.menu ? part.menu : state_.item,
                   SELECTION_OPEN_SUBMENU | SELECTION_UPDATE_IMMEDIATELY);
      event->StopPropagation();
    }
  } else if (event->type() == ui::ET_GESTURE_TAP_CANCEL && part.menu &&
             part.type == MenuPart::MENU_ITEM) {
    // Move the selection to the parent menu so that the selection in the
    // current menu is unset. Make sure the submenu remains open by sending the
    // appropriate SetSelectionTypes flags.
    SetSelection(part.menu->GetParentMenuItem(),
                 SELECTION_OPEN_SUBMENU | SELECTION_UPDATE_IMMEDIATELY);
    event->StopPropagation();
  }

  if (event->stopped_propagation())
    return;

  if (!part.submenu)
    return;
  part.submenu->OnGestureEvent(event);
}

void MenuController::OnTouchEvent(SubmenuView* source, ui::TouchEvent* event) {
  // Bail if owner wants the current active gesture sequence.
  if (owner_ && send_gesture_events_to_owner())
    return;

  if (event->type() == ui::ET_TOUCH_PRESSED) {
    MenuPart part = GetMenuPart(source, event->location());
    if (part.type == MenuPart::NONE) {
      RepostEventAndCancel(source, event);
      event->SetHandled();
    }
  }
}

View* MenuController::GetTooltipHandlerForPoint(SubmenuView* source,
                                                const gfx::Point& point) {
  MenuHostRootView* root_view = GetRootView(source, point);
  return root_view ? root_view->ProcessGetTooltipHandlerForPoint(point)
                   : nullptr;
}

void MenuController::ViewHierarchyChanged(
    SubmenuView* source,
    const ViewHierarchyChangedDetails& details) {
  if (!details.is_add) {
    // If the current mouse handler is removed, remove it as the handler.
    if (details.child == current_mouse_event_target_) {
      current_mouse_event_target_ = nullptr;
      current_mouse_pressed_state_ = 0;
    }
    // Update |hot_button_| (both in |this| and in |menu_stack_| if it gets
    // removed while a menu is up.
    if (details.child == hot_button_) {
      hot_button_ = nullptr;
      for (auto& nested_state : menu_stack_) {
        State& state = nested_state.first;
        if (details.child == state.hot_button)
          state.hot_button = nullptr;
      }
    }
  }
}

bool MenuController::GetDropFormats(
    SubmenuView* source,
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  return source->GetMenuItem()->GetDelegate()->GetDropFormats(
      source->GetMenuItem(), formats, format_types);
}

bool MenuController::AreDropTypesRequired(SubmenuView* source) {
  return source->GetMenuItem()->GetDelegate()->AreDropTypesRequired(
      source->GetMenuItem());
}

bool MenuController::CanDrop(SubmenuView* source, const OSExchangeData& data) {
  return source->GetMenuItem()->GetDelegate()->CanDrop(source->GetMenuItem(),
                                                       data);
}

void MenuController::OnDragEntered(SubmenuView* source,
                                   const ui::DropTargetEvent& event) {
  valid_drop_coordinates_ = false;
}

int MenuController::OnDragUpdated(SubmenuView* source,
                                  const ui::DropTargetEvent& event) {
  StopCancelAllTimer();

  gfx::Point screen_loc(event.location());
  View::ConvertPointToScreen(source, &screen_loc);
  if (valid_drop_coordinates_ && screen_loc == drop_pt_)
    return last_drop_operation_;
  drop_pt_ = screen_loc;
  valid_drop_coordinates_ = true;

  MenuItemView* menu_item = GetMenuItemAt(source, event.x(), event.y());
  bool over_empty_menu = false;
  if (!menu_item) {
    // See if we're over an empty menu.
    menu_item = GetEmptyMenuItemAt(source, event.x(), event.y());
    if (menu_item)
      over_empty_menu = true;
  }
  MenuDelegate::DropPosition drop_position = MenuDelegate::DropPosition::kNone;
  int drop_operation = ui::DragDropTypes::DRAG_NONE;
  if (menu_item) {
    gfx::Point menu_item_loc(event.location());
    View::ConvertPointToTarget(source, menu_item, &menu_item_loc);
    MenuItemView* query_menu_item;
    if (!over_empty_menu) {
      int menu_item_height = menu_item->height();
      if (menu_item->HasSubmenu() &&
          (menu_item_loc.y() > kDropBetweenPixels &&
           menu_item_loc.y() < (menu_item_height - kDropBetweenPixels))) {
        drop_position = MenuDelegate::DropPosition::kOn;
      } else {
        drop_position = (menu_item_loc.y() < menu_item_height / 2)
                            ? MenuDelegate::DropPosition::kBefore
                            : MenuDelegate::DropPosition::kAfter;
      }
      query_menu_item = menu_item;
    } else {
      query_menu_item = menu_item->GetParentMenuItem();
      drop_position = MenuDelegate::DropPosition::kOn;
    }
    drop_operation = menu_item->GetDelegate()->GetDropOperation(
        query_menu_item, event, &drop_position);

    // If the menu has a submenu, schedule the submenu to open.
    SetSelection(menu_item, menu_item->HasSubmenu() ? SELECTION_OPEN_SUBMENU
                                                    : SELECTION_DEFAULT);

    if (drop_position == MenuDelegate::DropPosition::kNone ||
        drop_operation == ui::DragDropTypes::DRAG_NONE)
      menu_item = nullptr;
  } else {
    SetSelection(source->GetMenuItem(), SELECTION_OPEN_SUBMENU);
  }
  SetDropMenuItem(menu_item, drop_position);
  last_drop_operation_ = drop_operation;
  return drop_operation;
}

void MenuController::OnDragExited(SubmenuView* source) {
  StartCancelAllTimer();

  if (drop_target_) {
    StopShowTimer();
    SetDropMenuItem(nullptr, MenuDelegate::DropPosition::kNone);
  }
}

int MenuController::OnPerformDrop(SubmenuView* source,
                                  const ui::DropTargetEvent& event) {
  DCHECK(drop_target_);
  // NOTE: the delegate may delete us after invoking OnPerformDrop, as such
  // we don't call cancel here.

  MenuItemView* item = state_.item;
  DCHECK(item);

  MenuItemView* drop_target = drop_target_;
  MenuDelegate::DropPosition drop_position = drop_position_;

  // Close all menus, including any nested menus.
  SetSelection(nullptr, SELECTION_UPDATE_IMMEDIATELY | SELECTION_EXIT);
  CloseAllNestedMenus();

  // Set state such that we exit.
  showing_ = false;
  SetExitType(ExitType::kAll);

  // If over an empty menu item, drop occurs on the parent.
  if (drop_target->GetID() == MenuItemView::kEmptyMenuItemViewID)
    drop_target = drop_target->GetParentMenuItem();

  if (for_drop_) {
    delegate_->OnMenuClosed(
        internal::MenuControllerDelegate::DONT_NOTIFY_DELEGATE,
        item->GetRootMenuItem(), accept_event_flags_);
  }

  // WARNING: the call to MenuClosed deletes us.

  return drop_target->GetDelegate()->OnPerformDrop(drop_target, drop_position,
                                                   event);
}

void MenuController::OnDragEnteredScrollButton(SubmenuView* source,
                                               bool is_up) {
  MenuPart part;
  part.type = is_up ? MenuPart::SCROLL_UP : MenuPart::SCROLL_DOWN;
  part.submenu = source;
  UpdateScrolling(part);

  // Do this to force the selection to hide.
  SetDropMenuItem(source->GetMenuItemAt(0), MenuDelegate::DropPosition::kNone);

  StopCancelAllTimer();
}

void MenuController::OnDragExitedScrollButton(SubmenuView* source) {
  StartCancelAllTimer();
  SetDropMenuItem(nullptr, MenuDelegate::DropPosition::kNone);
  StopScrolling();
}

void MenuController::OnDragWillStart() {
  DCHECK(!drag_in_progress_);
  drag_in_progress_ = true;
}

void MenuController::OnDragComplete(bool should_close) {
  DCHECK(drag_in_progress_);
  drag_in_progress_ = false;
  // During a drag, mouse events are processed directly by the widget, and not
  // sent to the MenuController. At drag completion, reset pressed state and
  // the event target.
  current_mouse_pressed_state_ = 0;
  current_mouse_event_target_ = nullptr;

  // Only attempt to close if the MenuHost said to.
  if (should_close) {
    if (showing_) {
      // During a drag operation there are several ways in which this can be
      // canceled and deleted. Verify that this is still active before closing
      // the widgets.
      if (GetActiveInstance() == this) {
        base::WeakPtr<MenuController> this_ref = AsWeakPtr();
        CloseAllNestedMenus();
        Cancel(ExitType::kAll);
        // The above may have deleted us. If not perform a full shutdown.
        if (!this_ref)
          return;
        ExitMenu();
      }
    } else if (exit_type_ == ExitType::kAll) {
      // We may have been canceled during the drag. If so we still need to fully
      // shutdown.
      ExitMenu();
    }
  }
}

ui::PostDispatchAction MenuController::OnWillDispatchKeyEvent(
    ui::KeyEvent* event) {
  if (exit_type() == ExitType::kAll || exit_type() == ExitType::kDestroyed) {
    // If the event has arrived after the menu's exit type has changed but
    // before its Widgets have been destroyed, the event will continue its
    // normal propagation for the following reason:
    // If the user accepts a menu item in a nested menu, and the menu item
    // action starts a base::RunLoop; IDC_BOOKMARK_BAR_OPEN_ALL sometimes opens
    // a modal dialog. The modal dialog starts a base::RunLoop and keeps the
    // base::RunLoop running for the duration of its lifetime.
    return ui::POST_DISPATCH_PERFORM_DEFAULT;
  }

  base::WeakPtr<MenuController> this_ref = AsWeakPtr();
  if (event->type() == ui::ET_KEY_PRESSED) {
    bool key_handled = false;
#if defined(OS_MACOSX)
    // Special handling for Option-Up and Option-Down, which should behave like
    // Home and End respectively in menus.
    if ((event->flags() & ui::EF_ALT_DOWN)) {
      if (event->key_code() == ui::VKEY_UP) {
        key_handled = OnKeyPressed(ui::VKEY_HOME);
      } else if (event->key_code() == ui::VKEY_DOWN) {
        key_handled = OnKeyPressed(ui::VKEY_END);
      } else {
        key_handled = OnKeyPressed(event->key_code());
      }
    } else {
      key_handled = OnKeyPressed(event->key_code());
    }
#else
    key_handled = OnKeyPressed(event->key_code());
#endif

    if (key_handled)
      event->StopPropagation();

    // Key events can lead to this being deleted.
    if (!this_ref) {
      event->StopPropagation();
      return ui::POST_DISPATCH_NONE;
    }

    if (!IsEditableCombobox() && !event->stopped_propagation()) {
      // Do not check mnemonics if the Alt or Ctrl modifiers are pressed. For
      // example Ctrl+<T> is an accelerator, but <T> only is a mnemonic.
      const int kKeyFlagsMask =
          ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN;
      const int flags = event->flags();
      if (exit_type() == ExitType::kNone && (flags & kKeyFlagsMask) == 0) {
        base::char16 c = event->GetCharacter();
        SelectByChar(c);
        // SelectByChar can lead to this being deleted.
        if (!this_ref) {
          event->StopPropagation();
          return ui::POST_DISPATCH_NONE;
        }
      }
    }
  }

  ui::Accelerator accelerator(*event);

#if defined(OS_MACOSX)
  if (AcceleratorShouldCancelMenu(accelerator)) {
    Cancel(ExitType::kAll);
    return ui::POST_DISPATCH_PERFORM_DEFAULT;
  }
#endif

  ViewsDelegate::ProcessMenuAcceleratorResult result =
      ViewsDelegate::GetInstance()->ProcessAcceleratorWhileMenuShowing(
          accelerator);
  // Above can lead to |this| being deleted.
  if (!this_ref) {
    event->StopPropagation();
    return ui::POST_DISPATCH_NONE;
  }

  if (result == ViewsDelegate::ProcessMenuAcceleratorResult::CLOSE_MENU) {
    Cancel(ExitType::kAll);
    event->StopPropagation();
    return ui::POST_DISPATCH_NONE;
  }

  if (IsEditableCombobox()) {
    const base::flat_set<ui::KeyboardCode> kKeysThatDontPropagate = {
        ui::VKEY_DOWN, ui::VKEY_UP, ui::VKEY_ESCAPE, ui::VKEY_F4,
        ui::VKEY_RETURN};
    if (kKeysThatDontPropagate.find(event->key_code()) ==
        kKeysThatDontPropagate.end())
      return ui::POST_DISPATCH_PERFORM_DEFAULT;
  }
  event->StopPropagation();
  return ui::POST_DISPATCH_NONE;
}

void MenuController::UpdateSubmenuSelection(SubmenuView* submenu) {
  if (submenu->IsShowing()) {
    gfx::Point point = display::Screen::GetScreen()->GetCursorScreenPoint();
    const SubmenuView* root_submenu =
        submenu->GetMenuItem()->GetRootMenuItem()->GetSubmenu();
    View::ConvertPointFromScreen(root_submenu->GetWidget()->GetRootView(),
                                 &point);
    HandleMouseLocation(submenu, point);
  }
}

void MenuController::OnWidgetDestroying(Widget* widget) {
  DCHECK_EQ(owner_, widget);
  owner_->RemoveObserver(this);
  owner_ = nullptr;
}

bool MenuController::IsCancelAllTimerRunningForTest() {
  return cancel_all_timer_.IsRunning();
}

void MenuController::ClearStateForTest() {
  state_ = State();
  pending_state_ = State();
}

// static
void MenuController::TurnOffMenuSelectionHoldForTest() {
  menu_selection_hold_time = base::TimeDelta();
}

void MenuController::OnMenuItemDestroying(MenuItemView* menu_item) {
#if defined(OS_MACOSX)
  if (menu_closure_animation_ && menu_closure_animation_->item() == menu_item)
    menu_closure_animation_.reset();
#endif
  UnregisterAlertedItem(menu_item);
}

void MenuController::AnimationProgressed(const gfx::Animation* animation) {
  DCHECK_EQ(animation, &alert_animation_);

  // Schedule paints at each alerted menu item. The menu items pull the
  // animation's current value in their OnPaint methods.
  for (MenuItemView* item : alerted_items_) {
    if (item->GetParentMenuItem()->SubmenuIsShowing())
      item->SchedulePaint();
  }
}

void MenuController::SetSelection(MenuItemView* menu_item,
                                  int selection_types) {
  size_t paths_differ_at = 0;
  std::vector<MenuItemView*> current_path;
  std::vector<MenuItemView*> new_path;
  BuildPathsAndCalculateDiff(pending_state_.item, menu_item, &current_path,
                             &new_path, &paths_differ_at);

  size_t current_size = current_path.size();
  size_t new_size = new_path.size();

  // ACTIONABLE_SUBMENUs can change without changing the pending item, this
  // occurs when selection moves from the COMMAND area to the SUBMENU area of
  // the ACTIONABLE_SUBMENU.
  const bool pending_item_changed =
      pending_state_.item != menu_item ||
      pending_state_.submenu_open !=
          !!(selection_types & SELECTION_OPEN_SUBMENU);

  if (pending_item_changed && pending_state_.item)
    SetHotTrackedButton(nullptr);

  // Notify the old path it isn't selected.
  MenuDelegate* current_delegate =
      current_path.empty() ? nullptr : current_path.front()->GetDelegate();
  for (size_t i = paths_differ_at; i < current_size; ++i) {
    if (current_delegate &&
        (current_path[i]->GetType() == MenuItemView::SUBMENU ||
         current_path[i]->GetType() == MenuItemView::ACTIONABLE_SUBMENU)) {
      current_delegate->WillHideMenu(current_path[i]);
    }
    current_path[i]->SetSelected(false);
  }

  // Notify the new path it is selected.
  for (size_t i = paths_differ_at; i < new_size; ++i) {
    new_path[i]->ScrollRectToVisible(new_path[i]->GetLocalBounds());
    new_path[i]->SetSelected(true);
    if (new_path[i]->GetType() == MenuItemView::ACTIONABLE_SUBMENU) {
      new_path[i]->SetSelectionOfActionableSubmenu(
          (selection_types & SELECTION_OPEN_SUBMENU) != 0);
    }
  }
  if (menu_item && menu_item->GetType() == MenuItemView::ACTIONABLE_SUBMENU) {
    menu_item->SetSelectionOfActionableSubmenu(
        (selection_types & SELECTION_OPEN_SUBMENU) != 0);
  }

  DCHECK(menu_item || (selection_types & SELECTION_EXIT) != 0);

  pending_state_.item = menu_item;
  pending_state_.submenu_open = (selection_types & SELECTION_OPEN_SUBMENU) != 0;

  // Stop timers.
  StopCancelAllTimer();
  // Resets show timer only when pending menu item is changed.
  if (pending_item_changed)
    StopShowTimer();

  if (selection_types & SELECTION_UPDATE_IMMEDIATELY)
    CommitPendingSelection();
  else if (pending_item_changed)
    StartShowTimer();

  // Notify an accessibility focus event on all menu items except for the root.
  if (menu_item && (MenuDepth(menu_item) != 1 ||
                    menu_item->GetType() != MenuItemView::SUBMENU ||
                    (menu_item->GetType() == MenuItemView::ACTIONABLE_SUBMENU &&
                     (selection_types & SELECTION_OPEN_SUBMENU) == 0))) {
    menu_item->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
    // Notify an accessibility selected children changed event on the parent
    // submenu.
    if (menu_item->GetParentMenuItem() &&
        menu_item->GetParentMenuItem()->GetSubmenu()) {
      menu_item->GetParentMenuItem()->GetSubmenu()->NotifyAccessibilityEvent(
          ax::mojom::Event::kSelectedChildrenChanged,
          true /* send_native_event */);
    }
  }
}

void MenuController::SetSelectionOnPointerDown(SubmenuView* source,
                                               const ui::LocatedEvent* event) {
  if (for_drop_)
    return;

  DCHECK(!active_mouse_view_tracker_->view());

  MenuPart part = GetMenuPart(source, event->location());
  if (part.is_scroll())
    return;  // Ignore presses on scroll buttons.

  // When this menu is opened through a touch event, a simulated right-click
  // is sent before the menu appears.  Ignore it.
  if ((event->flags() & ui::EF_RIGHT_MOUSE_BUTTON) &&
      (event->flags() & ui::EF_FROM_TOUCH))
    return;

  if (part.type == MenuPart::NONE ||
      (part.type == MenuPart::MENU_ITEM && part.menu &&
       part.menu->GetRootMenuItem() != state_.item->GetRootMenuItem())) {
    // Remember the time stamp of the current (press down) event. The owner can
    // then use this to figure out if this menu was finished with the same click
    // which is sent to it thereafter.
    closing_event_time_ = event->time_stamp();
    // Event wasn't pressed over any menu, or the active menu, cancel.
    RepostEventAndCancel(source, event);
    // Do not repost events for Linux Aura because this behavior is more
    // consistent with the behavior of other Linux apps.
    return;
  }

  // On a press we immediately commit the selection, that way a submenu
  // pops up immediately rather than after a delay.
  int selection_types = SELECTION_UPDATE_IMMEDIATELY;
  if (!part.menu) {
    part.menu = part.parent;
    selection_types |= SELECTION_OPEN_SUBMENU;
  } else {
    if (part.menu->GetDelegate()->CanDrag(part.menu)) {
      possible_drag_ = true;
      press_pt_ = event->location();
    }
    if (part.menu->HasSubmenu() && part.should_submenu_show)
      selection_types |= SELECTION_OPEN_SUBMENU;
  }
  SetSelection(part.menu, selection_types);
}

void MenuController::StartDrag(SubmenuView* source,
                               const gfx::Point& location) {
  MenuItemView* item = state_.item;
  DCHECK(item);
  // Points are in the coordinates of the submenu, need to map to that of
  // the selected item. Additionally source may not be the parent of
  // the selected item, so need to map to screen first then to item.
  gfx::Point press_loc(location);
  View::ConvertPointToScreen(source->GetScrollViewContainer(), &press_loc);
  View::ConvertPointFromScreen(item, &press_loc);
  gfx::Point widget_loc(press_loc);
  View::ConvertPointToWidget(item, &widget_loc);

  float raster_scale = ScaleFactorForDragFromWidget(source->GetWidget());
  gfx::Canvas canvas(item->size(), raster_scale, false /* opaque */);
  item->PaintButton(&canvas, MenuItemView::PaintButtonMode::kForDrag);
  gfx::ImageSkia image(gfx::ImageSkiaRep(canvas.GetBitmap(), raster_scale));

  std::unique_ptr<OSExchangeData> data(std::make_unique<OSExchangeData>());
  item->GetDelegate()->WriteDragData(item, data.get());
  data->provider().SetDragImage(image, press_loc.OffsetFromOrigin());

  StopScrolling();
  int drag_ops = item->GetDelegate()->GetDragOperations(item);
  did_initiate_drag_ = true;
  base::WeakPtr<MenuController> this_ref = AsWeakPtr();
  // TODO(varunjain): Properly determine and send DRAG_EVENT_SOURCE below.
  item->GetWidget()->RunShellDrag(nullptr, std::move(data), widget_loc,
                                  drag_ops,
                                  ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
  // MenuController may have been deleted so check before accessing member
  // variables.
  if (this_ref)
    did_initiate_drag_ = false;
}

bool MenuController::OnKeyPressed(ui::KeyboardCode key_code) {
  // Do not process while performing drag-and-drop
  if (for_drop_)
    return false;

  bool handled_key_code = false;

  switch (key_code) {
    case ui::VKEY_HOME:
      if (IsEditableCombobox())
        break;
      MoveSelectionToFirstOrLastItem(INCREMENT_SELECTION_DOWN);
      break;

    case ui::VKEY_END:
      if (IsEditableCombobox())
        break;
      MoveSelectionToFirstOrLastItem(INCREMENT_SELECTION_UP);
      break;

    case ui::VKEY_UP:
      IncrementSelection(INCREMENT_SELECTION_UP);
      break;

    case ui::VKEY_DOWN:
      IncrementSelection(INCREMENT_SELECTION_DOWN);
      break;

    // Handling of VK_RIGHT and VK_LEFT is different depending on the UI
    // layout.
    case ui::VKEY_RIGHT:
      if (IsEditableCombobox())
        break;
      if (base::i18n::IsRTL())
        CloseSubmenu();
      else
        OpenSubmenuChangeSelectionIfCan();
      break;

    case ui::VKEY_LEFT:
      if (IsEditableCombobox())
        break;
      if (base::i18n::IsRTL())
        OpenSubmenuChangeSelectionIfCan();
      else
        CloseSubmenu();
      break;

// On Mac, treat space the same as return.
#if !defined(OS_MACOSX)
    case ui::VKEY_SPACE:
      SendAcceleratorToHotTrackedView();
      break;
#endif

    case ui::VKEY_F4:
      if (!IsCombobox())
        break;
      // Fallthrough to accept or dismiss combobox menus on F4, like windows.
      FALLTHROUGH;
    case ui::VKEY_RETURN:
#if defined(OS_MACOSX)
    case ui::VKEY_SPACE:
#endif
      // An odd special case: if a prefix selection is in flight, space should
      // add to that selection rather than activating the menu. This is
      // important for allowing the user to select between items that have the
      // same first word.
      if (key_code == ui::VKEY_SPACE &&
          MenuConfig::instance().all_menus_use_prefix_selection &&
          ShouldContinuePrefixSelection()) {
        break;
      }
      if (pending_state_.item) {
        if (pending_state_.item->HasSubmenu()) {
          if ((key_code == ui::VKEY_F4 ||
               (key_code == ui::VKEY_RETURN && IsEditableCombobox())) &&
              pending_state_.item->GetSubmenu()->IsShowing())
            Cancel(ExitType::kAll);
          else
            OpenSubmenuChangeSelectionIfCan();
        } else {
          handled_key_code = true;
          if (!SendAcceleratorToHotTrackedView() &&
              pending_state_.item->GetEnabled()) {
            Accept(pending_state_.item, 0);
          }
        }
      }
      break;

    case ui::VKEY_ESCAPE:
      if (!state_.item->GetParentMenuItem() ||
          (!state_.item->GetParentMenuItem()->GetParentMenuItem() &&
           (!state_.item->SubmenuIsShowing()))) {
        // User pressed escape and current menu has no submenus. If we are
        // nested, close the current menu on the stack. Otherwise fully exit the
        // menu.
        Cancel(delegate_stack_.size() > 1 ? ExitType::kOutermost
                                          : ExitType::kAll);
        break;
      }
      CloseSubmenu();
      break;

#if !defined(OS_MACOSX)
    case ui::VKEY_APPS: {
      Button* hot_view = GetFirstHotTrackedView(pending_state_.item);
      if (hot_view) {
        hot_view->ShowContextMenu(hot_view->GetKeyboardContextMenuLocation(),
                                  ui::MENU_SOURCE_KEYBOARD);
      } else if (pending_state_.item->GetEnabled() &&
                 pending_state_.item->GetRootMenuItem() !=
                     pending_state_.item) {
        // Show the context menu for the given menu item. We don't try to show
        // the menu for the (boundless) root menu item. This can happen, e.g.,
        // when the user hits the APPS key after opening the menu, when no item
        // is selected, but showing a context menu for an implicitly-selected
        // and invisible item doesn't make sense.
        ShowContextMenu(pending_state_.item,
                        pending_state_.item->GetKeyboardContextMenuLocation(),
                        ui::MENU_SOURCE_KEYBOARD);
      }
      break;
    }
#endif

#if defined(OS_WIN)
    // On Windows, pressing Alt and F10 keys should hide the menu to match the
    // OS behavior.
    case ui::VKEY_MENU:
    case ui::VKEY_F10:
      Cancel(ExitType::kAll);
      break;
#endif

    default:
      break;
  }
  return handled_key_code;
}

MenuController::MenuController(bool for_drop,
                               internal::MenuControllerDelegate* delegate)
    : for_drop_(for_drop),
      active_mouse_view_tracker_(std::make_unique<ViewTracker>()),
      delegate_(delegate),
      alert_animation_(this) {
  delegate_stack_.push_back(delegate_);
  active_instance_ = this;
}

MenuController::~MenuController() {
  DCHECK(!showing_);
  if (owner_)
    owner_->RemoveObserver(this);
  if (active_instance_ == this)
    active_instance_ = nullptr;
  StopShowTimer();
  StopCancelAllTimer();
}

bool MenuController::SendAcceleratorToHotTrackedView() {
  Button* hot_view = GetFirstHotTrackedView(pending_state_.item);
  if (!hot_view)
    return false;

  base::WeakPtr<MenuController> this_ref = AsWeakPtr();
  ui::Accelerator accelerator(ui::VKEY_RETURN, ui::EF_NONE);
  hot_view->AcceleratorPressed(accelerator);
  // An accelerator may have canceled the menu after activation.
  if (this_ref) {
    Button* button = static_cast<Button*>(hot_view);
    SetHotTrackedButton(button);
  }
  return true;
}

void MenuController::UpdateInitialLocation(const gfx::Rect& bounds,
                                           MenuAnchorPosition position,
                                           bool context_menu) {
  pending_state_.context_menu = context_menu;
  pending_state_.initial_bounds = bounds;

  // Reverse anchor position for RTL languages.
  if (base::i18n::IsRTL() && (position == MenuAnchorPosition::kTopRight ||
                              position == MenuAnchorPosition::kTopLeft)) {
    pending_state_.anchor = position == MenuAnchorPosition::kTopRight
                                ? MenuAnchorPosition::kTopLeft
                                : MenuAnchorPosition::kTopRight;
  } else {
    pending_state_.anchor = position;
  }

  // Calculate the bounds of the monitor we'll show menus on. Do this once to
  // avoid repeated system queries for the info.
  pending_state_.monitor_bounds = display::Screen::GetScreen()
                                      ->GetDisplayNearestPoint(bounds.origin())
                                      .work_area();

  if (!pending_state_.monitor_bounds.Contains(bounds)) {
    // Use the monitor area if the work area doesn't contain the bounds. This
    // handles showing a menu from the launcher.
    gfx::Rect monitor_area = display::Screen::GetScreen()
                                 ->GetDisplayNearestPoint(bounds.origin())
                                 .bounds();
    if (monitor_area.Contains(bounds))
      pending_state_.monitor_bounds = monitor_area;
  }
}

void MenuController::Accept(MenuItemView* item, int event_flags) {
#if defined(OS_MACOSX)
  menu_closure_animation_ = std::make_unique<MenuClosureAnimationMac>(
      item, item->GetParentMenuItem()->GetSubmenu(),
      base::BindOnce(&MenuController::ReallyAccept, base::Unretained(this),
                     base::Unretained(item), event_flags));
  menu_closure_animation_->Start();
#else
  ReallyAccept(item, event_flags);
#endif
}

void MenuController::ReallyAccept(MenuItemView* item, int event_flags) {
  DCHECK(!for_drop_);
  result_ = item;
#if defined(OS_MACOSX)
  // Reset the closure animation since it's now finished - this also unblocks
  // input events for the menu.
  menu_closure_animation_.reset();
#endif
  if (item && !menu_stack_.empty() &&
      !item->GetDelegate()->ShouldCloseAllMenusOnExecute(item->GetCommand())) {
    SetExitType(ExitType::kOutermost);
  } else {
    SetExitType(ExitType::kAll);
  }
  accept_event_flags_ = event_flags;
  ExitMenu();
}

bool MenuController::ShowSiblingMenu(SubmenuView* source,
                                     const gfx::Point& mouse_location) {
  if (!menu_stack_.empty() || !pressed_lock_.get())
    return false;

  View* source_view = source->GetScrollViewContainer();
  if (mouse_location.x() >= 0 && mouse_location.x() < source_view->width() &&
      mouse_location.y() >= 0 && mouse_location.y() < source_view->height()) {
    // The mouse is over the menu, no need to continue.
    return false;
  }

  // TODO(oshima): Replace with views only API.
  if (!owner_ || !display::Screen::GetScreen()->IsWindowUnderCursor(
                     owner_->GetNativeWindow())) {
    return false;
  }

  // The user moved the mouse outside the menu and over the owning window. See
  // if there is a sibling menu we should show.
  gfx::Point screen_point(mouse_location);
  View::ConvertPointToScreen(source_view, &screen_point);
  MenuAnchorPosition anchor;
  bool has_mnemonics;
  MenuButton* button = nullptr;
  MenuItemView* alt_menu = source->GetMenuItem()->GetDelegate()->GetSiblingMenu(
      source->GetMenuItem()->GetRootMenuItem(), screen_point, &anchor,
      &has_mnemonics, &button);
  if (!alt_menu || (state_.item && state_.item->GetRootMenuItem() == alt_menu))
    return false;

  delegate_->SiblingMenuCreated(alt_menu);

  if (!button) {
    // If the delegate returns a menu, they must also return a button.
    NOTREACHED();
    return false;
  }

  // There is a sibling menu, update the button state, hide the current menu
  // and show the new one.
  pressed_lock_ = button->button_controller()->TakeLock(true, nullptr);
  // Need to reset capture when we show the menu again, otherwise we aren't
  // going to get any events.
  did_capture_ = false;
  gfx::Point screen_menu_loc;
  View::ConvertPointToScreen(button, &screen_menu_loc);

  // It is currently not possible to show a submenu recursively in a bubble.
  DCHECK(!MenuItemView::IsBubble(anchor));
  UpdateInitialLocation(gfx::Rect(screen_menu_loc.x(), screen_menu_loc.y(),
                                  button->width(), button->height()),
                        anchor, state_.context_menu);
  alt_menu->PrepareForRun(
      false, has_mnemonics,
      source->GetMenuItem()->GetRootMenuItem()->show_mnemonics_);
  alt_menu->controller_ = AsWeakPtr();
  SetSelection(alt_menu, SELECTION_OPEN_SUBMENU | SELECTION_UPDATE_IMMEDIATELY);
  return true;
}

bool MenuController::ShowContextMenu(MenuItemView* menu_item,
                                     const gfx::Point& screen_location,
                                     ui::MenuSourceType source_type) {
  // Set the selection immediately, making sure the submenu is only open
  // if it already was.
  int selection_types = SELECTION_UPDATE_IMMEDIATELY;
  if (state_.item == pending_state_.item && state_.submenu_open)
    selection_types |= SELECTION_OPEN_SUBMENU;
  SetSelection(pending_state_.item, selection_types);

  if (menu_item->GetDelegate()->ShowContextMenu(
          menu_item, menu_item->GetCommand(), screen_location, source_type)) {
    SendMouseCaptureLostToActiveView();
    return true;
  }
  return false;
}

void MenuController::CloseAllNestedMenus() {
  for (auto& nested_menu : menu_stack_) {
    State& state = nested_menu.first;
    MenuItemView* last_item = state.item;
    for (MenuItemView* item = last_item; item;
         item = item->GetParentMenuItem()) {
      CloseMenu(item);
      last_item = item;
    }
    state.submenu_open = false;
    state.item = last_item;
  }
}

MenuItemView* MenuController::GetMenuItemAt(View* source, int x, int y) {
  // Walk the view hierarchy until we find a menu item (or the root).
  View* child_under_mouse = source->GetEventHandlerForPoint(gfx::Point(x, y));
  while (child_under_mouse &&
         child_under_mouse->GetID() != MenuItemView::kMenuItemViewID) {
    child_under_mouse = child_under_mouse->parent();
  }
  if (child_under_mouse && child_under_mouse->GetEnabled() &&
      child_under_mouse->GetID() == MenuItemView::kMenuItemViewID) {
    return static_cast<MenuItemView*>(child_under_mouse);
  }
  return nullptr;
}

MenuItemView* MenuController::GetEmptyMenuItemAt(View* source, int x, int y) {
  View* child_under_mouse = source->GetEventHandlerForPoint(gfx::Point(x, y));
  if (child_under_mouse &&
      child_under_mouse->GetID() == MenuItemView::kEmptyMenuItemViewID) {
    return static_cast<MenuItemView*>(child_under_mouse);
  }
  return nullptr;
}

bool MenuController::IsScrollButtonAt(SubmenuView* source,
                                      int x,
                                      int y,
                                      MenuPart::Type* part) {
  MenuScrollViewContainer* scroll_view = source->GetScrollViewContainer();
  View* child_under_mouse =
      scroll_view->GetEventHandlerForPoint(gfx::Point(x, y));
  if (child_under_mouse && child_under_mouse->GetEnabled()) {
    if (child_under_mouse == scroll_view->scroll_up_button()) {
      *part = MenuPart::SCROLL_UP;
      return true;
    }
    if (child_under_mouse == scroll_view->scroll_down_button()) {
      *part = MenuPart::SCROLL_DOWN;
      return true;
    }
  }
  return false;
}

MenuController::MenuPart MenuController::GetMenuPart(
    SubmenuView* source,
    const gfx::Point& source_loc) {
  gfx::Point screen_loc(source_loc);
  View::ConvertPointToScreen(source->GetScrollViewContainer(), &screen_loc);
  return GetMenuPartByScreenCoordinateUsingMenu(state_.item, screen_loc);
}

MenuController::MenuPart MenuController::GetMenuPartByScreenCoordinateUsingMenu(
    MenuItemView* item,
    const gfx::Point& screen_loc) {
  MenuPart part;
  for (; item; item = item->GetParentMenuItem()) {
    if (item->SubmenuIsShowing() &&
        GetMenuPartByScreenCoordinateImpl(item->GetSubmenu(), screen_loc,
                                          &part)) {
      return part;
    }
  }
  return part;
}

bool MenuController::GetMenuPartByScreenCoordinateImpl(
    SubmenuView* menu,
    const gfx::Point& screen_loc,
    MenuPart* part) {
  // Is the mouse over the scroll buttons?
  gfx::Point scroll_view_loc = screen_loc;
  View* scroll_view_container = menu->GetScrollViewContainer();
  View::ConvertPointFromScreen(scroll_view_container, &scroll_view_loc);
  if (scroll_view_loc.x() < 0 ||
      scroll_view_loc.x() >= scroll_view_container->width() ||
      scroll_view_loc.y() < 0 ||
      scroll_view_loc.y() >= scroll_view_container->height()) {
    // Point isn't contained in menu.
    return false;
  }
  if (IsScrollButtonAt(menu, scroll_view_loc.x(), scroll_view_loc.y(),
                       &(part->type))) {
    part->submenu = menu;
    return true;
  }

  // Not over the scroll button. Check the actual menu.
  if (DoesSubmenuContainLocation(menu, screen_loc)) {
    gfx::Point menu_loc = screen_loc;
    View::ConvertPointFromScreen(menu, &menu_loc);
    part->menu = GetMenuItemAt(menu, menu_loc.x(), menu_loc.y());
    part->type = MenuPart::MENU_ITEM;
    part->submenu = menu;
    part->should_submenu_show =
        part->submenu && part->menu &&
        (part->menu->GetType() == MenuItemView::SUBMENU ||
         IsLocationOverSubmenuAreaOfActionableSubmenu(part->menu, screen_loc));
    if (!part->menu)
      part->parent = menu->GetMenuItem();
    return true;
  }

  // Return false for points on touchable menu shadows, to search parent menus.
  if (use_touchable_layout_)
    return false;

  // While the mouse isn't over a menu item or the scroll buttons of menu, it
  // is contained by menu and so we return true. If we didn't return true other
  // menus would be searched, even though they are likely obscured by us.
  return true;
}

MenuHostRootView* MenuController::GetRootView(SubmenuView* submenu,
                                              const gfx::Point& source_loc) {
  MenuPart part = GetMenuPart(submenu, source_loc);
  SubmenuView* view = part.submenu;
  return view && view->GetWidget()
             ? static_cast<MenuHostRootView*>(view->GetWidget()->GetRootView())
             : nullptr;
}

void MenuController::ConvertLocatedEventForRootView(View* source,
                                                    View* dst,
                                                    ui::LocatedEvent* event) {
  if (source->GetWidget()->GetRootView() == dst)
    return;
  gfx::Point new_location(event->location());
  View::ConvertPointToScreen(source, &new_location);
  View::ConvertPointFromScreen(dst, &new_location);
  event->set_location(new_location);
}

bool MenuController::DoesSubmenuContainLocation(SubmenuView* submenu,
                                                const gfx::Point& screen_loc) {
  gfx::Point view_loc = screen_loc;
  View::ConvertPointFromScreen(submenu, &view_loc);
  gfx::Rect vis_rect = submenu->GetVisibleBounds();
  return vis_rect.Contains(view_loc);
}

bool MenuController::IsLocationOverSubmenuAreaOfActionableSubmenu(
    MenuItemView* item,
    const gfx::Point& screen_loc) const {
  if (!item || item->GetType() != MenuItemView::ACTIONABLE_SUBMENU)
    return false;

  gfx::Point view_loc = screen_loc;
  View::ConvertPointFromScreen(item, &view_loc);
  if (base::i18n::IsRTL())
    view_loc.set_x(item->GetMirroredXInView(view_loc.x()));
  return item->GetSubmenuAreaOfActionableSubmenu().Contains(view_loc);
}

void MenuController::CommitPendingSelection() {
  StopShowTimer();

  size_t paths_differ_at = 0;
  std::vector<MenuItemView*> current_path;
  std::vector<MenuItemView*> new_path;
  BuildPathsAndCalculateDiff(state_.item, pending_state_.item, &current_path,
                             &new_path, &paths_differ_at);

  // Hide the old menu.
  for (size_t i = paths_differ_at; i < current_path.size(); ++i) {
    CloseMenu(current_path[i]);
  }

  // Copy pending to state_, making sure to preserve the direction menus were
  // opened.
  std::list<bool> pending_open_direction;
  state_.open_leading.swap(pending_open_direction);
  state_ = pending_state_;
  state_.open_leading.swap(pending_open_direction);

  int menu_depth = MenuDepth(state_.item);
  if (menu_depth == 0) {
    state_.open_leading.clear();
  } else {
    int cached_size = static_cast<int>(state_.open_leading.size());
    DCHECK_GE(menu_depth, 0);
    while (cached_size-- >= menu_depth)
      state_.open_leading.pop_back();
  }

  if (!state_.item) {
    // Nothing to select.
    StopScrolling();
    return;
  }

  // Open all the submenus preceeding the last menu item (last menu item is
  // handled next).
  if (new_path.size() > 1) {
    for (auto i = new_path.begin(); i != new_path.end() - 1; ++i)
      OpenMenu(*i);
  }

  if (state_.submenu_open) {
    // The submenu should be open, open the submenu if the item has a submenu.
    if (state_.item->HasSubmenu()) {
      OpenMenu(state_.item);
    } else {
      state_.submenu_open = false;
    }
  } else if (state_.item->SubmenuIsShowing()) {
    state_.item->GetSubmenu()->Hide();
  }

  if (scroll_task_.get() && scroll_task_->submenu()) {
    // Stop the scrolling if none of the elements of the selection contain
    // the menu being scrolled.
    bool found = false;
    for (MenuItemView* item = state_.item; item && !found;
         item = item->GetParentMenuItem()) {
      found = (item->SubmenuIsShowing() &&
               item->GetSubmenu() == scroll_task_->submenu());
    }
    if (!found)
      StopScrolling();
  }
}

void MenuController::CloseMenu(MenuItemView* item) {
  DCHECK(item);
  if (!item->HasSubmenu())
    return;

  for (MenuItemView* subitem : item->GetSubmenu()->GetMenuItems())
    UnregisterAlertedItem(subitem);

  item->GetSubmenu()->Hide();
}

void MenuController::OpenMenu(MenuItemView* item) {
  DCHECK(item);
  if (item->GetSubmenu()->IsShowing()) {
    return;
  }

  OpenMenuImpl(item, true);
  did_capture_ = true;
}

void MenuController::OpenMenuImpl(MenuItemView* item, bool show) {
  // TODO(oshima|sky): Don't show the menu if drag is in progress and
  // this menu doesn't support drag drop. See crbug.com/110495.
  if (show) {
    size_t old_num_children = item->GetSubmenu()->children().size();
    item->GetDelegate()->WillShowMenu(item);
    if (old_num_children != item->GetSubmenu()->children().size()) {
      // If the number of children changed then we may need to add empty items.
      item->RemoveEmptyMenus();
      item->AddEmptyMenus();
    }
  }
  bool prefer_leading =
      state_.open_leading.empty() ? true : state_.open_leading.back();
  bool resulting_direction;
  gfx::Rect bounds =
      MenuItemView::IsBubble(state_.anchor)
          ? CalculateBubbleMenuBounds(item, prefer_leading,
                                      &resulting_direction)
          : CalculateMenuBounds(item, prefer_leading, &resulting_direction);
  state_.open_leading.push_back(resulting_direction);
  bool do_capture = (!did_capture_ && !for_drop_ && !IsEditableCombobox());
  showing_submenu_ = true;

  // Register alerted MenuItemViews so we can animate them. We do this here to
  // handle both newly-opened submenus and submenus that have changed.
  for (MenuItemView* subitem : item->GetSubmenu()->GetMenuItems()) {
    if (subitem->is_alerted())
      RegisterAlertedItem(subitem);
  }

  if (show) {
    item->GetSubmenu()->ShowAt(owner_, bounds, do_capture);

    // Figure out if the mouse is under the menu; if so, remember the mouse
    // location so we can ignore the first mouse move event(s) with that
    // location. We do this after ShowAt because ConvertPointFromScreen
    // doesn't work correctly if the widget isn't shown.
    if (item->GetSubmenu()->GetWidget() != nullptr) {
      gfx::Point mouse_pos =
          display::Screen::GetScreen()->GetCursorScreenPoint();
      View::ConvertPointFromScreen(item->submenu_->GetWidget()->GetRootView(),
                                   &mouse_pos);
      MenuPart part_under_mouse = GetMenuPart(item->submenu_, mouse_pos);
      if (part_under_mouse.type != MenuPart::NONE)
        menu_open_mouse_loc_ = mouse_pos;
    }

    // Menus are the only place using kGroupingPropertyKey, so any value (other
    // than 0) is fine.
    constexpr int kGroupingId = 1001;
    item->GetSubmenu()->GetWidget()->SetNativeWindowProperty(
        TooltipManager::kGroupingPropertyKey,
        reinterpret_cast<void*>(kGroupingId));

    // Set the selection indices for this menu level based on traversal order.
    SetSelectionIndices(item);
  } else {
    item->GetSubmenu()->Reposition(bounds);
  }
  showing_submenu_ = false;
}

void MenuController::MenuChildrenChanged(MenuItemView* item) {
  DCHECK(item);
  // Menu shouldn't be updated during drag operation.
  DCHECK(!active_mouse_view_tracker_->view());

  // If the current item or pending item is a descendant of the item
  // that changed, move the selection back to the changed item.
  const MenuItemView* ancestor = state_.item;
  while (ancestor && ancestor != item)
    ancestor = ancestor->GetParentMenuItem();
  if (!ancestor) {
    ancestor = pending_state_.item;
    while (ancestor && ancestor != item)
      ancestor = ancestor->GetParentMenuItem();
    if (!ancestor)
      return;
  }
  SetSelection(item, SELECTION_OPEN_SUBMENU | SELECTION_UPDATE_IMMEDIATELY);
  if (item->HasSubmenu())
    OpenMenuImpl(item, false);
}

void MenuController::BuildPathsAndCalculateDiff(
    MenuItemView* old_item,
    MenuItemView* new_item,
    std::vector<MenuItemView*>* old_path,
    std::vector<MenuItemView*>* new_path,
    size_t* first_diff_at) {
  DCHECK(old_path && new_path && first_diff_at);
  BuildMenuItemPath(old_item, old_path);
  BuildMenuItemPath(new_item, new_path);

  *first_diff_at = std::distance(
      old_path->cbegin(), std::mismatch(old_path->cbegin(), old_path->cend(),
                                        new_path->cbegin(), new_path->cend())
                              .first);
}

void MenuController::BuildMenuItemPath(MenuItemView* item,
                                       std::vector<MenuItemView*>* path) {
  if (!item)
    return;
  BuildMenuItemPath(item->GetParentMenuItem(), path);
  path->push_back(item);
}

void MenuController::StartShowTimer() {
  show_timer_.Start(
      FROM_HERE, TimeDelta::FromMilliseconds(MenuConfig::instance().show_delay),
      this, &MenuController::CommitPendingSelection);
}

void MenuController::StopShowTimer() {
  show_timer_.Stop();
}

void MenuController::StartCancelAllTimer() {
  cancel_all_timer_.Start(
      FROM_HERE, TimeDelta::FromMilliseconds(kCloseOnExitTime),
      base::BindOnce(&MenuController::Cancel, base::Unretained(this),
                     ExitType::kAll));
}

void MenuController::StopCancelAllTimer() {
  cancel_all_timer_.Stop();
}

gfx::Rect MenuController::CalculateMenuBounds(MenuItemView* item,
                                              bool prefer_leading,
                                              bool* is_leading) {
  DCHECK(item);

  SubmenuView* submenu = item->GetSubmenu();
  DCHECK(submenu);

  gfx::Rect menu_bounds =
      gfx::Rect(submenu->GetScrollViewContainer()->GetPreferredSize());

  const gfx::Rect& monitor_bounds = state_.monitor_bounds;
  const gfx::Rect& anchor_bounds = state_.initial_bounds;

  // For comboboxes, ensure the menu is at least as wide as the anchor.
  if (IsCombobox())
    menu_bounds.set_width(std::max(menu_bounds.width(), anchor_bounds.width()));

  // Don't let the menu go too wide or too tall.
  menu_bounds.set_width(std::min(
      menu_bounds.width(), item->GetDelegate()->GetMaxWidthForMenu(item)));
  if (!monitor_bounds.IsEmpty()) {
    menu_bounds.set_width(
        std::min(menu_bounds.width(), monitor_bounds.width()));
    menu_bounds.set_height(
        std::min(menu_bounds.height(), monitor_bounds.height()));
  }

  // Assume we can honor prefer_leading.
  *is_leading = prefer_leading;

  const MenuConfig& menu_config = MenuConfig::instance();

  if (item->GetParentMenuItem()) {
    // Not the first menu; position it relative to the bounds of its parent menu
    // item.
    gfx::Point item_loc;
    View::ConvertPointToScreen(item, &item_loc);

    // We must make sure we take into account the UI layout. If the layout is
    // RTL, then a 'leading' menu is positioned to the left of the parent menu
    // item and not to the right.
    const bool layout_is_rtl = base::i18n::IsRTL();
    const bool create_on_right = prefer_leading != layout_is_rtl;
    const int submenu_horizontal_inset = menu_config.submenu_horizontal_inset;

    const int left_of_parent =
        item_loc.x() - menu_bounds.width() + submenu_horizontal_inset;
    const int right_of_parent =
        item_loc.x() + item->width() - submenu_horizontal_inset;

    MenuScrollViewContainer* container =
        item->GetParentMenuItem()->GetSubmenu()->GetScrollViewContainer();
    menu_bounds.set_y(item_loc.y() - container->border()->GetInsets().top());

    // Assume the menu can be placed in the preferred location.
    menu_bounds.set_x(create_on_right ? right_of_parent : left_of_parent);

    // Everything after this check requires monitor bounds to be non-empty.
    if (monitor_bounds.IsEmpty())
      return menu_bounds;

    // Menu does not actually fit where it was placed, move it to the other side
    // and update |is_leading|.
    if (menu_bounds.x() < monitor_bounds.x()) {
      *is_leading = !layout_is_rtl;
      menu_bounds.set_x(right_of_parent);
    } else if (menu_bounds.right() > monitor_bounds.right()) {
      *is_leading = layout_is_rtl;
      menu_bounds.set_x(left_of_parent);
    }
  } else {
    using MenuPosition = MenuItemView::MenuPosition;
    // First item, align top left corner of menu with bottom left corner of
    // anchor bounds.
    menu_bounds.set_x(anchor_bounds.x());
    menu_bounds.set_y(anchor_bounds.bottom());

    const int above_anchor = anchor_bounds.y() - menu_bounds.height();

    if (state_.anchor == MenuAnchorPosition::kTopRight) {
      // Move the menu so that its right edge is aligned with the anchor
      // bounds right edge.
      menu_bounds.set_x(anchor_bounds.right() - menu_bounds.width());
    } else if (state_.anchor == MenuAnchorPosition::kBottomCenter) {
      // Try to fit the menu above the anchor bounds. If it doesn't fit, place
      // it below.
      const int horizontally_centered =
          anchor_bounds.x() + (anchor_bounds.width() - menu_bounds.width()) / 2;
      menu_bounds.set_x(horizontally_centered);
      menu_bounds.set_y(above_anchor - kTouchYPadding);
      if (menu_bounds.y() < monitor_bounds.y())
        menu_bounds.set_y(anchor_bounds.y() + kTouchYPadding);
    }

    if (item->actual_menu_position() == MenuPosition::kAboveBounds) {
      // Menu has already been drawn above, put it above the anchor bounds.
      menu_bounds.set_y(above_anchor);
    }

    // Everything beyond this point requires monitor bounds to be non-empty.
    if (monitor_bounds.IsEmpty())
      return menu_bounds;

    // If the menu position is below or above the anchor bounds, force it to fit
    // on the screen. Otherwise, try to fit the menu in the following locations:
    //   1.) Below the anchor bounds
    //   2.) Above the anchor bounds
    //   3.) At the bottom of the monitor and off the side of the anchor bounds
    if (item->actual_menu_position() == MenuPosition::kBelowBounds ||
        item->actual_menu_position() == MenuPosition::kAboveBounds) {
      // Menu has been drawn below/above the anchor bounds, make sure it fits
      // on the screen in its current location.
      const int drawn_width = menu_bounds.width();
      menu_bounds.Intersect(monitor_bounds);

      // Do not allow the menu to get narrower. This handles the case where the
      // menu would have drawn off-screen, but the effective anchor was shifted
      // at the end of this function. Preserve the width, so it is shifted
      // again.
      menu_bounds.set_width(drawn_width);
    } else if (menu_bounds.bottom() <= monitor_bounds.bottom()) {
      // Menu fits below anchor bounds.
      item->set_actual_menu_position(MenuPosition::kBelowBounds);
    } else if (above_anchor >= monitor_bounds.y()) {
      // Menu fits above anchor bounds.
      menu_bounds.set_y(above_anchor);
      item->set_actual_menu_position(MenuPosition::kAboveBounds);
    } else if (item->GetDelegate()->ShouldTryPositioningBesideAnchor()) {
      const int left_of_anchor = anchor_bounds.x() - menu_bounds.width();
      const int right_of_anchor = anchor_bounds.right();

      menu_bounds.set_y(monitor_bounds.bottom() - menu_bounds.height());
      if (state_.anchor == MenuAnchorPosition::kTopLeft) {
        // Prefer menu to right of anchor bounds but move it to left if it
        // doesn't fit.
        menu_bounds.set_x(right_of_anchor);
        if (menu_bounds.right() > monitor_bounds.right())
          menu_bounds.set_x(left_of_anchor);
      } else {
        // Prefer menu to left of anchor bounds but move it to right if it
        // doesn't fit.
        menu_bounds.set_x(left_of_anchor);
        if (menu_bounds.x() < monitor_bounds.x())
          menu_bounds.set_x(right_of_anchor);
      }
    } else {
      // The delegate doesn't want the menu repositioned to the side, and it
      // doesn't fit on the screen in any orientation - just clip the menu to
      // the screen and let the scrolling arrows appear.
      menu_bounds.Intersect(monitor_bounds);
    }
  }

  // Ensure the menu is not displayed off screen.
  menu_bounds.set_x(
      base::ClampToRange(menu_bounds.x(), monitor_bounds.x(),
                         monitor_bounds.right() - menu_bounds.width()));
  menu_bounds.set_y(
      base::ClampToRange(menu_bounds.y(), monitor_bounds.y(),
                         monitor_bounds.bottom() - menu_bounds.height()));

  return menu_bounds;
}

gfx::Rect MenuController::CalculateBubbleMenuBounds(MenuItemView* item,
                                                    bool prefer_leading,
                                                    bool* is_leading) {
  DCHECK(item);

  // Assume we can honor prefer_leading.
  *is_leading = prefer_leading;

  SubmenuView* submenu = item->GetSubmenu();
  DCHECK(submenu);

  gfx::Size menu_size = submenu->GetScrollViewContainer()->GetPreferredSize();
  int x = 0;
  int y = 0;
  const MenuConfig& menu_config = MenuConfig::instance();
  // Shadow insets are built into MenuScrollView's preferred size so it must be
  // compensated for when determining the bounds of touchable menus.
  const gfx::Insets border_and_shadow_insets =
      BubbleBorder::GetBorderAndShadowInsets(
          menu_config.touchable_menu_shadow_elevation);

  const gfx::Rect& monitor_bounds = state_.monitor_bounds;

  if (!item->GetParentMenuItem()) {
    // This is a top-level menu, position it relative to the anchor bounds.
    const gfx::Rect& anchor_bounds = state_.initial_bounds;

    // First the size gets reduced to the possible space.
    if (!monitor_bounds.IsEmpty()) {
      int max_width = monitor_bounds.width();
      int max_height = monitor_bounds.height();
      // In case of bubbles, the maximum width is limited by the space
      // between the display corner and the target area + the tip size.
      if (state_.anchor == MenuAnchorPosition::kBubbleAbove) {
        // Don't consider |border_and_shadow_insets| because when the max size
        // is enforced, the scroll view is shown and the md shadows are not
        // applied.
        max_height =
            std::max(anchor_bounds.y() - monitor_bounds.y(),
                     monitor_bounds.bottom() - anchor_bounds.bottom()) -
            menu_config.touchable_anchor_offset;
      }
      // The menu should always have a non-empty available area.
      DCHECK_GE(max_width, kBubbleTipSizeLeftRight);
      DCHECK_GE(max_height, kBubbleTipSizeTopBottom);
      menu_size.SetToMin(gfx::Size(max_width, max_height));
    }
    // Respect the delegate's maximum width.
    menu_size.set_width(std::min(
        menu_size.width(), item->GetDelegate()->GetMaxWidthForMenu(item)));

    if (state_.anchor == MenuAnchorPosition::kBubbleAbove) {
      // Align the left edges of the menu and anchor, and the bottom of the menu
      // with the top of the anchor.
      x = std::max(monitor_bounds.x(),
                   anchor_bounds.x() - border_and_shadow_insets.left());
      y = anchor_bounds.y() - menu_size.height() +
          border_and_shadow_insets.bottom() -
          menu_config.touchable_anchor_offset;

      // Align the right of the menu with the right of the anchor.
      if (x + menu_size.width() > monitor_bounds.right()) {
        x = anchor_bounds.right() - menu_size.width() +
            border_and_shadow_insets.right();
      }
      // Align the top of the menu with the bottom of the anchor.
      if (y < monitor_bounds.y()) {
        y = anchor_bounds.bottom() - border_and_shadow_insets.top() +
            menu_config.touchable_anchor_offset;
      }
    } else if (state_.anchor == MenuAnchorPosition::kBubbleLeft ||
               state_.anchor == MenuAnchorPosition::kBubbleRight) {
      if (state_.anchor == MenuAnchorPosition::kBubbleLeft) {
        // Align the right of the menu with the left of the anchor, and the top
        // of the menu with the top of the anchor.
        x = anchor_bounds.x() - menu_size.width() +
            border_and_shadow_insets.right() -
            menu_config.touchable_anchor_offset;
        // Align the left of the menu with the right of the anchor.
        if (x < monitor_bounds.x()) {
          x = anchor_bounds.right() - border_and_shadow_insets.left() +
              menu_config.touchable_anchor_offset;
        }
      } else {
        // Align the left of the menu with the right of the anchor, and the top
        // of the menu with the top of the anchor.
        x = anchor_bounds.right() - border_and_shadow_insets.left() +
            menu_config.touchable_anchor_offset;
        if (x + menu_size.width() > monitor_bounds.right()) {
          // Align the right of the menu with the left of the anchor.
          x = anchor_bounds.x() - menu_size.width() +
              border_and_shadow_insets.right() -
              menu_config.touchable_anchor_offset;
        }
      }

      const int y_below = anchor_bounds.y() - border_and_shadow_insets.top();
      const int y_above = anchor_bounds.bottom() - menu_size.height() +
                          border_and_shadow_insets.bottom();
      if (y_below + menu_size.height() <= monitor_bounds.bottom()) {
        // Show below the anchor. Align the top of the menu with the top of the
        // anchor.
        y = y_below;
      } else if (y_above >= monitor_bounds.y()) {
        // No room below, but there is room above. Show above the anchor. Align
        // the bottom of the menu with the bottom of the anchor.
        y = y_above;
      } else {
        // No room above or below. Show as low as possible. Align the bottom of
        // the menu with the bottom of the screen.
        y = monitor_bounds.bottom() - menu_size.height();
      }
    }
    // The above adjustments may have shifted a large menu off the screen.
    // Clamp the menu origin to the valid range.
    const int x_min = monitor_bounds.x() - border_and_shadow_insets.left();
    const int x_max = monitor_bounds.right() - menu_size.width() +
                      border_and_shadow_insets.right();
    const int y_min = monitor_bounds.y() - border_and_shadow_insets.top();
    const int y_max = monitor_bounds.bottom() - menu_size.height() +
                      border_and_shadow_insets.bottom();
    DCHECK_LE(x_min, x_max);
    DCHECK_LE(y_min, y_max);
    x = base::ClampToRange(x, x_min, x_max);
    y = base::ClampToRange(y, y_min, y_max);
  } else {
    if (!use_touchable_layout_) {
      NOTIMPLEMENTED()
          << "Nested bubble menus are only implemented for touchable menus.";
    }

    // This is a sub-menu, position it relative to the parent menu.
    const gfx::Rect item_bounds = item->GetBoundsInScreen();
    // If the layout is RTL, then a 'leading' menu is positioned to the left of
    // the parent menu item and not to the right.
    const bool layout_is_rtl = base::i18n::IsRTL();
    const bool create_on_right = prefer_leading != layout_is_rtl;

    const int width_with_right_inset =
        menu_config.touchable_menu_width + border_and_shadow_insets.right();
    const int x_max = monitor_bounds.right() - width_with_right_inset;
    const int x_left = item_bounds.x() - width_with_right_inset;
    const int x_right = item_bounds.right() - border_and_shadow_insets.left();
    if (create_on_right) {
      x = x_right;
      if (monitor_bounds.width() == 0 || x_right <= x_max) {
        // Enough room on the right, show normally.
        x = x_right;
      } else if (x_left >= monitor_bounds.x()) {
        // Enough room on the left, show there.
        *is_leading = prefer_leading;
        x = x_left;
      } else {
        // No room on either side. Flush the menu to the right edge.
        x = x_max;
      }
    } else {
      if (monitor_bounds.width() == 0 || x_left >= monitor_bounds.x()) {
        // Enough room on the left, show normally.
        x = x_left;
      } else if (x_right <= x_max) {
        // Enough room on the right, show there.
        *is_leading = !prefer_leading;
        x = x_right;
      } else {
        // No room on either side. Flush the menu to the left edge.
        x = monitor_bounds.x();
      }
    }
    y = item_bounds.y() - border_and_shadow_insets.top() -
        menu_config.vertical_touchable_menu_item_padding;
    y = base::ClampToRange(y,
                           monitor_bounds.y() - border_and_shadow_insets.top(),
                           monitor_bounds.bottom() - menu_size.height() +
                               border_and_shadow_insets.top());
  }
  return gfx::Rect(x, y, menu_size.width(), menu_size.height());
}

// static
int MenuController::MenuDepth(MenuItemView* item) {
  return item ? (MenuDepth(item->GetParentMenuItem()) + 1) : 0;
}

void MenuController::IncrementSelection(
    SelectionIncrementDirectionType direction) {
  MenuItemView* item = pending_state_.item;
  DCHECK(item);
  if (pending_state_.submenu_open && item->SubmenuIsShowing()) {
    // A menu is selected and open, but none of its children are selected,
    // select the first menu item that is visible and enabled.
    if (!item->GetSubmenu()->GetMenuItems().empty()) {
      MenuItemView* to_select = FindInitialSelectableMenuItem(item, direction);
      SetInitialHotTrackedView(to_select, direction);
      return;
    }
  }

  if (!item->children().empty()) {
    Button* button = GetFirstHotTrackedView(item);
    if (button) {
      DCHECK_EQ(hot_button_, button);
      SetHotTrackedButton(nullptr);
    }
    bool direction_is_down = direction == INCREMENT_SELECTION_DOWN;
    View* to_make_hot =
        button ? GetNextFocusableView(item, button, direction_is_down)
               : GetInitialFocusableView(item, direction_is_down);
    Button* hot_button = Button::AsButton(to_make_hot);
    if (hot_button) {
      SetHotTrackedButton(hot_button);
      return;
    }
  }

  SetNextHotTrackedView(item, direction);
}

void MenuController::SetSelectionIndices(MenuItemView* parent) {
  std::vector<View*> ordering;
  SubmenuView* const submenu = parent->GetSubmenu();

  for (MenuItemView* item : submenu->GetMenuItems()) {
    if (!item->GetVisible() || !item->GetEnabled())
      continue;

    bool found_focusable = false;
    if (!item->children().empty()) {
      for (View* child = GetInitialFocusableView(item, true); child;
           child = GetNextFocusableView(item, child, true)) {
        ordering.push_back(child);
        found_focusable = true;
      }
    }
    if (!found_focusable)
      ordering.push_back(item);
  }

  if (ordering.empty())
    return;

  const int set_size = ordering.size();
  for (int i = 0; i < set_size; ++i)
    ordering[i]->GetViewAccessibility().OverridePosInSet(i + 1, set_size);
}

void MenuController::MoveSelectionToFirstOrLastItem(
    SelectionIncrementDirectionType direction) {
  MenuItemView* item = pending_state_.item;
  DCHECK(item);
  MenuItemView* submenu = nullptr;

  if (pending_state_.submenu_open && item->SubmenuIsShowing()) {
    if (item->GetSubmenu()->GetMenuItems().empty())
      return;

    // A menu is selected and open, but none of its children are selected,
    // select the first or last menu item that is visible and enabled.
    submenu = item;
  } else {
    submenu = item->GetParentMenuItem();
  }

  MenuItemView* to_select = FindInitialSelectableMenuItem(submenu, direction);
  SetInitialHotTrackedView(to_select, direction);
}

MenuItemView* MenuController::FindInitialSelectableMenuItem(
    MenuItemView* parent,
    SelectionIncrementDirectionType direction) {
  return FindNextSelectableMenuItem(
      parent, direction == INCREMENT_SELECTION_DOWN ? -1 : 0, direction, true);
}

MenuItemView* MenuController::FindNextSelectableMenuItem(
    MenuItemView* parent,
    int index,
    SelectionIncrementDirectionType direction,
    bool is_initial) {
  int parent_count = int{parent->GetSubmenu()->GetMenuItems().size()};
  int stop_index = (index + parent_count) % parent_count;
  bool include_all_items =
      (index == -1 && direction == INCREMENT_SELECTION_DOWN) ||
      (index == 0 && direction == INCREMENT_SELECTION_UP);
  int delta = direction == INCREMENT_SELECTION_UP ? -1 : 1;
  // Loop through the menu items skipping any invisible menus. The loop stops
  // when we wrap or find a visible and enabled child.
  do {
    if (!MenuConfig::instance().arrow_key_selection_wraps && !is_initial) {
      if (index == 0 && direction == INCREMENT_SELECTION_UP)
        return nullptr;
      if (index == parent_count - 1 && direction == INCREMENT_SELECTION_DOWN)
        return nullptr;
    }
    index = (index + delta + parent_count) % parent_count;
    if (index == stop_index && !include_all_items)
      return nullptr;
    MenuItemView* child = parent->GetSubmenu()->GetMenuItemAt(index);
    if (child->GetVisible() && child->GetEnabled())
      return child;
  } while (index != stop_index);
  return nullptr;
}

void MenuController::OpenSubmenuChangeSelectionIfCan() {
  MenuItemView* item = pending_state_.item;
  if (!item->HasSubmenu() || !item->GetEnabled())
    return;
  MenuItemView* to_select = nullptr;
  if (!item->GetSubmenu()->GetMenuItems().empty())
    to_select = FindInitialSelectableMenuItem(item, INCREMENT_SELECTION_DOWN);
  if (to_select) {
    // Selection is going from the ACTIONABLE to the SUBMENU region of the
    // ACTIONABLE_SUBMENU, so highlight the SUBMENU area.
    if (item->type_ == MenuItemView::ACTIONABLE_SUBMENU)
      item->SetSelectionOfActionableSubmenu(true);
    SetSelection(to_select, SELECTION_UPDATE_IMMEDIATELY);
    return;
  }
  // No menu items, just show the sub-menu.
  SetSelection(item, SELECTION_OPEN_SUBMENU | SELECTION_UPDATE_IMMEDIATELY);
}

void MenuController::CloseSubmenu() {
  MenuItemView* item = state_.item;
  DCHECK(item);
  if (!item->GetParentMenuItem())
    return;
  if (item->SubmenuIsShowing())
    SetSelection(item, SELECTION_UPDATE_IMMEDIATELY);
  else if (item->GetParentMenuItem()->GetParentMenuItem())
    SetSelection(item->GetParentMenuItem(), SELECTION_UPDATE_IMMEDIATELY);
}

MenuController::SelectByCharDetails MenuController::FindChildForMnemonic(
    MenuItemView* parent,
    base::char16 key,
    bool (*match_function)(MenuItemView* menu, base::char16 mnemonic)) {
  SubmenuView* submenu = parent->GetSubmenu();
  DCHECK(submenu);
  SelectByCharDetails details;

  const auto menu_items = submenu->GetMenuItems();
  for (size_t i = 0; i < menu_items.size(); ++i) {
    MenuItemView* child = menu_items[i];
    if (child->GetEnabled() && child->GetVisible()) {
      if (child == pending_state_.item)
        details.index_of_item = int{i};
      if (match_function(child, key)) {
        if (details.first_match == -1)
          details.first_match = int{i};
        else
          details.has_multiple = true;
        if (details.next_match == -1 && details.index_of_item != -1 &&
            int{i} > details.index_of_item)
          details.next_match = int{i};
      }
    }
  }
  return details;
}

void MenuController::AcceptOrSelect(MenuItemView* parent,
                                    const SelectByCharDetails& details) {
  // This should only be invoked if there is a match.
  DCHECK_NE(details.first_match, -1);
  DCHECK(parent->HasSubmenu());
  SubmenuView* submenu = parent->GetSubmenu();
  DCHECK(submenu);
  if (!details.has_multiple) {
    // There's only one match, activate it (or open if it has a submenu).
    if (submenu->GetMenuItemAt(details.first_match)->HasSubmenu()) {
      SetSelection(submenu->GetMenuItemAt(details.first_match),
                   SELECTION_OPEN_SUBMENU | SELECTION_UPDATE_IMMEDIATELY);
    } else {
      Accept(submenu->GetMenuItemAt(details.first_match), 0);
    }
  } else if (details.index_of_item == -1 || details.next_match == -1) {
    SetSelection(submenu->GetMenuItemAt(details.first_match),
                 SELECTION_DEFAULT);
  } else {
    SetSelection(submenu->GetMenuItemAt(details.next_match), SELECTION_DEFAULT);
  }
}

void MenuController::SelectByChar(base::char16 character) {
  // Do not process while performing drag-and-drop.
  if (for_drop_)
    return;
  if (!character)
    return;

  base::char16 char_array[] = {character, 0};
  base::char16 key = base::i18n::ToLower(char_array)[0];
  MenuItemView* item = pending_state_.item;
  if (!item->SubmenuIsShowing())
    item = item->GetParentMenuItem();
  DCHECK(item);
  DCHECK(item->HasSubmenu());
  DCHECK(item->GetSubmenu());
  if (item->GetSubmenu()->GetMenuItems().empty())
    return;

  // Look for matches based on mnemonic first.
  SelectByCharDetails details =
      FindChildForMnemonic(item, key, &MatchesMnemonic);
  if (details.first_match != -1) {
    AcceptOrSelect(item, details);
    return;
  }

  if (IsReadonlyCombobox() ||
      MenuConfig::instance().all_menus_use_prefix_selection) {
    item->GetSubmenu()->GetPrefixSelector()->InsertText(char_array);
  } else {
    // If no mnemonics found, look at first character of titles.
    details = FindChildForMnemonic(item, key, &TitleMatchesMnemonic);
    if (details.first_match != -1)
      AcceptOrSelect(item, details);
  }
}

void MenuController::RepostEventAndCancel(SubmenuView* source,
                                          const ui::LocatedEvent* event) {
  // Cancel can lead to the deletion |source| so we save the view and window to
  // be used when reposting the event.
  gfx::Point screen_loc(event->location());
  View::ConvertPointToScreen(source->GetScrollViewContainer(), &screen_loc);

#if defined(OS_WIN) || defined(OS_CHROMEOS)
  gfx::NativeView native_view = source->GetWidget()->GetNativeView();
  gfx::NativeWindow window = nullptr;
  if (native_view) {
    display::Screen* screen = display::Screen::GetScreen();
    window = screen->GetWindowAtScreenPoint(screen_loc);
  }
#endif

#if defined(OS_WIN)
  if (event->IsMouseEvent() || event->IsTouchEvent()) {
    base::WeakPtr<MenuController> this_ref = AsWeakPtr();
    if (state_.item) {
      state_.item->GetRootMenuItem()->GetSubmenu()->ReleaseCapture();
      // We're going to close and we own the event capture. We need to repost
      // the event, otherwise the window the user clicked on won't get the
      // event.
      RepostEventImpl(event, screen_loc, native_view, window);
    } else {
      // We some times get an event after closing all the menus. Ignore it. Make
      // sure the menu is in fact not visible. If the menu is visible, then
      // we're in a bad state where we think the menu isn't visible but it is.
      DCHECK(!source->GetWidget()->IsVisible());
    }

    // Reposting the event may have deleted this, if so exit.
    if (!this_ref)
      return;
  }
#endif

  // Determine target to see if a complete or partial close of the menu should
  // occur.
  ExitType exit_type = ExitType::kAll;
  if (!menu_stack_.empty()) {
    // We're running nested menus. Only exit all if the mouse wasn't over one
    // of the menus from the last run.
    MenuPart last_part = GetMenuPartByScreenCoordinateUsingMenu(
        menu_stack_.back().first.item, screen_loc);
    if (last_part.type != MenuPart::NONE)
      exit_type = ExitType::kOutermost;
  }
#if defined(OS_MACOSX)
  // When doing a menu closure animation, target the deepest submenu - that way
  // MenuClosureAnimationMac will fade out all the menus in sync, rather than
  // the shallowest menu only.
  menu_closure_animation_ = std::make_unique<MenuClosureAnimationMac>(
      nullptr, state_.item->GetSubmenu(),
      base::BindOnce(&MenuController::Cancel, base::Unretained(this),
                     exit_type));
  menu_closure_animation_->Start();
#else
  Cancel(exit_type);
#endif
}

void MenuController::SetDropMenuItem(MenuItemView* new_target,
                                     MenuDelegate::DropPosition new_position) {
  if (new_target == drop_target_ && new_position == drop_position_)
    return;

  if (drop_target_) {
    drop_target_->GetParentMenuItem()->GetSubmenu()->SetDropMenuItem(
        nullptr, MenuDelegate::DropPosition::kNone);
  }
  drop_target_ = new_target;
  drop_position_ = new_position;
  if (drop_target_) {
    drop_target_->GetParentMenuItem()->GetSubmenu()->SetDropMenuItem(
        drop_target_, drop_position_);
  }
}

void MenuController::UpdateScrolling(const MenuPart& part) {
  if (!part.is_scroll() && !scroll_task_.get())
    return;

  if (!scroll_task_.get())
    scroll_task_ = std::make_unique<MenuScrollTask>();
  scroll_task_->Update(part);
}

void MenuController::StopScrolling() {
  scroll_task_.reset(nullptr);
}

void MenuController::UpdateActiveMouseView(SubmenuView* event_source,
                                           const ui::MouseEvent& event,
                                           View* target_menu) {
  View* target = nullptr;
  gfx::Point target_menu_loc(event.location());
  if (target_menu && !target_menu->children().empty()) {
    // Locate the deepest child view to send events to.  This code assumes we
    // don't have to walk up the tree to find a view interested in events. This
    // is currently true for the cases we are embedding views, but if we embed
    // more complex hierarchies it'll need to change.
    View::ConvertPointToScreen(event_source->GetScrollViewContainer(),
                               &target_menu_loc);
    View::ConvertPointFromScreen(target_menu, &target_menu_loc);
    target = target_menu->GetEventHandlerForPoint(target_menu_loc);
    if (target == target_menu || !target->GetEnabled())
      target = nullptr;
  }
  View* active_mouse_view = active_mouse_view_tracker_->view();
  if (target != active_mouse_view) {
    SendMouseCaptureLostToActiveView();
    active_mouse_view = target;
    active_mouse_view_tracker_->SetView(active_mouse_view);
    if (active_mouse_view) {
      gfx::Point target_point(target_menu_loc);
      View::ConvertPointToTarget(target_menu, active_mouse_view, &target_point);
      ui::MouseEvent mouse_entered_event(ui::ET_MOUSE_ENTERED, target_point,
                                         target_point, ui::EventTimeForNow(), 0,
                                         0);
      active_mouse_view->OnMouseEntered(mouse_entered_event);

      ui::MouseEvent mouse_pressed_event(
          ui::ET_MOUSE_PRESSED, target_point, target_point,
          ui::EventTimeForNow(), event.flags(), event.changed_button_flags());
      active_mouse_view->OnMousePressed(mouse_pressed_event);
    }
  }

  if (active_mouse_view) {
    gfx::Point target_point(target_menu_loc);
    View::ConvertPointToTarget(target_menu, active_mouse_view, &target_point);
    ui::MouseEvent mouse_dragged_event(
        ui::ET_MOUSE_DRAGGED, target_point, target_point, ui::EventTimeForNow(),
        event.flags(), event.changed_button_flags());
    active_mouse_view->OnMouseDragged(mouse_dragged_event);
  }
}

void MenuController::SendMouseReleaseToActiveView(SubmenuView* event_source,
                                                  const ui::MouseEvent& event) {
  View* active_mouse_view = active_mouse_view_tracker_->view();
  if (!active_mouse_view)
    return;

  gfx::Point target_loc(event.location());
  View::ConvertPointToScreen(event_source->GetScrollViewContainer(),
                             &target_loc);
  View::ConvertPointFromScreen(active_mouse_view, &target_loc);
  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, target_loc, target_loc,
                               ui::EventTimeForNow(), event.flags(),
                               event.changed_button_flags());
  // Reset the active mouse view before sending mouse released. That way if it
  // calls back to us, we aren't in a weird state.
  active_mouse_view_tracker_->SetView(nullptr);
  active_mouse_view->OnMouseReleased(release_event);
}

void MenuController::SendMouseCaptureLostToActiveView() {
  View* active_mouse_view = active_mouse_view_tracker_->view();
  if (!active_mouse_view)
    return;

  // Reset the active mouse view before sending mouse capture lost. That way if
  // it calls back to us, we aren't in a weird state.
  active_mouse_view_tracker_->SetView(nullptr);
  active_mouse_view->OnMouseCaptureLost();
}

void MenuController::SetExitType(ExitType type) {
  exit_type_ = type;
}

void MenuController::ExitMenu() {
  bool nested = delegate_stack_.size() > 1;
  // ExitTopMostMenu unwinds nested delegates
  internal::MenuControllerDelegate* delegate = delegate_;
  // MenuController may have been deleted when releasing ViewsDelegate ref.
  // However as |delegate| can outlive this, it must still be notified of the
  // menu closing so that it can perform teardown.
  int accept_event_flags = accept_event_flags_;
  base::WeakPtr<MenuController> this_ref = AsWeakPtr();
  MenuItemView* result = ExitTopMostMenu();
  delegate->OnMenuClosed(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
                         result, accept_event_flags);
  // |delegate| may have deleted this.
  if (this_ref && nested && exit_type_ == ExitType::kAll)
    ExitMenu();
}

MenuItemView* MenuController::ExitTopMostMenu() {
  base::WeakPtr<MenuController> this_ref = AsWeakPtr();

  // Release the lock which prevents Chrome from shutting down while the menu is
  // showing.
  ViewsDelegate::GetInstance()->ReleaseRef();

  // Releasing the lock can result in Chrome shutting down, deleting this.
  if (!this_ref)
    return nullptr;

  // Close any open menus.
  SetSelection(nullptr, SELECTION_UPDATE_IMMEDIATELY | SELECTION_EXIT);

#if defined(OS_WIN)
  // On Windows, if we select the menu item by touch and if the window at the
  // location is another window on the same thread, that window gets a
  // WM_MOUSEACTIVATE message and ends up activating itself, which is not
  // correct. We workaround this by setting a property on the window at the
  // current cursor location. We check for this property in our
  // WM_MOUSEACTIVATE handler and don't activate the window if the property is
  // set.
  if (item_selected_by_touch_) {
    item_selected_by_touch_ = false;
    POINT cursor_pos;
    ::GetCursorPos(&cursor_pos);
    HWND window = ::WindowFromPoint(cursor_pos);
    if (::GetWindowThreadProcessId(window, nullptr) == ::GetCurrentThreadId()) {
      ::SetProp(window, ui::kIgnoreTouchMouseActivateForWindow,
                reinterpret_cast<HANDLE>(true));
    }
  }
#endif

  std::unique_ptr<MenuButtonController::PressedLock> nested_pressed_lock;
  bool nested_menu = !menu_stack_.empty();
  if (nested_menu) {
    DCHECK(!menu_stack_.empty());
    // We're running from within a menu, restore the previous state.
    // The menus are already showing, so we don't have to show them.
    state_ = menu_stack_.back().first;
    pending_state_ = menu_stack_.back().first;
    hot_button_ = state_.hot_button;
    nested_pressed_lock = std::move(menu_stack_.back().second);
    menu_stack_.pop_back();
    // Even though the menus are nested, there may not be nested delegates.
    if (delegate_stack_.size() > 1) {
      delegate_stack_.pop_back();
      delegate_ = delegate_stack_.back();
    }
  } else {
#if defined(USE_AURA)
    menu_pre_target_handler_.reset();
#endif

    showing_ = false;
    did_capture_ = false;
  }

  MenuItemView* result = result_;
  // In case we're nested, reset |result_|.
  result_ = nullptr;

  if (exit_type_ == ExitType::kOutermost) {
    SetExitType(ExitType::kNone);
  } else if (nested_menu && result) {
    // We're nested and about to return a value. The caller might enter
    // another blocking loop. We need to make sure all menus are hidden
    // before that happens otherwise the menus will stay on screen.
    CloseAllNestedMenus();
    SetSelection(nullptr, SELECTION_UPDATE_IMMEDIATELY | SELECTION_EXIT);

    // Set exit_all_, which makes sure all nested loops exit immediately.
    if (exit_type_ != ExitType::kDestroyed)
      SetExitType(ExitType::kAll);
  }

  // Reset our pressed lock and hot-tracked state to the previous state's, if
  // they were active. The lock handles the case if the button was destroyed.
  pressed_lock_ = std::move(nested_pressed_lock);
  if (hot_button_)
    hot_button_->SetHotTracked(true);

  return result;
}

void MenuController::HandleMouseLocation(SubmenuView* source,
                                         const gfx::Point& mouse_location) {
  if (showing_submenu_)
    return;

  // Ignore mouse events if we're closing the menu.
  if (exit_type_ != ExitType::kNone)
    return;

  MenuPart part = GetMenuPart(source, mouse_location);

  UpdateScrolling(part);

  if (for_drop_)
    return;

  if (part.type == MenuPart::NONE && ShowSiblingMenu(source, mouse_location))
    return;

  if (part.type == MenuPart::MENU_ITEM && part.menu) {
    SetSelection(part.menu, part.should_submenu_show ? SELECTION_OPEN_SUBMENU
                                                     : SELECTION_DEFAULT);
  } else if (!part.is_scroll() && pending_state_.item &&
             pending_state_.item->GetParentMenuItem() &&
             !pending_state_.item->SubmenuIsShowing()) {
    // On exit if the user hasn't selected an item with a submenu, move the
    // selection back to the parent menu item.
    SetSelection(pending_state_.item->GetParentMenuItem(),
                 SELECTION_OPEN_SUBMENU);
  }
}

void MenuController::SetInitialHotTrackedView(
    MenuItemView* item,
    SelectionIncrementDirectionType direction) {
  if (!item)
    return;
  SetSelection(item, SELECTION_DEFAULT);
  View* hot_view =
      GetInitialFocusableView(item, direction == INCREMENT_SELECTION_DOWN);
  SetHotTrackedButton(Button::AsButton(hot_view));
}

void MenuController::SetNextHotTrackedView(
    MenuItemView* item,
    SelectionIncrementDirectionType direction) {
  MenuItemView* parent = item->GetParentMenuItem();
  if (!parent)
    return;
  const auto menu_items = parent->GetSubmenu()->GetMenuItems();
  if (menu_items.size() <= 1)
    return;
  const auto i = std::find(menu_items.cbegin(), menu_items.cend(), item);
  DCHECK(i != menu_items.cend());
  MenuItemView* to_select = FindNextSelectableMenuItem(
      parent, std::distance(menu_items.cbegin(), i), direction, false);
  SetInitialHotTrackedView(to_select, direction);
}

void MenuController::SetHotTrackedButton(Button* hot_button) {
  if (hot_button == hot_button_) {
    // Hot-tracked state may change outside of the MenuController. Correct it.
    if (hot_button && !hot_button->IsHotTracked()) {
      hot_button->SetHotTracked(true);
      hot_button->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
    }
    return;
  }
  if (hot_button_)
    hot_button_->SetHotTracked(false);
  hot_button_ = hot_button;
  if (hot_button) {
    hot_button->SetHotTracked(true);
    hot_button->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  }
}

bool MenuController::ShouldContinuePrefixSelection() const {
  MenuItemView* item = pending_state_.item;
  if (!item->SubmenuIsShowing())
    item = item->GetParentMenuItem();
  return item->GetSubmenu()->GetPrefixSelector()->ShouldContinueSelection();
}

void MenuController::RegisterAlertedItem(MenuItemView* item) {
  alerted_items_.insert(item);
  // Start animation if necessary. We stop the animation once no alerted
  // items are showing.
  if (!alert_animation_.is_animating()) {
    alert_animation_.SetThrobDuration(kAlertAnimationThrobDuration);
    alert_animation_.StartThrobbing(-1);
  }
}

void MenuController::UnregisterAlertedItem(MenuItemView* item) {
  alerted_items_.erase(item);
  // Stop animation if necessary.
  if (alerted_items_.empty())
    alert_animation_.Stop();
}

bool MenuController::CanProcessInputEvents() const {
#if defined(OS_MACOSX)
  return !menu_closure_animation_;
#else
  return true;
#endif
}

}  // namespace views
