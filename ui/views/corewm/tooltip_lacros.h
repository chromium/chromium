// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TOOLTIP_LACROS_H_
#define UI_VIEWS_COREWM_TOOLTIP_LACROS_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/corewm/tooltip.h"
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
class TooltipLacrosForTesting;
}

// Implementation of Tooltip on Lacros using server side tooltip.
// TooltipLacros requests Ash to show/hide tooltip and Ash creates tooltip
// widget on server side and notifies Lacros of the tooltip visibility, position
// and the actual text to be shown.
//
// This is only used when Ash supports server side tooltip. If not, fallbacks to
// TooltipAura.
class VIEWS_EXPORT TooltipLacros : public Tooltip {
 public:
  static const char kWidgetName[];

  TooltipLacros();
  TooltipLacros(const TooltipLacros&) = delete;
  TooltipLacros& operator=(const TooltipLacros&) = delete;
  ~TooltipLacros() override;

  // Tooltip:
  void AddObserver(wm::TooltipObserver* observer) override;
  void RemoveObserver(wm::TooltipObserver* observer) override;
  void OnTooltipShownOnServer(const std::u16string& text,
                              const gfx::Rect& boudns) override;
  void OnTooltipHiddenOnServer() override;

 private:
  friend class test::TooltipLacrosForTesting;

  // Tooltip:
  int GetMaxWidth(const gfx::Point& location) const override;
  void Update(aura::Window* parent_window,
              const std::u16string& text,
              const gfx::Point& position,
              const TooltipTrigger trigger) override;
  void SetDelay(const base::TimeDelta& show_delay,
                const base::TimeDelta& hide_delay) override;
  void Show() override;
  void Hide() override;
  bool IsVisible() override;

  // True if tooltip is visible.
  bool is_visible_ = false;

  raw_ptr<aura::Window, DanglingUntriaged> parent_window_ = nullptr;
  std::u16string text_;
  gfx::Point position_;
  TooltipTrigger trigger_;
  base::TimeDelta show_delay_;
  base::TimeDelta hide_delay_;

  // Observes tooltip state change.
  base::ObserverList<wm::TooltipObserver> observers_;
};

}  // namespace views::corewm

#endif  // UI_VIEWS_COREWM_TOOLTIP_LACROS_H_
