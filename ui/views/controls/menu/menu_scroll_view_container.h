// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_SCROLL_VIEW_CONTAINER_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_SCROLL_VIEW_CONTAINER_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/view.h"

namespace gfx {
class RoundedCornersF;
}  // namespace gfx

namespace views {

class MenuItemView;
class SubmenuView;

// MenuScrollViewContainer contains the SubmenuView (through a MenuScrollView)
// and two scroll buttons. The scroll buttons are only visible and enabled if
// the preferred height of the SubmenuView is bigger than our bounds.
class MenuScrollViewContainer : public View {
 public:
  METADATA_HEADER(MenuScrollViewContainer);

  explicit MenuScrollViewContainer(SubmenuView* content_view);

  MenuScrollViewContainer(const MenuScrollViewContainer&) = delete;
  MenuScrollViewContainer& operator=(const MenuScrollViewContainer&) = delete;

  // Returns the buttons for scrolling up/down.
  View* scroll_down_button() const { return scroll_down_button_; }
  View* scroll_up_button() const { return scroll_up_button_; }

  // External function to check if the bubble border is used.
  bool HasBubbleBorder() const;

  // View overrides.
  gfx::Size CalculatePreferredSize() const override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnThemeChanged() override;

  void SetBorderColorId(absl::optional<ui::ColorId> border_color_id) {
    border_color_id_ = border_color_id;
  }

 protected:
  // View override.
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  friend class MenuScrollView;

  void DidScrollToTop();
  void DidScrollToBottom();
  void DidScrollAwayFromTop();
  void DidScrollAwayFromBottom();

  // Create a default border or bubble border, as appropriate.
  void CreateBorder();

  // Create the default border.
  void CreateDefaultBorder();

  // Create the bubble border.
  void CreateBubbleBorder();

  BubbleBorder::Arrow BubbleBorderTypeFromAnchor(MenuAnchorPosition anchor);

  // Returns the last item in the menu if it is of type HIGHLIGHTED.
  MenuItemView* GetFootnote() const;

  // Calcultes the rounded corners of the view based on: either the
  // `rounded_corners()` if it's set in `MenuController`, or the
  // `CornerRadiusForMenu` in the `MenuConfig` if `rounded_corners()` is not
  // set.
  gfx::RoundedCornersF GetRoundedCorners() const;

  class MenuScrollView;

  // The background view.
  raw_ptr<View> background_view_ = nullptr;

  // The scroll buttons.
  raw_ptr<View> scroll_up_button_;
  raw_ptr<View> scroll_down_button_;

  // The scroll view.
  raw_ptr<MenuScrollView> scroll_view_;

  // The content view.
  raw_ptr<SubmenuView> content_view_;

  // If set the currently set border is a bubble border.
  BubbleBorder::Arrow arrow_ = BubbleBorder::NONE;

  // Corner radius of the background.
  int corner_radius_ = 0;

  // Whether the menu uses ash system UI layout.
  const bool use_ash_system_ui_layout_;

  absl::optional<ui::ColorId> border_color_id_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_SCROLL_VIEW_CONTAINER_H_
