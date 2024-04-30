// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SCROLLBAR_OVERLAY_SCROLL_BAR_H_
#define UI_VIEWS_CONTROLS_SCROLLBAR_OVERLAY_SCROLL_BAR_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"

namespace views {

// The transparent scrollbar which overlays its contents.
class VIEWS_EXPORT OverlayScrollBar : public ScrollBar {
  METADATA_HEADER(OverlayScrollBar, ScrollBar)

 public:
  explicit OverlayScrollBar(Orientation orientation);

  OverlayScrollBar(const OverlayScrollBar&) = delete;
  OverlayScrollBar& operator=(const OverlayScrollBar&) = delete;

  ~OverlayScrollBar() override;

  // ScrollBar:
  gfx::Insets GetInsets() const override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OverlapsContent() const override;
  gfx::Rect GetTrackBounds() const override;
  int GetThickness() const override;

 private:
  class Thumb : public BaseScrollBarThumb {
    METADATA_HEADER(Thumb, BaseScrollBarThumb)

   public:
    explicit Thumb(OverlayScrollBar* scroll_bar);

    Thumb(const Thumb&) = delete;
    Thumb& operator=(const Thumb&) = delete;

    ~Thumb() override;

    void Init();

   protected:
    // BaseScrollBarThumb:
    gfx::Size CalculatePreferredSize(
        const SizeBounds& /*available_size*/) const override;
    void OnPaint(gfx::Canvas* canvas) override;
    void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
    void OnStateChanged() override;

   private:
    raw_ptr<OverlayScrollBar> scroll_bar_;
  };
  friend class Thumb;

  // Shows this (effectively, the thumb) without delay.
  void Show();
  // Hides this with a delay.
  void Hide();
  // Starts a countdown that hides this when it fires.
  void StartHideCountdown();

  base::OneShotTimer hide_timer_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SCROLLBAR_OVERLAY_SCROLL_BAR_H_
