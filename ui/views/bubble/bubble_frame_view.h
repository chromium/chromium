// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_FRAME_VIEW_H_
#define UI_VIEWS_BUBBLE_BUBBLE_FRAME_VIEW_H_

#include <memory>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/input_event_activation_protector.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/window/non_client_view.h"

namespace gfx {
class RoundedCornersF;
}

namespace views {

class FootnoteContainerView;
class ImageView;

// The non-client frame view of bubble-styled widgets.
//  +- BubbleFrameView ------------------+
//  | +- ProgressBar ------------------+ |
//  | +-----------------------(-)-(x)-+  |
//  | | HeaderView                    |  |
//  | +-------------------------------+  |
//  | +-------------------------------+  |
//  | | TitleView                     |  |
//  | +-------------------------------+  |
//  | +-- DialogClientView------------+  |
//  | | <<Dialog Contents View>>      |  |
//  | | <<OK and Cancel Buttons>>     |  |
//  | | <<...>>                       |  |
//  | +-------------------------------+  |
//  | +-------------------------------+  |
//  | | FootnoteView                  |  |
//  | +-------------------------------+  |
//  +------------------------------------+
// All views are optional except for DialogClientView. An ImageView
// `main_image` might optionally occupy the top left corner (not
// illustrated above).
// If TitleView exists and HeaderView does not exists, the close
// and the minimize buttons will be positioned at the end of the
// title row. Otherwise, they will be positioned closer to the frame
// edge.
class VIEWS_EXPORT BubbleFrameView : public NonClientFrameView {
  METADATA_HEADER(BubbleFrameView, NonClientFrameView)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMinimizeButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCloseButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kProgressIndicatorElementId);

  enum class PreferredArrowAdjustment { kMirror, kOffset };

  BubbleFrameView(const gfx::Insets& title_margins,
                  const gfx::Insets& content_margins);
  BubbleFrameView(const BubbleFrameView&) = delete;
  BubbleFrameView& operator=(BubbleFrameView&) = delete;
  ~BubbleFrameView() override;

  static std::unique_ptr<Label> CreateDefaultTitleLabel(
      const std::u16string& title_text);

  // Creates a close button used in the corner of the dialog.
  static std::unique_ptr<Button> CreateCloseButton(
      Button::PressedCallback callback);

  // Creates a minimize button used in the corner of the dialog.
  static std::unique_ptr<Button> CreateMinimizeButton(
      Button::PressedCallback callback);

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
  void InsertClientView(ClientView* client_view) override;
  void UpdateWindowRoundedCorners() override;
  bool HasWindowTitle() const override;
  bool IsWindowTitleVisible() const override;

  // Sets a custom view to be the dialog title instead of the |default_title_|
  // label. If there is an existing title view it will be deleted.
  void SetTitleView(std::unique_ptr<View> title_view);

  // Updates the subtitle label from the BubbleDialogDelegate.
  void UpdateSubtitle();

  // Signals that the main image may have changed and needs to be fetched again.
  void UpdateMainImage();

  // Updates the current progress value of |progress_indicator_|. If progress is
  // absent, hides |the progress_indicator|.
  void SetProgress(std::optional<double> progress);
  // Returns the current progress value of |progress_indicator_| if
  // |progress_indicator_| is visible.
  std::optional<double> GetProgress() const;

  // View:
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void Layout(PassKey) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void PaintChildren(const PaintInfo& paint_info) override;
  void OnThemeChanged() override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;

  // Use SetBubbleBorder() not SetBorder().
  void SetBubbleBorder(std::unique_ptr<BubbleBorder> border);

  const View* title() const {
    return custom_title_ ? custom_title_.get() : default_title_.get();
  }
  View* title() {
    return const_cast<View*>(
        static_cast<const BubbleFrameView*>(this)->title());
  }

  Label* default_title() { return default_title_.get(); }

  void SetContentMargins(const gfx::Insets& content_margins);
  gfx::Insets GetContentMargins() const;

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
  void SetFootnoteMargins(const gfx::Insets& footnote_margins);
  gfx::Insets GetFootnoteMargins() const;

  void SetPreferredArrowAdjustment(PreferredArrowAdjustment adjustment);
  PreferredArrowAdjustment GetPreferredArrowAdjustment() const;

  // TODO(crbug.com/40100380): remove this in favor of using
  // Widget::InitParams::accept_events. In the mean time, don't add new uses of
  // this flag.
  bool hit_test_transparent() const { return hit_test_transparent_; }
  void set_hit_test_transparent(bool hit_test_transparent) {
    hit_test_transparent_ = hit_test_transparent;
  }

