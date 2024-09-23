// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/tooltip_state_manager.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/text_elider.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/wm/public/tooltip_client.h"
#include "ui/wm/public/tooltip_observer.h"

namespace views::corewm {
namespace {

#if BUILDFLAG(IS_WIN)
// Drawing a long word in tooltip is very slow on Windows. crbug.com/513693
constexpr size_t kMaxTooltipLength = 1024;
#else
constexpr size_t kMaxTooltipLength = 2048;
#endif

}  // namespace

TooltipStateManager::TooltipStateManager(std::unique_ptr<Tooltip> tooltip)
    : tooltip_(std::move(tooltip)) {}

TooltipStateManager::~TooltipStateManager() = default;

void TooltipStateManager::AddObserver(wm::TooltipObserver* observer) {
  DCHECK(tooltip_);
  tooltip_->AddObserver(observer);
}

void TooltipStateManager::RemoveObserver(wm::TooltipObserver* observer) {
  DCHECK(tooltip_);
  tooltip_->RemoveObserver(observer);
}

int TooltipStateManager::GetMaxWidth(const gfx::Point& location) const {
  return tooltip_->GetMaxWidth(location);
}

void TooltipStateManager::HideAndReset() {
  // Hide any open tooltips.
  will_hide_tooltip_timer_.Stop();
  tooltip_->Hide();

  // Cancel pending tooltips and reset states.
  will_show_tooltip_timer_.Stop();
  tooltip_id_ = nullptr;
  tooltip_parent_window_ = nullptr;
}

void TooltipStateManager::Show(aura::Window* window,
                               const std::u16string& tooltip_text,
                               const gfx::Point& position,
                               TooltipTrigger trigger,
                               const base::TimeDelta show_delay,
                               const base::TimeDelta hide_delay) {
  HideAndReset();

  position_ = position;
  tooltip_id_ = wm::GetTooltipId(window);
  tooltip_text_ = tooltip_text;
  tooltip_parent_window_ = window;
  tooltip_trigger_ = trigger;

  std::u16string truncated_text =
      gfx::TruncateString(tooltip_text_, kMaxTooltipLength, gfx::WORD_BREAK);
  std::u16string trimmed_text;
  base::TrimWhitespace(truncated_text, base::TRIM_ALL, &trimmed_text);

  // If the string consists entirely of whitespace, then don't both showing it
  // (an empty tooltip is useless).
  if (trimmed_text.empty())
    return;

  // Initialize the one-shot timer to show the tooltip after a delay. Any
  // running timers have already been canceled by calling HideAndReset above.
  // This ensures that the tooltip won't show up too early. The delayed
  // appearance of a tooltip is by default and the `show_delay` is only set to
  // 0 in the unit tests.
  StartWillShowTooltipTimer(trimmed_text, show_delay, hide_delay);
}

void TooltipStateManager::UpdatePositionIfNeeded(const gfx::Point& position,
                                                 TooltipTrigger trigger) {
  // The position should only be updated when the tooltip has been triggered but
  // is not yet visible. Also, we only want to allow the update when it's set
  // off by the same trigger that started the |will_show_tooltip_timer_| in
  // the first place. Otherwise, for example, the position of a keyboard
  // triggered tooltip could be updated by an unrelated mouse exited event. The
  // tooltip would then show up at the wrong location.
  if (!will_show_tooltip_timer_.IsRunning() || trigger != tooltip_trigger_)
    return;

  position_ = position;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void TooltipStateManager::OnTooltipShownOnServer(aura::Window* window,
                                                 const std::u16string& text,
                                                 const gfx::Rect& bounds) {
  tooltip_id_ = wm::GetTooltipId(window);
  tooltip_parent_window_ = window;
  tooltip_->OnTooltipShownOnServer(text, bounds);
}

void TooltipStateManager::OnTooltipHiddenOnServer() {
  tooltip_id_ = nullptr;
  tooltip_parent_window_ = nullptr;
  tooltip_->OnTooltipHiddenOnServer();
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

void TooltipStateManager::ShowNow(const std::u16string& trimmed_text,
                                  const base::TimeDelta hide_delay) {
  if (!tooltip_parent_window_)
    return;

  if (!tooltip_parent_window_->GetRootWindow()) {
    // This can happen if the window is in the process of closing.
    return;
  }

  tooltip_->Update(tooltip_parent_window_, trimmed_text, position_,
                   tooltip_trigger_);
  tooltip_->Show();
  if (!hide_delay.is_zero()) {
    will_hide_tooltip_timer_.Start(FROM_HERE, hide_delay, this,
                                   &TooltipStateManager::HideAndReset);
  }
}

void TooltipStateManager::StartWillShowTooltipTimer(
    const std::u16string& trimmed_text,
    const base::TimeDelta show_delay,
    const base::TimeDelta hide_delay) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Send `show_delay` and `hide_delay` together and delegate the timer
  // handling on Ash side.
  tooltip_->Update(tooltip_parent_window_, trimmed_text, position_,
                   tooltip_trigger_);
  tooltip_->SetDelay(show_delay, hide_delay);
  tooltip_->Show();
#else
  if (!show_delay.is_zero()) {
    // Start will show tooltip timer is show_delay is non-zero.
    will_show_tooltip_timer_.Start(
        FROM_HERE, show_delay,
        base::BindOnce(&TooltipStateManager::ShowNow,
                       weak_factory_.GetWeakPtr(), trimmed_text, hide_delay));
    return;
  } else {
    // This other path is needed for the unit tests to pass because Show is not
    // immediately called when we have a `show_delay` of zero.
    // TODO(bebeaudr): Fix this by ensuring that the unit tests wait for the
    // timer to fire before continuing for non-Lacros platforms.
    ShowNow(trimmed_text, hide_delay);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

}  // namespace views::corewm
