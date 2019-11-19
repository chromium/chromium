// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_FRAME_VIEW_H_
#define UI_VIEWS_BUBBLE_BUBBLE_FRAME_VIEW_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/input_event_activation_protector.h"
#include "ui/views/window/non_client_view.h"

namespace views {

class FootnoteContainerView;
class ImageView;

// The non-client frame view of bubble-styled widgets.
class VIEWS_EXPORT BubbleFrameView : public NonClientFrameView,
                                     public ButtonListener {
 public:
  enum class PreferredArrowAdjustment { kMirror, kOffset };

  // Internal class name.
  static const char kViewClassName[];

  BubbleFrameView(const gfx::Insets& title_margins,
                  const gfx::Insets& content_margins);
  ~BubbleFrameView() override;

  static std::unique_ptr<Label> CreateDefaultTitleLabel(
      const base::string16& title_text);

  // Creates a close button used in the corner of the dialog.
  static std::unique_ptr<Button> CreateCloseButton(ButtonListener* listener,
                                                   bool should_use_dark_colors);

  // NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  bool GetClientMask(const gfx::Size& size, SkPath* path) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  void ResetWindowControls() override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;

  // Sets a custom view to be the dialog title instead of the |default_title_|
  // label. If there is an existing title view it will be deleted.
  void SetTitleView(std::unique_ptr<View> title_view);

  // Updates the current progress value of |progress_indicator_|. If progress is
  // absent, hides |the progress_indicator|.
  void SetProgress(base::Optional<double> progress);

  // View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void Layout() override;
  void OnPaint(gfx::Canvas* canvas) override;
  void PaintChildren(const PaintInfo& paint_info) override;
  void OnThemeChanged() override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;

  // ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

  // Use SetBubbleBorder() not SetBorder().
  void SetBubbleBorder(std::unique_ptr<BubbleBorder> border);

  const View* title() const {
    return custom_title_ ? custom_title_ : default_title_;
  }
  View* title() {
    return const_cast<View*>(
        static_cast<const BubbleFrameView*>(this)->title());
  }

  gfx::Insets content_margins() const { return content_margins_; }

  // Sets a custom header view for the dialog. If there is an existing header
  // view it will be deleted. The header view will be inserted above the title,
  // so outside the content bounds. If there is a close button, it will be shown
  // in front of the header view and will overlap with it. The title will be
  // shown below the header and / or the close button, depending on which is
  // lower. An example usage for a header view would be a banner image.
  void SetHeaderView(std::unique_ptr<View> view);

  // Sets a custom footnote view for the dialog. If there is an existing
  // footnote view it will be deleted. The footnote will be rendered at the
  // bottom of the bubble, after the content view. It is separated by a 1 dip
  // line and has a solid background by being embedded in a
  // FootnoteContainerView. An example footnote would be some help text.
  void SetFootnoteView(std::unique_ptr<View> view);
  View* GetFootnoteView() const;
  void set_footnote_margins(const gfx::Insets& footnote_margins) {
    footnote_margins_ = footnote_margins;
  }

  void set_preferred_arrow_adjustment(PreferredArrowAdjustment adjustment) {
    preferred_arrow_adjustment_ = adjustment;
  }

  // TODO(crbug.com/1007604): remove this in favor of using
  // Widget::InitParams::accept_events. In the mean time, don't add new uses of
  // this flag.
  bool hit_test_transparent() const { return hit_test_transparent_; }
  void set_hit_test_transparent(bool hit_test_transparent) {
    hit_test_transparent_ = hit_test_transparent;
  }

  // Get/set the corner radius of the bubble border.
  int corner_radius() const {
    return bubble_border_ ? bubble_border_->corner_radius() : 0;
  }
  void SetCornerRadius(int radius);

  // Set the arrow of the bubble border.
  void SetArrow(BubbleBorder::Arrow arrow);

  // Set the background color of the bubble border.
  void SetBackgroundColor(SkColor color);
  SkColor GetBackgroundColor() const;

  // Given the size of the contents and the rect to point at, returns the bounds
  // of the bubble window. The bubble's arrow location may change if the bubble
  // does not fit on the monitor or anchor window (if one exists) and
  // |adjust_to_fit_available_bounds| is true.
  gfx::Rect GetUpdatedWindowBounds(const gfx::Rect& anchor_rect,
                                   const BubbleBorder::Arrow arrow,
                                   const gfx::Size& client_size,
                                   bool adjust_to_fit_available_bounds);

  Button* GetCloseButtonForTesting() { return close_; }

  View* GetHeaderViewForTesting() const { return header_view_; }

  // Resets the time when view has been shown. Tests may need to call this
  // method if they use events that could be otherwise treated as unintended.
  // See IsPossiblyUnintendedInteraction().
  void ResetViewShownTimeStampForTesting();

 protected:
  // Returns the available screen bounds if the frame were to show in |rect|.
  virtual gfx::Rect GetAvailableScreenBounds(const gfx::Rect& rect) const;

  // Returns the available anchor window bounds in the screen.
  virtual gfx::Rect GetAvailableAnchorWindowBounds() const;

  // Override and return true to allow client view to overlap into the title
  // area when HasTitle() returns false and/or ShouldShowCloseButton() returns
  // true. Returns false by default.
  virtual bool ExtendClientIntoTitle() const;

  bool IsCloseButtonVisible() const;
  gfx::Rect GetCloseButtonMirroredBounds() const;

  BubbleBorder* bubble_border_for_testing() const { return bubble_border_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(BubbleFrameViewTest, RemoveFootnoteView);
  FRIEND_TEST_ALL_PREFIXES(BubbleFrameViewTest, LayoutWithIcon);
  FRIEND_TEST_ALL_PREFIXES(BubbleFrameViewTest, LayoutWithProgressIndicator);
  FRIEND_TEST_ALL_PREFIXES(BubbleFrameViewTest, IgnorePossiblyUnintendedClicks);
  FRIEND_TEST_ALL_PREFIXES(BubbleDelegateTest, CloseReasons);
  FRIEND_TEST_ALL_PREFIXES(BubbleDialogDelegateViewTest, CloseMethods);
  FRIEND_TEST_ALL_PREFIXES(BubbleDialogDelegateViewTest, CreateDelegate);

  // Mirrors the bubble's arrow location on the |vertical| or horizontal axis,
  // if the generated window bounds don't fit in the given available bounds.
  void MirrorArrowIfOutOfBounds(bool vertical,
                                const gfx::Rect& anchor_rect,
                                const gfx::Size& client_size,
                                const gfx::Rect& available_bounds);

  // Adjust the bubble's arrow offsets if the generated window bounds don't fit
  // in the given available bounds.
  void OffsetArrowIfOutOfBounds(const gfx::Rect& anchor_rect,
                                const gfx::Size& client_size,
                                const gfx::Rect& available_bounds);

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

  // Gets the height of the |header_view_| given a |frame_width|. Returns zero
  // if there is no header view or if it is not visible.
  int GetHeaderHeightForFrameWidth(int frame_width) const;

  // The bubble border.
  BubbleBorder* bubble_border_ = nullptr;

  // Margins around the title label.
  gfx::Insets title_margins_;

  // Margins between the content and the inside of the border, in pixels.
  gfx::Insets content_margins_;

  // Margins between the footnote view and the footnote container.
  gfx::Insets footnote_margins_;

  // The optional title icon.
  ImageView* title_icon_ = nullptr;

  // One of these fields is used as the dialog title. If SetTitleView is called
  // the custom title view is stored in |custom_title_| and this class assumes
  // ownership. Otherwise |default_title_| is used.
  Label* default_title_ = nullptr;
  View* custom_title_ = nullptr;

  // The optional close button (the X).
  Button* close_ = nullptr;

  // The optional progress bar. Used to indicate bubble pending state. By
  // default it is invisible.
  ProgressBar* progress_indicator_ = nullptr;

  // The optional header view.
  View* header_view_ = nullptr;

  // A view to contain the footnote view, if it exists.
  FootnoteContainerView* footnote_container_ = nullptr;

  // Set preference for how the arrow will be adjusted if the window is outside
  // the available bounds.
  PreferredArrowAdjustment preferred_arrow_adjustment_ =
      PreferredArrowAdjustment::kMirror;

  // If true the view is transparent to all hit tested events (i.e. click and
  // hover). DEPRECATED: See note above set_hit_test_transparent().
  bool hit_test_transparent_ = false;

  InputEventActivationProtector input_protector_;

  DISALLOW_COPY_AND_ASSIGN(BubbleFrameView);
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_BUBBLE_FRAME_VIEW_H_
