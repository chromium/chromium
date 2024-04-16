// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TOOLTIP_STATE_MANAGER_H_
#define UI_VIEWS_COREWM_TOOLTIP_STATE_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/corewm/tooltip.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/views_export.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace wm {
class TooltipObserver;
}

namespace views::corewm {

namespace test {
class TooltipControllerTestHelper;
}  // namespace test

// TooltipStateManager separates the state handling from the events handling of
// the TooltipController. It is in charge of updating the tooltip state and
// keeping track of it.
class VIEWS_EXPORT TooltipStateManager {
 public:
  explicit TooltipStateManager(std::unique_ptr<Tooltip> tooltip);
  TooltipStateManager(const TooltipStateManager&) = delete;
  TooltipStateManager& operator=(const TooltipStateManager&) = delete;
  ~TooltipStateManager();

  void AddObserver(wm::TooltipObserver* observer);
  void RemoveObserver(wm::TooltipObserver* observer);

  int GetMaxWidth(const gfx::Point& location) const;

  // Hides the tooltip, clears timers, and resets controller states.
  void HideAndReset();

  bool IsVisible() const { return tooltip_->IsVisible(); }

  // Updates the tooltip state attributes and starts timer to show the tooltip.
  // If `hide_delay` is greater than 0, sets a timer to hide it after a specific
  // delay. Otherwise, shows the tooltip indefinitely.
  void Show(aura::Window* window,
            const std::u16string& tooltip_text,
            const gfx::Point& position,
            TooltipTrigger trigger,
            const base::TimeDelta show_delay,
            const base::TimeDelta hide_delay);

  // Returns the `tooltip_id_`, which corresponds to the pointer of the view on
  // which the tooltip was last added.
  const void* tooltip_id() const { return tooltip_id_; }
  // Returns the `tooltip_text_`, which corresponds to the last value the
  // tooltip got updated to.
  const std::u16string& tooltip_text() const { return tooltip_text_; }
  const aura::Window* tooltip_parent_window() const {
    return tooltip_parent_window_;
  }
  TooltipTrigger tooltip_trigger() const { return tooltip_trigger_; }

  // Updates the 'position_' if we're about to show the tooltip. This is to
  // ensure that the tooltip's position is aligned with either the latest cursor
  // location for a cursor triggered tooltip or the most recent position
  // received for a keyboard triggered tooltip.
  void UpdatePositionIfNeeded(const gfx::Point& position,
                              TooltipTrigger trigger);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Called when tooltip is shown/hidden on server.
  // Only used by Lacros.
  void OnTooltipShownOnServer(aura::Window* window,
                              const std::u16string& text,
                              const gfx::Rect& bounds);
  void OnTooltipHiddenOnServer();
#endif

 private:
  friend class test::TooltipControllerTestHelper;

  // Called once the `will_show_tooltip_timer_` fires to show the tooltip.
  void ShowNow(const std::u16string& trimmed_text,
               const base::TimeDelta hide_delay);

  // Start the show timer to show the tooltip.
  void StartWillShowTooltipTimer(const std::u16string& trimmed_text,
                                 const base::TimeDelta show_delay,
                                 const base::TimeDelta hide_delay);

  // For tests only.
  bool IsWillShowTooltipTimerRunningForTesting() const {
    return will_show_tooltip_timer_.IsRunning();
  }
  bool IsWillHideTooltipTimerRunningForTesting() const {
    return will_hide_tooltip_timer_.IsRunning();
  }

  // The current position of the tooltip. This position is relative to the
  // `tooltip_parent_window_` and in that window's coordinate space.
  gfx::Point position_;

  std::unique_ptr<Tooltip> tooltip_;

  // The pointer to the view for which the tooltip is set.
  // TODO(crbug.com/40285438) - Fix this dangling pointer.
  raw_ptr<const void, DanglingUntriaged> tooltip_id_ = nullptr;

  // The text value used at the last tooltip update.
  std::u16string tooltip_text_;

  // The window on which the tooltip is added.
  raw_ptr<aura::Window> tooltip_parent_window_ = nullptr;

  TooltipTrigger tooltip_trigger_ = TooltipTrigger::kCursor;

  // Two timers for the tooltip: one to hide an on-screen tooltip after a delay,
  // and one to display the tooltip when the timer fires.
  // Timers are always not running on Lacros using server side tooltip since
  // they are handled on Ash side.
  base::OneShotTimer will_hide_tooltip_timer_;
  base::OneShotTimer will_show_tooltip_timer_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<TooltipStateManager> weak_factory_{this};
};

}  // namespace views::corewm

#endif  // UI_VIEWS_COREWM_TOOLTIP_STATE_MANAGER_H_