  void set_use_anchor_window_bounds(bool use_anchor_window_bounds) {
    use_anchor_window_bounds_ = use_anchor_window_bounds;
  }

  // Set the corner radius of the bubble border.
  void SetCornerRadius(int radius);
  int GetCornerRadius() const;

  // Set the arrow of the bubble border.
  void SetArrow(BubbleBorder::Arrow arrow);
  BubbleBorder::Arrow GetArrow() const;

  // Specify whether the frame should include a visible, caret-shaped arrow.
  void SetDisplayVisibleArrow(bool display_visible_arrow);
  bool GetDisplayVisibleArrow() const;

  // Set the background color of the bubble border.
  // TODO(b/261653838): Update this function to use color id instead.
  void SetBackgroundColor(SkColor color);
  SkColor GetBackgroundColor() const;

  // For masking reasons, the ClientView may be painted to a textured layer. To
  // ensure bubbles that rely on the frame background color continue to work as
  // expected, we must set the background of the ClientView to match that of the
  // BubbleFrameView.
  void UpdateClientViewBackground();

  // Given the size of the contents and the rect to point at, returns the bounds
  // of the bubble window. The bubble's arrow location may change if the bubble
  // does not fit on the monitor or anchor window (if one exists) and
  // |adjust_to_fit_available_bounds| is true.
  gfx::Rect GetUpdatedWindowBounds(const gfx::Rect& anchor_rect,
                                   const BubbleBorder::Arrow arrow,
                                   const gfx::Size& client_size,
                                   bool adjust_to_fit_available_bounds);

  Button* close_button() { return close_; }
  const Button* close_button() const { return close_; }

  View* GetHeaderViewForTesting() const { return header_view_; }

  // Update the |view_shown_time_stamp_| of input protector. A short time
  // from this point onward, input event will be ignored.
  void UpdateInputProtectorTimeStamp();

  // Resets the time when view has been shown. Tests may need to call this
  // method if they use events that could be otherwise treated as unintended.
  // See IsPossiblyUnintendedInteraction().
  void ResetViewShownTimeStampForTesting();

  BubbleBorder* bubble_border() const { return bubble_border_; }

  // Returns the client_view insets from the frame view.
  gfx::Insets GetClientViewInsets() const;

  using HitTestCallback = base::RepeatingCallback<int(const gfx::Point& point)>;
  void set_non_client_hit_test_cb(HitTestCallback non_client_hit_test_cb) {
    non_client_hit_test_cb_ = std::move(non_client_hit_test_cb);
  }

 protected:
  // Returns the available screen bounds if the frame were to show in |rect|.
  virtual gfx::Rect GetAvailableScreenBounds(const gfx::Rect& rect) const;

  // Returns the available anchor window bounds in the screen.
  // This will only be used if `use_anchor_window_bounds_` is true.
  virtual gfx::Rect GetAvailableAnchorWindowBounds() const;

  // Override and return true to allow client view to overlap into the title
  // area when HasTitle() returns false and/or ShouldShowCloseButton() returns
  // true. Returns false by default.
  virtual bool ExtendClientIntoTitle() const;

  bool IsCloseButtonVisible() const;
  gfx::Rect GetCloseButtonMirroredBounds() const;

  // Helper function that gives the corner radius values that should be applied
  // to the BubbleFrameView's client view. These values depend on the amount of
  // inset present on the client view and the presence of header and footer
  // views.
  gfx::RoundedCornersF GetClientCornerRadii() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(BubbleFrameViewTest, RemoveFootnoteView);
  FRIEND_TEST_ALL_PREFIXES(BubbleFrameViewTest, LayoutWithIcon);
  FRIEND_TEST_ALL_PREFIXES(BubbleFrameViewTest, LayoutWithProgressIndicator);
  FRIEND_TEST_ALL_PREFIXES(BubbleFrameViewTest,
                           IgnorePossiblyUnintendedClicksClose);
  FRIEND_TEST_ALL_PREFIXES(BubbleFrameViewTest,
                           IgnorePossiblyUnintendedClicksMinimize);
  FRIEND_TEST_ALL_PREFIXES(BubbleFrameViewTest,
                           IgnorePossiblyUnintendedClicksAnchorBoundsChanged);
  FRIEND_TEST_ALL_PREFIXES(BubbleDelegateTest, CloseReasons);
  FRIEND_TEST_ALL_PREFIXES(BubbleDialogDelegateViewTest, CloseMethods);
  FRIEND_TEST_ALL_PREFIXES(BubbleDialogDelegateViewTest, CreateDelegate);

