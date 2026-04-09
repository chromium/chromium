// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_FRAME_VIEW_LAYOUT_LINUX_H_
#define UI_VIEWS_WINDOW_FRAME_VIEW_LAYOUT_LINUX_H_

#include "base/memory/raw_ptr.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/views_export.h"
#include "ui/views/window/frame_buttons.h"

namespace views {

class Button;
class FrameViewLinux;

// Layout manager for FrameViewLinux that handles positioning of window control
// buttons, the titlebar, and the client view area. Subclasses may override
// virtual methods to change frame geometry, button sizing, and layout behavior.
class VIEWS_EXPORT FrameViewLayoutLinux : public LayoutManager {
 public:
  // Metrics for positioning a single caption button.
  struct VIEWS_EXPORT ButtonLayoutParams {
    gfx::Size size;
    gfx::Insets margin;
    int inter_spacing = 0;
    int y = 0;
  };

  FrameViewLayoutLinux();

  FrameViewLayoutLinux(const FrameViewLayoutLinux&) = delete;
  FrameViewLayoutLinux& operator=(const FrameViewLayoutLinux&) = delete;

  ~FrameViewLayoutLinux() override;

  // Sets the back-pointer to the owning view.
  void set_view(FrameViewLinux* view) { view_ = view; }

  // Sets whether the window is snapped/tiled by the compositor.
  void set_tiled(bool tiled) { tiled_ = tiled; }

  bool tiled() const { return tiled_; }

  // Sets whether the compositor supports client-drawn frame shadows.
  void set_supports_client_frame_shadow(bool supports) {
    supports_client_frame_shadow_ = supports;
  }

  bool supports_client_frame_shadow() const {
    return supports_client_frame_shadow_;
  }

  // LayoutManager:
  void Layout(View* host) override;
  gfx::Size GetPreferredSize(const View* host) const override;
  gfx::Size GetPreferredSize(const View* host,
                             const SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize(const View* host) const override;

  // Returns the insets of the frame border.
  gfx::Insets GetFrameBorderInsets() const;

  // Returns the frame border insets for the restored state, regardless of
  // current widget state.
  virtual gfx::Insets GetRestoredFrameBorderInsets() const;

  // Returns the bounds of the client view area.
  gfx::Rect GetBoundsForClientView() const;

  // Returns the window bounds needed to enclose the given `client_bounds`.
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const;

  // Returns the insets from the window edge to the input-sensitive area.
  virtual gfx::Insets GetInputInsets() const;

  // Returns the height of the translucent area at the top of the window frame.
  virtual int GetTranslucentTopAreaHeight() const;

  // Returns the height of the entire non-client top area including the frame
  // border, titlebar padding, and titlebar content.
  virtual int GetTopAreaHeight() const;

  // Returns the Y offset where the client view's top edge starts. When this
  // is less than GetTopAreaHeight(), the client view overlaps the titlebar
  // area, allowing content to render behind the caption buttons.
  virtual int GetClientTopOffset() const;

  // Returns true if the titlebar and frame border should be shown.
  virtual bool ShouldShowTitlebarAndBorder() const;

  // Returns the font list used for the window title.
  virtual gfx::FontList GetTitleFontList() const;

  // Returns the shadow values for the frame border.
  virtual gfx::ShadowValues GetShadowValues(bool active) const;

  // Returns the corner radii for the frame.
  virtual gfx::RoundedCornersF GetCornerRadii() const;

  // Width of the resize-handle band.
  static constexpr int kResizeBorder = 10;

 protected:
  // Returns the owning view.
  FrameViewLinux* view() const { return view_; }

  // Returns the Y coordinate at which caption buttons start, which is the
  // top of the frame border.
  int CaptionButtonY() const;

  // Returns the border insets of the titlebar. Content such as buttons and
  // title text is placed inside these borders. Defaults to empty.
  virtual gfx::Insets GetTopAreaBorderInsets() const;

  // Returns the size and positioning metrics for a caption button.
  virtual ButtonLayoutParams GetButtonLayoutParams(FrameButton button_id,
                                                   Button* button) const;

  // Returns the padding around the top area content.
  virtual gfx::Insets GetTopAreaSpacing() const;

  // Lays out the window control buttons.
  virtual void LayoutWindowControls();

  // Lays out the title label.
  virtual void LayoutTitlebar();

  // Lays out the client view area.
  virtual void LayoutClientView();

  // Hides the button(s) corresponding to `button_id`.
  void HideButton(FrameButton button_id);

  // Configures a single caption button: checks capabilities, gets layout
  // params via `GetButtonLayoutParams()`, and places the button. Returns
  // true if the button was placed, false if it was hidden.
  bool ConfigureButton(FrameButton button_id,
                       bool is_leading,
                       bool is_first,
                       int& next_button_x);

  // The horizontal boundaries for the titlebar layout, restricted by
  // the space used by the leading and trailing buttons.
  int minimum_titlebar_x_ = 0;
  int maximum_titlebar_x_ = 0;

 private:
  gfx::Size GetDefaultButtonSize() const;

  raw_ptr<FrameViewLinux> view_ = nullptr;

  gfx::Rect client_view_bounds_;
  bool tiled_ = false;
  bool supports_client_frame_shadow_ = false;
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_FRAME_VIEW_LAYOUT_LINUX_H_
