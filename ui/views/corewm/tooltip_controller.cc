// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/tooltip_controller.h"

#include <stddef.h>

#include <string_view>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/views/corewm/tooltip_state_manager.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/wm/public/activation_client.h"
#include "ui/wm/public/tooltip_observer.h"

namespace views::corewm {
namespace {

constexpr auto kDefaultShowTooltipDelay = base::Milliseconds(500);
constexpr auto kDefaultHideTooltipDelay = base::Seconds(10);

// Returns true if |target| is a valid window to get the tooltip from.
// |event_target| is the original target from the event and |target| the window
// at the same location.
bool IsValidTarget(aura::Window* event_target, aura::Window* target) {
  if (!target || (event_target == target))
    return true;

  // If `target` is contained in `event_target`, it's valid.
  // This case may happen on exo surfaces.
  if (event_target->Contains(target) &&
      event_target->GetBoundsInScreen().Contains(target->GetBoundsInScreen())) {
    return true;
  }

  void* event_target_grouping_id = event_target->GetNativeWindowProperty(
      TooltipManager::kGroupingPropertyKey);

  auto* toplevel_of_target = target->GetToplevelWindow();

  // Return true if grouping id is same for `target` and `event_target`.
  // Also, check grouping id of target's toplevel window to allow the child
  // window under `target`, because the menu window may have a child window.
  return event_target_grouping_id &&
         (event_target_grouping_id ==
              target->GetNativeWindowProperty(
                  TooltipManager::kGroupingPropertyKey) ||
          (toplevel_of_target &&
           event_target_grouping_id ==
               toplevel_of_target->GetNativeWindowProperty(
                   TooltipManager::kGroupingPropertyKey)));
}

// Returns the target (the Window tooltip text comes from) based on the event.
// If a Window other than event.target() is returned, |location| is adjusted
// to be in the coordinates of the returned Window.
aura::Window* GetTooltipTarget(const ui::MouseEvent& event,
                               gfx::Point* location) {
  switch (event.type()) {
    case ui::EventType::kMouseCaptureChanged:
      // On windows we can get a capture changed without an exit. We need to
      // reset state when this happens else the tooltip may incorrectly show.
      return nullptr;
    case ui::EventType::kMouseExited:
      return nullptr;
    case ui::EventType::kMouseMoved:
    case ui::EventType::kMouseDragged: {
      aura::Window* event_target = static_cast<aura::Window*>(event.target());
      if (!event_target)
        return nullptr;

      // If a window other than |event_target| has capture, ignore the event.
      // This can happen when RootWindow creates events when showing/hiding, or
      // the system generates an extra event. We have to check
      // GetGlobalCaptureWindow() as Windows does not use a singleton
      // CaptureClient.
      if (!event_target->HasCapture()) {
        aura::Window* root = event_target->GetRootWindow();
        if (root) {
          aura::client::CaptureClient* capture_client =
              aura::client::GetCaptureClient(root);
          if (capture_client) {
            aura::Window* capture_window =
                capture_client->GetGlobalCaptureWindow();
            if (capture_window && event_target != capture_window)
              return nullptr;
          }
        }
        return event_target;
      }

      // If |target| has capture all events go to it, even if the mouse is
      // really over another window. Find the real window the mouse is over.
      const gfx::Point screen_loc = event.target()->GetScreenLocation(event);
      display::Screen* screen = display::Screen::GetScreen();
      aura::Window* target = screen->GetWindowAtScreenPoint(screen_loc);
      if (!target)
        return nullptr;
      gfx::Point target_loc(screen_loc);
      aura::client::GetScreenPositionClient(target->GetRootWindow())
          ->ConvertPointFromScreen(target, &target_loc);
      aura::Window* screen_target = target->GetEventHandlerForPoint(target_loc);
      if (!IsValidTarget(event_target, screen_target))
        return nullptr;

      aura::Window::ConvertPointToTarget(screen_target, target, &target_loc);
      *location = target_loc;
      return screen_target;
    }
    default:
      NOTREACHED();
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TooltipController public:

TooltipController::TooltipController(std::unique_ptr<Tooltip> tooltip,
                                     wm::ActivationClient* activation_client)
    : activation_client_(activation_client),
      state_manager_(
          std::make_unique<TooltipStateManager>(std::move(tooltip))) {
  if (activation_client_)
    activation_client_->AddObserver(this);
}

TooltipController::~TooltipController() {
  if (observed_window_)
    observed_window_->RemoveObserver(this);
  if (activation_client_)
    activation_client_->RemoveObserver(this);
}

void TooltipController::AddObserver(wm::TooltipObserver* observer) {
  state_manager_->AddObserver(observer);
}

void TooltipController::RemoveObserver(wm::TooltipObserver* observer) {
  state_manager_->RemoveObserver(observer);
}

int TooltipController::GetMaxWidth(const gfx::Point& location) const {
  return state_manager_->GetMaxWidth(location);
}

void TooltipController::UpdateTooltip(aura::Window* target) {
  // The |tooltip_parent_window_| is only set when the tooltip is visible or
  // its |will_show_tooltip_timer_| is running.
  if (target && observed_window_ == target) {
    if (state_manager_->tooltip_parent_window() ||
        IsTooltipTextUpdateNeeded()) {
      UpdateIfRequired(TooltipTrigger::kCursor);
    }
  }

  ResetWindowAtMousePressedIfNeeded(target, /* force_reset */ false);
}

void TooltipController::UpdateTooltipFromKeyboard(const gfx::Rect& bounds,
                                                  aura::Window* target) {
  UpdateTooltipFromKeyboardWithAnchorPoint(bounds.bottom_center(), target);
}

void TooltipController::UpdateTooltipFromKeyboardWithAnchorPoint(
    const gfx::Point& anchor_point,
    aura::Window* target) {
  last_focus_loc_ = anchor_point;
  SetObservedWindow(target);

  // Update the position of the active but not yet visible keyboard triggered
  // tooltip, if any.
  if (state_manager_->tooltip_parent_window()) {
    state_manager_->UpdatePositionIfNeeded(last_focus_loc_,
                                           TooltipTrigger::kKeyboard);
  }

  UpdateIfRequired(TooltipTrigger::kKeyboard);

  ResetWindowAtMousePressedIfNeeded(target, /* force_reset */ true);
}

bool TooltipController::IsTooltipSetFromKeyboard(aura::Window* target) {
  return target && target == state_manager_->tooltip_parent_window() &&
         state_manager_->tooltip_trigger() == TooltipTrigger::kKeyboard;
}

void TooltipController::SetHideTooltipTimeout(aura::Window* target,
                                              base::TimeDelta timeout) {
  hide_tooltip_timeout_map_[target] = timeout;
}

void TooltipController::SetTooltipsEnabled(bool enable) {
  if (tooltips_enabled_ == enable)
    return;
  tooltips_enabled_ = enable;
  UpdateTooltip(observed_window_);
}

void TooltipController::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() != ui::EventType::kKeyPressed) {
    return;
  }
  // Always hide a tooltip on a key press. Since this controller is a pre-target
  // handler (i.e. the events are received here before the target act on them),
  // hiding the tooltip will not cancel any action supposed to show it triggered
  // by a key press.
  HideAndReset();
}

// TODO(crbug.com/40285439): Figure out why we have code both here and
// in DesktopNativeWidgetAura to handle mouse (and key?) events. Seems like we
// should only need one set of them.
void TooltipController::OnMouseEvent(ui::MouseEvent* event) {
  // Ignore mouse events that coincide with the last touch event.
  if (event->location() == last_touch_loc_) {
    // If the tooltip is visible, SetObservedWindow will also hide it if needed.
    SetObservedWindow(nullptr);
    return;
  }
  switch (event->type()) {
    case ui::EventType::kMouseExited:
      // TODO(bebeaudr): Keyboard-triggered tooltips that show up right where
      // the cursor currently is are hidden as soon as they show up because of
      // this event. Handle this case differently to fix the issue.
      //
      // Whenever a tooltip is closed, an EventType::kMouseExited event is
      // fired, even if the cursor is not in the tooltip's window. Make sure
      // that these mouse exited events don't interfere with keyboard triggered
      // tooltips by returning early.
      if (state_manager_->tooltip_parent_window() &&
          state_manager_->tooltip_trigger() == TooltipTrigger::kKeyboard) {
        return;
      }
      SetObservedWindow(nullptr);
      break;
    case ui::EventType::kMouseCaptureChanged:
    case ui::EventType::kMouseMoved:
    case ui::EventType::kMouseDragged: {
      // Synthesized mouse moves shouldn't cause us to show a tooltip. See
      // https://crbug.com/1146981.
      if (event->IsSynthesized())
        break;

#if BUILDFLAG(IS_WIN)
      // Showing a tooltip causes Windows to generate a MOUSE_MOVED
      // event to the same location it was already at; when that happens,
      // we need to throw the event away rather than acting as if someone
      // moved the mouse and showing a new tooltip.
      if (event->location() == last_mouse_loc_) {
        break;
      }
#endif

      last_mouse_loc_ = event->location();
      aura::Window* target = nullptr;
      // Avoid a call to display::Screen::GetWindowAtScreenPoint() since it can
      // be very expensive on X11 in cases when the tooltip is hidden anyway.
      if (tooltips_enabled_ && !aura::Env::GetInstance()->IsMouseButtonDown() &&
          !IsDragDropInProgress()) {
        target = GetTooltipTarget(*event, &last_mouse_loc_);
      }
      // This needs to be called after the |last_mouse_loc_| is converted to the
      // target's screen coordinates.
      state_manager_->UpdatePositionIfNeeded(last_mouse_loc_,
                                             TooltipTrigger::kCursor);
      SetObservedWindow(target);

      is_duplicate_pen_hover_event_ =
          IsDuplicatePenHoverEvent(event->pointer_details().pointer_type);

      if (state_manager_->IsVisible() ||
          (observed_window_ && IsTooltipTextUpdateNeeded())) {
        UpdateIfRequired(TooltipTrigger::kCursor);
      }
      break;
    }
    case ui::EventType::kMousePressed:
      if ((event->flags() & ui::EF_IS_NON_CLIENT) == 0) {
        aura::Window* target = static_cast<aura::Window*>(event->target());
        // We don't get a release for non-client areas.
        tooltip_window_at_mouse_press_tracker_.RemoveAll();
        if (target) {
          tooltip_window_at_mouse_press_tracker_.Add(target);
          tooltip_text_at_mouse_press_ = wm::GetTooltipText(target);
        }
      }
      state_manager_->HideAndReset();
      break;
    case ui::EventType::kMousewheel:
      // Hide the tooltip for click, release, drag, wheel events.
      if (state_manager_->IsVisible())
        state_manager_->HideAndReset();
      break;
    default:
      break;
  }
}

void TooltipController::OnTouchEvent(ui::TouchEvent* event) {
  // Hide the tooltip for touch events.
  HideAndReset();
  last_touch_loc_ = event->location();
}

void TooltipController::OnCancelMode(ui::CancelModeEvent* event) {
  HideAndReset();
}

std::string_view TooltipController::GetLogContext() const {
  return "TooltipController";
}

void TooltipController::OnCursorVisibilityChanged(bool is_visible) {
  if (is_visible && !state_manager_->tooltip_parent_window()) {
    // When there's no tooltip and the cursor becomes visible, the cursor might
    // already be over an item that should trigger a tooltip. Update it to
    // ensure we don't miss this case.
    UpdateIfRequired(TooltipTrigger::kCursor);
  } else if (!is_visible && state_manager_->tooltip_parent_window() &&
             state_manager_->tooltip_trigger() == TooltipTrigger::kCursor) {
    // When the cursor is hidden and we have an active tooltip that was
    // triggered by the cursor, hide it.
    HideAndReset();
  }
}

void TooltipController::OnWindowVisibilityChanged(aura::Window* window,
                                                  bool visible) {
  // If window is not drawn, skip modifying tooltip.
  if (!visible && window->layer()->type() != ui::LAYER_NOT_DRAWN)
    HideAndReset();
}

void TooltipController::OnWindowDestroying(aura::Window* window) {
  // Reset tooltip before `observed_window_` is destructed since Tooltip::Hide
  // which is called by HideAndReset() may try to access to the raw_ptr of the
  // window.
  if (state_manager_->tooltip_parent_window() == window) {
    HideAndReset();
  }
}

void TooltipController::OnWindowDestroyed(aura::Window* window) {
  if (observed_window_ == window) {
    RemoveTooltipDelayFromMap(observed_window_);
    observed_window_ = nullptr;
  }
}

void TooltipController::OnWindowPropertyChanged(aura::Window* window,
                                                const void* key,
                                                intptr_t old) {
  if ((key == wm::kTooltipIdKey || key == wm::kTooltipTextKey) &&
      wm::GetTooltipText(window) != std::u16string() &&
      (IsTooltipTextUpdateNeeded() || IsTooltipIdUpdateNeeded())) {
    UpdateIfRequired(state_manager_->tooltip_trigger());
  }
}

void TooltipController::OnWindowActivated(ActivationReason reason,
                                          aura::Window* gained_active,
                                          aura::Window* lost_active) {
  // We want to hide tooltips whenever the client is losing user focus.
  if (lost_active)
    HideAndReset();
}

void TooltipController::SetShowTooltipDelay(aura::Window* target,
                                            base::TimeDelta delay) {
  show_tooltip_delay_map_[target] = delay;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void TooltipController::OnTooltipShownOnServer(aura::Window* window,
                                               const std::u16string& text,
                                               const gfx::Rect& bounds) {
  state_manager_->OnTooltipShownOnServer(window, text, bounds);
}

void TooltipController::OnTooltipHiddenOnServer() {
  state_manager_->OnTooltipHiddenOnServer();
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

////////////////////////////////////////////////////////////////////////////////
// TooltipController private:

void TooltipController::HideAndReset() {
  state_manager_->HideAndReset();
  SetObservedWindow(nullptr);
}

void TooltipController::UpdateIfRequired(TooltipTrigger trigger) {
  if (!tooltips_enabled_ || aura::Env::GetInstance()->IsMouseButtonDown() ||
      IsDragDropInProgress() ||
      (trigger == TooltipTrigger::kCursor && !IsCursorVisible())) {
    state_manager_->HideAndReset();
    return;
  }

  // When a user press a mouse button, we want to hide the tooltip and prevent
  // the tooltip from showing up again until the cursor moves to another view
  // than the one that received the press event.
  if (ShouldHideBecauseMouseWasOncePressed()) {
    state_manager_->HideAndReset();
    return;
  }
  tooltip_window_at_mouse_press_tracker_.RemoveAll();

  // If this is a duplicate event generated by a hovering stylus or pen, the
  // tooltip has already been updated and its timer should not be restarted.
  if (is_duplicate_pen_hover_event_) {
    return;
  }

  // If the uniqueness indicator is different from the previously encountered
  // one, we should force tooltip update
  if (!state_manager_->IsVisible() || IsTooltipTextUpdateNeeded() ||
      IsTooltipIdUpdateNeeded()) {
    gfx::Point tooltip_point =
        ((trigger == TooltipTrigger::kCursor) ? last_mouse_loc_
                                              : last_focus_loc_);
    state_manager_->Show(observed_window_, wm::GetTooltipText(observed_window_),
                         tooltip_point, trigger, GetShowTooltipDelay(),
                         GetHideTooltipDelay());
  }
}

bool TooltipController::IsDragDropInProgress() const {
  if (!observed_window_)
    return false;
  aura::client::DragDropClient* client =
      aura::client::GetDragDropClient(observed_window_->GetRootWindow());
  return client && client->IsDragDropInProgress();
}

bool TooltipController::IsCursorVisible() const {
  if (!observed_window_)
    return false;
  aura::Window* root = observed_window_->GetRootWindow();
  if (!root)
    return false;
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root);
  // |cursor_client| may be NULL in tests, treat NULL as always visible.
  return !cursor_client || cursor_client->IsCursorVisible();
}

base::TimeDelta TooltipController::GetShowTooltipDelay() {
  std::map<aura::Window*, base::TimeDelta>::const_iterator it =
      show_tooltip_delay_map_.find(observed_window_);
  if (it == show_tooltip_delay_map_.end()) {
    return skip_show_delay_for_testing_ ? base::TimeDelta()
                                        : kDefaultShowTooltipDelay;
  }
  return it->second;
}

base::TimeDelta TooltipController::GetHideTooltipDelay() {
  std::map<aura::Window*, base::TimeDelta>::const_iterator it =
      hide_tooltip_timeout_map_.find(observed_window_);
  if (it == hide_tooltip_timeout_map_.end())
    return kDefaultHideTooltipDelay;
  return it->second;
}

void TooltipController::SetObservedWindow(aura::Window* target) {
  if (observed_window_ == target)
    return;

  // When we are setting the |observed_window_| to nullptr, it is generally
  // because the cursor is over a window not owned by Chromium. To prevent a
  // tooltip from being shown after the cursor goes to another window not
  // managed by us, hide the the tooltip and cancel all timers that would show
  // the tooltip.
  if (!target && state_manager_->tooltip_parent_window()) {
    // Important: We can't call `TooltipController::HideAndReset` or we'd get an
    // infinite loop here.
    state_manager_->HideAndReset();
  }

  if (observed_window_)
    observed_window_->RemoveObserver(this);
  observed_window_ = target;
  if (observed_window_)
    observed_window_->AddObserver(this);
}

bool TooltipController::IsTooltipIdUpdateNeeded() const {
  return state_manager_->tooltip_id() != wm::GetTooltipId(observed_window_);
}

bool TooltipController::IsTooltipTextUpdateNeeded() const {
  return state_manager_->tooltip_text() != wm::GetTooltipText(observed_window_);
}

void TooltipController::RemoveTooltipDelayFromMap(aura::Window* window) {
  show_tooltip_delay_map_.erase(window);
  hide_tooltip_timeout_map_.erase(window);
}

void TooltipController::ResetWindowAtMousePressedIfNeeded(aura::Window* target,
                                                          bool force_reset) {
  // Reset tooltip_window_at_mouse_press() if the cursor moved within the same
  // window but over a region that has different tooltip text. This handles the
  // case of clicking on a view, moving within the same window but over a
  // different view, then back to the original view.
  if (force_reset ||
      (tooltip_window_at_mouse_press() &&
       target == tooltip_window_at_mouse_press() &&
       wm::GetTooltipText(target) != tooltip_text_at_mouse_press_)) {
    tooltip_window_at_mouse_press_tracker_.RemoveAll();
  }
}

// TODO(bebeaudr): This approach is less than ideal. It looks at the tooltip
// text at the moment the mouse was pressed to determine whether or not we are
// on the same tooltip as before. This cause problems when two elements are next
// to each other and have the same text - unlikely, but an issue nonetheless.
// However, this is currently the nearest we can get since we don't have an
// identifier of the renderer side element that triggered the tooltip. Could we
// pass a renderer element unique id alongside the tooltip text?
bool TooltipController::ShouldHideBecauseMouseWasOncePressed() {
  // Skip hiding when tooltip text is empty as no need to hide again.
  // This is required since client-side tooltip appears as empty text on server
  // side so that the tooltip is overridden by empty text regardless of the
  // actual text to show.
  // TODO(crbug.com/40246278): Remove or update this special path when tooltip
  // identifier is implemented.
  if (wm::GetTooltipText(observed_window_).empty())
    return false;

  return tooltip_window_at_mouse_press() &&
         observed_window_ == tooltip_window_at_mouse_press() &&
         wm::GetTooltipText(observed_window_) == tooltip_text_at_mouse_press_;
}

bool TooltipController::IsDuplicatePenHoverEvent(
    ui::EventPointerType pointer_type) {
  if (pointer_type != ui::EventPointerType::kPen || !observed_window_) {
    return false;
  }
  const auto tooltip_text = wm::GetTooltipText(observed_window_);
  if (!tooltip_text.empty() && tooltip_text == last_pen_tooltip_text_) {
    return true;
  }
  last_pen_tooltip_text_ = tooltip_text;
  return false;
}

}  // namespace views::corewm
