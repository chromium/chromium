// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TOOLTIP_CONTROLLER_H_
#define UI_VIEWS_COREWM_TOOLTIP_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/views_export.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/tooltip_client.h"

namespace aura {
class Window;
}

namespace wm {
class ActivationClient;
}
namespace views {
namespace corewm {

class Tooltip;
class TooltipStateManager;

namespace test {
class TooltipControllerTestHelper;
}  // namespace test

enum class TooltipTrigger {
  kCursor,
  kKeyboard,
};

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
  base::StringPiece GetLogContext() const override;

  // Overridden from aura::client::CursorClientObserver.
  void OnCursorVisibilityChanged(bool is_visible) override;

  // Overridden from aura::WindowObserver.
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  // Overridden from wm::ActivationChangeObserver.
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

 private:
  friend class test::TooltipControllerTestHelper;

  // Reset the window and calls `TooltipStateManager::HideAndReset`.
  void HideAndReset();

  // Updates the tooltip if required (if there is any change in the tooltip
  // text, tooltip id or the aura::Window).
  void UpdateIfRequired(TooltipTrigger trigger);

  // Returns true if there's a drag-and-drop in progress.
  bool IsDragDropInProgress() const;

  // Returns true if the cursor is visible.
  bool IsCursorVisible() const;

  // Get the delay after which the tooltip should be hidden.
  base::TimeDelta GetHideTooltipTimeout();

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

  // The opposite of SetHideTooltipTimeout.
  void RemoveHideTooltipTimeoutFromMap(aura::Window* window);

  // Stop tracking the window on which the cursor was when the mouse was pressed
  // if we're on another window or if a new tooltip is triggered by keyboard.
  void ResetWindowAtMousePressedIfNeeded(aura::Window* target,
                                         bool force_reset);

  // To prevent the tooltip to show again after a mouse press event, we want
  // to hide it until the cursor moves to another window.
  bool ShouldHideBecauseMouseWasOncePressed();

  // The window on which we are currently listening for events. When there's a
  // keyboard-triggered visible tooltip, its value is set to the tooltip parent
  // window. Otherwise, it's following the cursor.
  raw_ptr<aura::Window> observed_window_ = nullptr;

  // This is the position our controller will use to position the tooltip. When
  // the tooltip is triggered by a keyboard action resulting in a view gaining
  // focus, the point is set from the bounds of the view that gained focus.
  // When the tooltip is triggered by the cursor, the |anchor_point_| is set to
  // the |last_mouse_loc_|.
  gfx::Point anchor_point_;

  // These fields are for tracking state when the user presses a mouse button.
  // The tooltip should stay hidden after a mouse press event on the view until
  // the cursor moves to another view.
  std::u16string tooltip_text_at_mouse_press_;
  raw_ptr<aura::Window> tooltip_window_at_mouse_press_ = nullptr;

  // Location of the last events in |tooltip_window_|'s coordinates.
  gfx::Point last_mouse_loc_;
  gfx::Point last_touch_loc_;

  // Whether tooltips can be displayed or not.
  bool tooltips_enabled_ = true;

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

}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_TOOLTIP_CONTROLLER_H_
