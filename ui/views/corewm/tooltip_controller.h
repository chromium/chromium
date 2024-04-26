// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TOOLTIP_CONTROLLER_H_
#define UI_VIEWS_COREWM_TOOLTIP_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"
#include "ui/events/event_handler.h"
#include "ui/views/corewm/tooltip.h"
#include "ui/views/views_export.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/tooltip_client.h"

namespace aura {
class Window;
}

namespace base {
class TimeDelta;
}

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace wm {
class ActivationClient;
class TooltipObserver;
}

namespace views::corewm {

class Tooltip;
class TooltipStateManager;

namespace test {
class TooltipControllerTestHelper;
}  // namespace test

// TooltipController listens for events that can have an impact on the
// tooltip state.
class VIEWS_EXPORT TooltipController
    : public wm::TooltipClient,
      public ui::EventHandler,
      public aura::client::CursorClientObserver,
      public aura::WindowObserver,
      public wm::ActivationChangeObserver {
 public:
  TooltipController(std::unique_ptr<Tooltip> tooltip,
                    wm::ActivationClient* activation_client);

  TooltipController(const TooltipController&) = delete;
  TooltipController& operator=(const TooltipController&) = delete;

  ~TooltipController() override;

  void AddObserver(wm::TooltipObserver* observer);
  void RemoveObserver(wm::TooltipObserver* observer);

  // Overridden from wm::TooltipClient.
  int GetMaxWidth(const gfx::Point& location) const override;
  void UpdateTooltip(aura::Window* target) override;
  void UpdateTooltipFromKeyboard(const gfx::Rect& bounds,
                                 aura::Window* target) override;
  bool IsTooltipSetFromKeyboard(aura::Window* target) override;
  void SetHideTooltipTimeout(aura::Window* target,
                             base::TimeDelta timeout) override;
  void SetTooltipsEnabled(bool enable) override;

  // Overridden from ui::EventHandler.
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnCancelMode(ui::CancelModeEvent* event) override;
  std::string_view GetLogContext() const override;

  // Overridden from aura::client::CursorClientObserver.
  void OnCursorVisibilityChanged(bool is_visible) override;

  // Overridden from aura::WindowObserver.
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  // Overridden from wm::ActivationChangeObserver.
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // Updates tooltip triggered by keyboard with `anchor_point` value.
  // This should be called instead of UpdateTooltipFromKeyboard() when the
  // anchor point is already calculated (e.g. Exo).
  void UpdateTooltipFromKeyboardWithAnchorPoint(const gfx::Point& anchor_point,
                                                aura::Window* target);

  // Sets show tooltip delay for `target` window.
  void SetShowTooltipDelay(aura::Window* target, base::TimeDelta delay);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Called when tooltip is shown/hidden on server.
  // This is only used for Lacros whose tooltip is handled on server-side.
  void OnTooltipShownOnServer(aura::Window* window,
                              const std::u16string& text,
                              const gfx::Rect& bounds);
  void OnTooltipHiddenOnServer();
#endif  // BUILDFLA(IS_CHROMEOS_LACROS)

 private:
  friend class test::TooltipControllerTestHelper;

  // Resets the window and calls `TooltipStateManager::HideAndReset`.
  void HideAndReset();

  // Updates the tooltip if required (if there is any change in the tooltip
  // text, tooltip id or the aura::Window).
  void UpdateIfRequired(TooltipTrigger trigger);

  // Returns true if there's a drag-and-drop in progress.
  bool IsDragDropInProgress() const;

  // Returns true if the cursor is visible.
  bool IsCursorVisible() const;

  // Gets the delay after which the tooltip should be shown/hidden.
  base::TimeDelta GetShowTooltipDelay();
  base::TimeDelta GetHideTooltipDelay();

  // Sets observed window to |target| if it is different from existing window.
  // Calls RemoveObserver on the existing window if it is not NULL.
  // Calls AddObserver on the new window if it is not NULL.
  void SetObservedWindow(aura::Window* target);

  // Returns true if the tooltip id stored on the state manager and the one
  // stored on the window are different.
  bool IsTooltipIdUpdateNeeded() const;

  // Returns true if the tooltip text stored on the state manager and the one
  // stored on the window are different.
  bool IsTooltipTextUpdateNeeded() const;

  // Removes show/hide tooltip delay from `show_tooltip_delay_map_` and
  // `hide_tooltip_timeout_map_`.
  void RemoveTooltipDelayFromMap(aura::Window* window);

  // Stops tracking the window on which the cursor was when the mouse was
  // pressed if we're on another window or if a new tooltip is triggered by
  // keyboard.
  void ResetWindowAtMousePressedIfNeeded(aura::Window* target,
                                         bool force_reset);

  // To prevent the tooltip to show again after a mouse press event, we want
  // to hide it until the cursor moves to another window.
  bool ShouldHideBecauseMouseWasOncePressed();

  // Returns true if the current event is a duplicate event generated by a
  // pen/stylus hovering over the same window.
  bool IsDuplicatePenHoverEvent(ui::EventPointerType pointer_type);

  aura::Window* tooltip_window_at_mouse_press() {
    auto& windows = tooltip_window_at_mouse_press_tracker_.windows();
    return windows.empty() ? nullptr : windows[0];
  }

  // The window on which we are currently listening for events. When there's a
  // keyboard-triggered visible tooltip, its value is set to the tooltip parent
  // window. Otherwise, it's following the cursor.
  raw_ptr<aura::Window> observed_window_ = nullptr;

  // These fields are for tracking state when the user presses a mouse button.
  // The tooltip should stay hidden after a mouse press event on the view until
  // the cursor moves to another view.
  std::u16string tooltip_text_at_mouse_press_;
  // NOTE: this either has zero or one window.
  aura::WindowTracker tooltip_window_at_mouse_press_tracker_;

  // Location of the last events in |tooltip_window_|'s coordinates.
  // |last_mouse_loc_| and |last_focus_loc_| are used to position tooltips
  // triggered by either the mouse or the keyboard, respectively. We also
  // track |last_mouse_loc_| to be able to ignore spurious and/or redundant
  // events.
  gfx::Point last_mouse_loc_;
  gfx::Point last_touch_loc_;
  gfx::Point last_focus_loc_;

  // True if the current event is a duplicate event generated by hovering with a
  // pen/stylus. Hovering with a pen generates a constant stream of move events,
  // so duplicate pen events on the same window should be ignored to prevent the
  // tooltip's show timer from being restarted on each event.
  bool is_duplicate_pen_hover_event_ = false;
  // The last tooltip text that was shown when the pen was hovering.
  // TODO(crbug.com/40246278): Replace this with a unique tooltip identifier
  // when one is implemented. For now, the tooltip text is the closest thing to
  // a tooltip identifier there is.
  std::u16string last_pen_tooltip_text_;

  // Whether tooltips can be displayed or not.
  bool tooltips_enabled_ = true;

  // Whether tooltip should be skip delay before showing.
  // This may be set to true only for testing.
  // Do NOT override this value except from TooltipControllerTestHelper.
  bool skip_show_delay_for_testing_ = false;

  // The show delay before showing tooltip may differ for external app's tooltip
  // such as Lacros. This map specifies the show delay for each target window.
  std::map<aura::Window*, base::TimeDelta> show_tooltip_delay_map_;

  // Web content tooltips should be shown indefinitely and those added on Views
  // should be hidden automatically after a timeout. This map stores the timeout
  // value for each aura::Window.
  // TODO(bebeaudr): Currently, all Views tooltips are hidden after the same
  // timeout and all web content views should be shown indefinitely. If this
  // general rule is always true, then we don't need a complex map here. A set
  // of aura::Window* would be enough with an attribute named
  // "disabled_hide_timeout_views_set_" or something like that.
  std::map<aura::Window*, base::TimeDelta> hide_tooltip_timeout_map_;

  // We want to hide tooltips whenever our client window loses focus. This will
  // ensure that no tooltip stays visible when the user navigated away from
  // our client.
  raw_ptr<wm::ActivationClient> activation_client_;

  // The TooltipStateManager is responsible for keeping track of the current
  // tooltip state (its text, position, id, etc.) and to modify it when asked
  // by the TooltipController or the show/hide timers.
  std::unique_ptr<TooltipStateManager> state_manager_;
};

}  // namespace views::corewm

#endif  // UI_VIEWS_COREWM_TOOLTIP_CONTROLLER_H_
