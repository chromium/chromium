// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_FRAME_VIEW_LINUX_H_
#define UI_VIEWS_WINDOW_FRAME_VIEW_LINUX_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/shadow_value.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/nav_button_provider.h"
#include "ui/linux/window_button_order_observer.h"
#include "ui/views/views_export.h"
#include "ui/views/window/frame_buttons.h"
#include "ui/views/window/frame_view.h"
#include "ui/views/window/frame_view_layout_linux.h"

class SkRRect;

namespace gfx {
class Canvas;
}  // namespace gfx

namespace views {

class Button;
class FrameBackground;
class Widget;

// A FrameView that provides client-side-decorated frames for non-browser
// widgets on Linux. Uses FrameCaptionButton with vector icons by default;
// subclasses may override to use native toolkit rendering.
class VIEWS_EXPORT FrameViewLinux : public FrameView,
                                    private ui::WindowButtonOrderObserver {
  METADATA_HEADER(FrameViewLinux, FrameView)

 public:
  // If `layout` is null, a default FrameViewLayoutLinux is used.
  explicit FrameViewLinux(Widget* widget,
                          FrameViewLayoutLinux* layout = nullptr);

  FrameViewLinux(const FrameViewLinux&) = delete;
  FrameViewLinux& operator=(const FrameViewLinux&) = delete;

  ~FrameViewLinux() override;

  // Must be called after construction so that virtual dispatch works correctly.
  void InitViews();

  // FrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  void ResetWindowControls() override;
  bool HasWindowTitle() const override;
  bool IsWindowTitleVisible() const override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;

  // View:
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout(PassKey) override;
  void OnThemeChanged() override;
  gfx::Size GetMaximumSize() const override;

  // Returns the insets of the frame border.
  gfx::Insets GetFrameBorderInsets() const;

  // Returns the frame border insets for the restored state, regardless of
  // current widget state.
  gfx::Insets GetRestoredFrameBorderInsets() const;

  // Returns the clip region for a restored window, rounded at the top corners.
  SkRRect GetRestoredClipRegion() const;

  // Called by the window host when the tiled state changes.
  void SetTiled(bool tiled);

  // Sets whether the compositor supports client-drawn frame shadows.
  void SetSupportsClientFrameShadow(bool supports);

  // Returns true when the compositor supports client-drawn frame shadows.
  bool ShouldDrawRestoredFrameShadow() const;

  // Delegates to the layout manager for frame geometry.
  gfx::Insets GetInputInsets() const;
  int GetTranslucentTopAreaHeight() const;
  gfx::RoundedCornersF GetCornerRadii() const;
  gfx::ShadowValues GetShadowValues(bool active) const;

  // Returns the button for `frame_button`, accounting for maximize/restore
  // state.
  Button* GetFrameButton(FrameButton frame_button);

  // Returns the button corresponding to `type`.
  Button* GetButtonFromType(ui::NavButtonProvider::FrameButtonDisplayType type);

  ui::NavButtonProvider::FrameButtonDisplayType GetButtonDisplayType(
      FrameButton button_id) const;

  Button* minimize_button() const { return minimize_button_; }
  Button* maximize_button() const { return maximize_button_; }
  Button* restore_button() const { return restore_button_; }
  Button* close_button() const { return close_button_; }

  const gfx::Rect& title_bounds() const { return title_bounds_; }
  void set_title_bounds(const gfx::Rect& bounds) { title_bounds_ = bounds; }

 protected:
  virtual void PaintRestoredFrameBorder(gfx::Canvas* canvas);
  virtual void PaintMaximizedFrameBorder(gfx::Canvas* canvas);
  virtual void PaintTitlebar(gfx::Canvas* canvas);
  virtual void OnThemeOrButtonOrderChanged();
  virtual void UpdateButtonColors();

  bool IsTiled() const;

  FrameViewLayoutLinux* layout() const { return layout_; }

  Widget* frame_widget() { return widget_; }

  FrameBackground* frame_background() { return frame_background_.get(); }

  virtual void CreateCaptionButtons();

  // Window controls.
  raw_ptr<Button> minimize_button_ = nullptr;
  raw_ptr<Button> maximize_button_ = nullptr;
  raw_ptr<Button> restore_button_ = nullptr;
  raw_ptr<Button> close_button_ = nullptr;

 private:
  // ui::WindowButtonOrderObserver:
  void OnWindowButtonOrderingChange() override;

  void OnPaintAsActiveChanged();

  const raw_ptr<Widget> widget_;
  raw_ptr<FrameViewLayoutLinux> layout_ = nullptr;
  std::unique_ptr<FrameBackground> frame_background_;
  gfx::Rect title_bounds_;

  base::CallbackListSubscription paint_as_active_subscription_;

  base::ScopedObservation<ui::LinuxUi, ui::WindowButtonOrderObserver>
      button_order_observation_{this};
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_FRAME_VIEW_LINUX_H_
