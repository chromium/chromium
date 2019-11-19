// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_CUSTOM_FRAME_VIEW_H_
#define UI_VIEWS_WINDOW_CUSTOM_FRAME_VIEW_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/frame_buttons.h"
#include "ui/views/window/non_client_view.h"

namespace gfx {
class FontList;
}

namespace views {

class FrameBackground;
class ImageButton;
class Widget;

///////////////////////////////////////////////////////////////////////////////
//
// CustomFrameView
//
//  A view that provides the non client frame for Windows. This means
//  rendering the non-standard window caption, border, and controls.
//
////////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT CustomFrameView : public NonClientFrameView,
                                     public ButtonListener {
 public:
  CustomFrameView();
  ~CustomFrameView() override;

  void Init(Widget* frame);

  // Overridden from NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  void ResetWindowControls() override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;
  void PaintAsActiveChanged(bool active) override;

  // Overridden from View:
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;

  // Overridden from ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

  // Returns the font list to use in the window's title bar.
  // TODO(https://crbug.com/968860): Move this into the typography provider.
  static gfx::FontList GetWindowTitleFontList();

 private:
  friend class CustomFrameViewTest;

  // Returns the thickness of the border that makes up the window frame edges.
  // This does not include any client edge.
  int FrameBorderThickness() const;

  // Returns the thickness of the entire nonclient left, right, and bottom
  // borders, including both the window frame and any client edge.
  int NonClientBorderThickness() const;

  // Returns the height of the entire nonclient top border, including the window
  // frame, any title area, and any connected client edge.
  int NonClientTopBorderHeight() const;

  // Returns the y-coordinate of the caption buttons.
  int CaptionButtonY() const;

  // Returns the thickness of the nonclient portion of the 3D edge along the
  // bottom of the titlebar.
  int TitlebarBottomThickness() const;

  // Returns the size of the titlebar icon.  This is used even when the icon is
  // not shown, e.g. to set the titlebar height.
  int IconSize() const;

  // Returns the bounds of the titlebar icon (or where the icon would be if
  // there was one).
  gfx::Rect IconBounds() const;

  // Returns true if the title bar, caption buttons, and frame border should be
  // drawn. If false, the client view occupies the full area of this view.
  bool ShouldShowTitleBarAndBorder() const;

  // Returns true if the client edge should be drawn. This is true if
  // the window is not maximized.
  bool ShouldShowClientEdge() const;

  // Paint various sub-components of this view.
  void PaintRestoredFrameBorder(gfx::Canvas* canvas);
  void PaintMaximizedFrameBorder(gfx::Canvas* canvas);
  void PaintTitleBar(gfx::Canvas* canvas);
  void PaintRestoredClientEdge(gfx::Canvas* canvas);

  // Compute aspects of the frame needed to paint the frame background.
  SkColor GetFrameColor() const;
  gfx::ImageSkia GetFrameImage() const;

  // Performs the layout for the window control buttons based on the
  // configuration specified in WindowButtonOrderProvider. The sizing and
  // positions of the buttons affects LayoutTitleBar, call this beforehand.
  void LayoutWindowControls();

  // Calculations depend on the positions of the window controls. Always call
  // LayoutWindowControls beforehand.
  void LayoutTitleBar();
  void LayoutClientView();

  // Creates, adds and returns a new window caption button (e.g, minimize,
  // maximize, restore).
  ImageButton* InitWindowCaptionButton(int accessibility_string_id,
                                       int normal_image_id,
                                       int hot_image_id,
                                       int pushed_image_id);

  // Returns the window caption button for the given FrameButton type, if it
  // should be visible. Otherwise NULL.
  ImageButton* GetImageButton(views::FrameButton button);

  // The bounds of the client view, in this view's coordinates.
  gfx::Rect client_view_bounds_;

  // The layout rect of the title, if visible.
  gfx::Rect title_bounds_;

  // Not owned.
  Widget* frame_ = nullptr;

  // The icon of this window. May be NULL.
  ImageButton* window_icon_ = nullptr;

  // Window caption buttons.
  ImageButton* minimize_button_ = nullptr;
  ImageButton* maximize_button_ = nullptr;
  ImageButton* restore_button_ = nullptr;
  ImageButton* close_button_ = nullptr;

  // Background painter for the window frame.
  std::unique_ptr<FrameBackground> frame_background_;

  // The horizontal boundaries for the title bar to layout within. Restricted
  // by the space used by the leading and trailing buttons.
  int minimum_title_bar_x_ = 0;
  int maximum_title_bar_x_ = -1;

  DISALLOW_COPY_AND_ASSIGN(CustomFrameView);
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_CUSTOM_FRAME_VIEW_H_
