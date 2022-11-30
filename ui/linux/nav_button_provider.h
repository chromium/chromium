// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LINUX_NAV_BUTTON_PROVIDER_H_
#define UI_LINUX_NAV_BUTTON_PROVIDER_H_

namespace chrome {
enum class FrameButtonDisplayType;
}

namespace gfx {
class ImageSkia;
class Insets;
}  // namespace gfx

namespace ui {

class NavButtonProvider {
 public:
  // This enum is similar to views::FrameButton, except it partitions
  // kMaximize and kRestore.  This is useful for when we want to be
  // explicit about which buttons we want drawn, without having to
  // implicitly determine if we should use kMaximize or kRestore
  // depending on the browser window's maximized/restored state.
  enum class FrameButtonDisplayType {
    kMinimize,
    kMaximize,
    kRestore,
    kClose,
  };

  // This enum is based on views::Button::ButtonState.
  enum class ButtonState {
    kNormal,
    kHovered,
    kPressed,
    kDisabled,
  };

  virtual ~NavButtonProvider() = default;

  // Redraws all images and updates all size state.  |top_area_height|
  // is the total available height to render the buttons, and buttons
  // may be drawn larger when more height is available.  |active|
  // indicates if the window the buttons reside in has activation.
  virtual void RedrawImages(int top_area_height,
                            bool maximized,
                            bool active) = 0;

  // Gets the cached button image corresponding to |type| and |state|.
  virtual gfx::ImageSkia GetImage(FrameButtonDisplayType type,
                                  ButtonState state) const = 0;

  // Gets the external margin around each button.  The left inset
  // represents the leading margin, and the right inset represents the
  // trailing margin.
  virtual gfx::Insets GetNavButtonMargin(FrameButtonDisplayType type) const = 0;

  // Gets the internal spacing (padding + border) of the top area.
  // The left inset represents the leading spacing, and the right
  // inset represents the trailing spacing.
  virtual gfx::Insets GetTopAreaSpacing() const = 0;

  // Gets the spacing to be used to separate buttons.
  virtual int GetInterNavButtonSpacing() const = 0;
};

}  // namespace ui

#endif  // UI_LINUX_NAV_BUTTON_PROVIDER_H_