  // The positioning options for the close button and the minimize button.
  enum class ButtonsPositioning {
    // The buttons are positioned at the end of the title row.
    kInTitleRow,
    // The buttons are positioned on the upper trailing corner of the
    // bubble. The distance between buttons and the frame edge will be shorter
    // than `kInTitleRow`.
    kOnFrameEdge,
  };

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

  // Returns the positioning options for the buttons.
  ButtonsPositioning GetButtonsPositioning() const;

  // Returns true if there're buttons in the title row.
  bool TitleRowHasButtons() const;

  // The insets of the text portion of the title, based on |title_margins_| and
  // whether there is an icon and/or close button. Note there may be no title,
  // in which case only insets required for the close button are returned.
  gfx::Insets GetTitleLabelInsetsFromFrame() const;

  // The client_view insets (from the frame view) for the given |frame_width|.
  gfx::Insets GetClientInsetsForFrameWidth(int frame_width) const;

  // Gets the height of the |header_view_| given a |frame_width|. Returns zero
  // if there is no header view or if it is not visible.
  int GetHeaderHeightForFrameWidth(int frame_width) const;

  // Updates the corner radius of a layer backed client view for MD rounded
  // corners.
  // TODO(tluk): Use this and remove the need for GetClientMask() for clipping
  // client views to the bubble border's bounds.
  void UpdateClientLayerCornerRadius();

  int GetMainImageLeftInsets() const;

  gfx::Point GetButtonAreaTopRight() const;

  gfx::Size GetButtonAreaSize() const;

  // Helper method to create a label with text style
  static std::unique_ptr<Label> CreateLabelWithContextAndStyle(
      const std::u16string& label_text,
      style::TextContext text_context,
      style::TextStyle text_style);

  // The bubble border.
  raw_ptr<BubbleBorder> bubble_border_ = nullptr;

  // Margins around the title label.
  const gfx::Insets title_margins_;

  // Margins between the content and the inside of the border, in pixels.
  gfx::Insets content_margins_;

  // Margins between the footnote view and the footnote container.
  gfx::Insets footnote_margins_;

  // The optional title icon.
  raw_ptr<ImageView> title_icon_ = nullptr;

  // The optional main image.
  raw_ptr<ImageView> main_image_ = nullptr;

  raw_ptr<BoxLayoutView> title_container_ = nullptr;

  // One of these fields is used as the dialog title. If SetTitleView is called
  // the custom title view is stored in `custom_title_` and this class assumes
  // ownership. Otherwise `default_title_` is used.
  raw_ptr<Label> default_title_ = nullptr;
  raw_ptr<View> custom_title_ = nullptr;

  raw_ptr<Label> subtitle_ = nullptr;

  // The optional minimize button (the _).
  raw_ptr<Button> minimize_ = nullptr;

  // The optional close button (the X).
  raw_ptr<Button> close_ = nullptr;

  // The optional progress bar. Used to indicate bubble pending state. By
  // default it is invisible.
  raw_ptr<ProgressBar> progress_indicator_ = nullptr;

  // The optional header view.
  raw_ptr<View> header_view_ = nullptr;

  // A view to contain the footnote view, if it exists.
  raw_ptr<FootnoteContainerView> footnote_container_ = nullptr;

  // Set preference for how the arrow will be adjusted if the window is outside
  // the available bounds.
  PreferredArrowAdjustment preferred_arrow_adjustment_ =
      PreferredArrowAdjustment::kMirror;

  // If true the view is transparent to all hit tested events (i.e. click and
  // hover). DEPRECATED: See note above set_hit_test_transparent().
  bool hit_test_transparent_ = false;

  // If true the bubble will try to stay inside the bounds returned by
  // `GetAvailableAnchorWindowBounds`.
  bool use_anchor_window_bounds_ = true;

  // Set by bubble clients to compose additional non-client hit test rules for
  // their host bubble. HTNOWHERE should be returned to tell the caller to do
  // further processing to determine where in the non-client area the tested
  // point is (if present at all). See NonClientFrameView::NonClientHitTest()
  // for details.
  HitTestCallback non_client_hit_test_cb_;

  InputEventActivationProtector input_protector_;
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_BUBBLE_FRAME_VIEW_H_
