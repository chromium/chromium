// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_FRAME_VIEW_H_
#define UI_VIEWS_BUBBLE_BUBBLE_FRAME_VIEW_H_

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/window/non_client_view.h"

namespace views {

class BubbleBorder;
class FootnoteContainerView;
class ImageView;

// The non-client frame view of bubble-styled widgets.
class VIEWS_EXPORT BubbleFrameView : public NonClientFrameView,
                                     public ButtonListener {
 public:
  // Internal class name.
  static const char kViewClassName[];

  BubbleFrameView(const gfx::Insets& title_margins,
                  const gfx::Insets& content_margins);
  ~BubbleFrameView() override;

  static std::unique_ptr<Label> CreateDefaultTitleLabel(
      const base::string16& title_text);

  // Creates a close button used in the corner of the dialog.
  static Button* CreateCloseButton(ButtonListener* listener);

  // NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  bool GetClientMask(const gfx::Size& size, gfx::Path* path) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override;
  void ResetWindowControls() override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;

  // Sets a custom view to be the dialog title instead of the |default_title_|
  // label. If there is an existing title view it will be deleted.
  void SetTitleView(std::unique_ptr<View> title_view);

  // View:
  const char* GetClassName() const override;
  gfx::Insets GetInsets() const override;
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void Layout() override;
  void OnPaint(gfx::Canvas* canvas) override;
  void PaintChildren(const PaintInfo& paint_info) override;
  void OnThemeChanged() override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;

  // ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

  // Use bubble_border() and SetBubbleBorder(), not border() and SetBorder().
  BubbleBorder* bubble_border() const { return bubble_border_; }
  void SetBubbleBorder(std::unique_ptr<BubbleBorder> border);

  const View* title() const {
    return custom_title_ ? custom_title_ : default_title_;
  }
  View* title() {
    return const_cast<View*>(
        static_cast<const BubbleFrameView*>(this)->title());
  }

  gfx::Insets content_margins() const { return content_margins_; }

  void SetFootnoteView(View* view);
  void set_footnote_margins(const gfx::Insets& footnote_margins) {
    footnote_margins_ = footnote_margins;
  }

  // Given the size of the contents and the rect to point at, returns the bounds
  // of the bubble window. The bubble's arrow location may change if the bubble
  // does not fit on the monitor and |adjust_if_offscreen| is true.
  gfx::Rect GetUpdatedWindowBounds(const gfx::Rect& anchor_rect,
                                   const gfx::Size& client_size,
                                   bool adjust_if_offscreen);

  bool close_button_clicked() const { return close_button_clicked_; }

  Button* GetCloseButtonForTest() { return close_; }

  // Resets the time when view has been shown. Tests may need to call this
  // method if they use events that could be otherwise treated as unintended.
  // See IsPossiblyUnintendedInteraction().
  void ResetViewShownTimeStampForTesting();

 protected:
  // Returns the available screen bounds if the frame were to show in |rect|.
  virtual gfx::Rect GetAvailableScreenBounds(const gfx::Rect& rect) const;

  // Override and return true to allow client view to overlap into the title
  // area when HasTitle() returns false and/or ShouldShowCloseButton() returns
  // true. Returns false by default.
  virtual bool ExtendClientIntoTitle() const;

  bool IsCloseButtonVisible() const;
  gfx::Rect GetCloseButtonMirroredBounds() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(BubbleFrameViewTest, GetBoundsForClientView);
  FRIEND_TEST_ALL_PREFIXES(BubbleFrameViewTest, RemoveFootnoteView);
  FRIEND_TEST_ALL_PREFIXES(BubbleFrameViewTest, LayoutWithIcon);
  FRIEND_TEST_ALL_PREFIXES(BubbleFrameViewTest, IgnorePossiblyUnintendedClicks);
  FRIEND_TEST_ALL_PREFIXES(BubbleDelegateTest, CloseReasons);
  FRIEND_TEST_ALL_PREFIXES(BubbleDialogDelegateViewTest, CloseMethods);

  // Mirrors the bubble's arrow location on the |vertical| or horizontal axis,
  // if the generated window bounds don't fit in the monitor bounds.
  void MirrorArrowIfOffScreen(bool vertical,
                              const gfx::Rect& anchor_rect,
                              const gfx::Size& client_size);

  // Adjust the bubble's arrow offsets if the generated window bounds don't fit
  // in the monitor bounds.
  void OffsetArrowIfOffScreen(const gfx::Rect& anchor_rect,
                              const gfx::Size& client_size);

  // The width of the frame for the given |client_width|. The result accounts
  // for the minimum title bar width and includes all insets and possible
  // snapping. It does not include the border.
  int GetFrameWidthForClientWidth(int client_width) const;

  // Calculates the size needed to accommodate the given client area.
  gfx::Size GetFrameSizeForClientSize(const gfx::Size& client_size) const;

  // True if the frame has a title area. This is the area affected by
  // |title_margins_|, including the icon and title text, but not the close
  // button.
  bool HasTitle() const;

  // The insets of the text portion of the title, based on |title_margins_| and
  // whether there is an icon and/or close button. Note there may be no title,
  // in which case only insets required for the close button are returned.
  gfx::Insets GetTitleLabelInsetsFromFrame() const;

  // The client_view insets (from the frame view) for the given |frame_width|.
  gfx::Insets GetClientInsetsForFrameWidth(int frame_width) const;

  // The bubble border.
  BubbleBorder* bubble_border_;

  // Margins around the title label.
  gfx::Insets title_margins_;

  // Margins between the content and the inside of the border, in pixels.
  gfx::Insets content_margins_;

  // Margins between the footnote view and the footnote container.
  gfx::Insets footnote_margins_;

  // The optional title icon.
  views::ImageView* title_icon_;

  // One of these fields is used as the dialog title. If SetTitleView is called
  // the custom title view is stored in |custom_title_| and this class assumes
  // ownership. Otherwise |default_title_| is used.
  Label* default_title_;
  View* custom_title_;

  // The optional close button (the X).
  Button* close_;

  // A view to contain the footnote view, if it exists.
  FootnoteContainerView* footnote_container_;

  // Whether the close button was clicked.
  bool close_button_clicked_;

  // Time when view has been shown.
  base::TimeTicks view_shown_time_stamp_;

  DISALLOW_COPY_AND_ASSIGN(BubbleFrameView);
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_BUBBLE_FRAME_VIEW_H_
