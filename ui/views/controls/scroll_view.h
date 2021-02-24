// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SCROLL_VIEW_H_
#define UI_VIEWS_CONTROLS_SCROLL_VIEW_H_

#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/controls/separator.h"

namespace gfx {
class ScrollOffset;
}

namespace views {
namespace test {
class ScrollViewTestApi;
}

enum class OverflowIndicatorAlignment { kLeft, kTop, kRight, kBottom };

/////////////////////////////////////////////////////////////////////////////
//
// ScrollView class
//
// A ScrollView is used to make any View scrollable. The view is added to
// a viewport which takes care of clipping.
//
// In this current implementation both horizontal and vertical scrollbars are
// added as needed.
//
// The scrollview supports keyboard UI and mousewheel.
//
/////////////////////////////////////////////////////////////////////////////

class VIEWS_EXPORT ScrollView : public View, public ScrollBarController {
 public:
  METADATA_HEADER(ScrollView);

  // Indicates whether or not scroll view is initialized with layer-scrolling.
  enum class ScrollWithLayers { kDisabled, kEnabled };

  // Controls how a scroll bar appears and functions.
  enum class ScrollBarMode {
    // The scrollbar is hidden, and the pane will not respond to e.g. mousewheel
    // events even if the contents are larger than the viewport.
    kDisabled,
    // The scrollbar is hidden whether or not the contents are larger than the
    // viewport, but the pane will respond to scroll events.
    kHiddenButEnabled,
    // The scrollbar will be visible if the contents are larger than the
    // viewport and the pane will respond to scroll events.
    kEnabled
  };

  class Observer {
   public:
    // Called when |contents_| scrolled.
    virtual void OnContentsScrolled() {}
  };

  ScrollView();

  // Additional constructor for overriding scrolling as defined by
  // |kUiCompositorScrollWithLayers|. See crbug.com/873923 for more details on
  // enabling by default this for all platforms.
  explicit ScrollView(ScrollWithLayers scroll_with_layers);

  ~ScrollView() override;

  // Creates a ScrollView with a theme specific border.
  static std::unique_ptr<ScrollView> CreateScrollViewWithBorder();

  // Returns the ScrollView for which |contents| is its contents, or null if
  // |contents| is not in a ScrollView.
  static ScrollView* GetScrollViewForContents(View* contents);

  // Set the contents. Any previous contents will be deleted. The contents
  // is the view that needs to scroll.
  template <typename T>
  T* SetContents(std::unique_ptr<T> a_view) {
    T* content_view = a_view.get();
    SetContentsImpl(std::move(a_view));
    return content_view;
  }
  void SetContents(std::nullptr_t);
  const View* contents() const { return contents_; }
  View* contents() { return contents_; }

  // Sets the header, deleting the previous header.
  template <typename T>
  T* SetHeader(std::unique_ptr<T> a_header) {
    T* header_view = a_header.get();
    SetHeaderImpl(std::move(a_header));
    return header_view;
  }
  void SetHeader(std::nullptr_t);

  int GetMaxHeight() const { return max_height_; }

  int GetMinHeight() const { return min_height_; }

  // The background color can be configured in two distinct ways:
  // . By way of SetBackgroundThemeColorId(). This is the default and when
  //   called the background color comes from the theme (and changes if the
  //   theme changes).
  // . By way of setting an explicit color, i.e. SetBackgroundColor(). Use
  //   base::nullopt if you don't want any color, but be warned this
  //   produces awful results when layers are used with subpixel rendering.
  base::Optional<SkColor> GetBackgroundColor() const;
  void SetBackgroundColor(const base::Optional<SkColor>& color);

  base::Optional<ui::NativeTheme::ColorId> GetBackgroundThemeColorId() const;
  void SetBackgroundThemeColorId(
      const base::Optional<ui::NativeTheme::ColorId>& color_id);

  // Returns the visible region of the content View.
  gfx::Rect GetVisibleRect() const;

  bool GetUseColorId() const { return !!background_color_id_; }

  ScrollBarMode GetHorizontalScrollBarMode() const {
    return horizontal_scroll_bar_mode_;
  }
  ScrollBarMode GetVerticalScrollBarMode() const {
    return vertical_scroll_bar_mode_;
  }
  bool GetTreatAllScrollEventsAsHorizontal() const {
    return treat_all_scroll_events_as_horizontal_;
  }
  void SetHorizontalScrollBarMode(ScrollBarMode horizontal_scroll_bar_mode);
  void SetVerticalScrollBarMode(ScrollBarMode vertical_scroll_bar_mode);
  void SetTreatAllScrollEventsAsHorizontal(
      bool treat_all_scroll_events_as_horizontal);

  bool GetDrawOverflowIndicator() const { return draw_overflow_indicator_; }
  void SetDrawOverflowIndicator(bool draw_overflow_indicator);

  View* SetCustomOverflowIndicator(OverflowIndicatorAlignment side,
                                   std::unique_ptr<View> indicator,
                                   int thickness,
                                   bool fills_opaquely);

  // Turns this scroll view into a bounded scroll view, with a fixed height.
  // By default, a ScrollView will stretch to fill its outer container.
  void ClipHeightTo(int min_height, int max_height);

  // Returns whether or not the ScrollView is bounded (as set by ClipHeightTo).
  bool is_bounded() const { return max_height_ >= 0 && min_height_ >= 0; }

  // Retrieves the width/height reserved for scrollbars. These return 0 if the
  // scrollbar has not yet been created or in the case of overlay scrollbars.
  int GetScrollBarLayoutWidth() const;
  int GetScrollBarLayoutHeight() const;

  // Returns the horizontal/vertical scrollbar. This may return null.
  ScrollBar* horizontal_scroll_bar() { return horiz_sb_.get(); }
  const ScrollBar* horizontal_scroll_bar() const { return horiz_sb_.get(); }
  ScrollBar* vertical_scroll_bar() { return vert_sb_.get(); }
  const ScrollBar* vertical_scroll_bar() const { return vert_sb_.get(); }

  // Customize the scrollbar design. |horiz_sb| and |vert_sb| cannot be null.
  ScrollBar* SetHorizontalScrollBar(std::unique_ptr<ScrollBar> horiz_sb);
  ScrollBar* SetVerticalScrollBar(std::unique_ptr<ScrollBar> vert_sb);

  // Gets/Sets whether this ScrollView has a focus indicator or not.
  bool GetHasFocusIndicator() const { return draw_focus_indicator_; }
  void SetHasFocusIndicator(bool has_focus_indicator);

  void AddScrollViewObserver(Observer* observer);
  void RemoveScrollViewObserver(Observer* observer);

  // View overrides:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void Layout() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& e) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnThemeChanged() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;

  // ScrollBarController overrides:
  void ScrollToPosition(ScrollBar* source, int position) override;
  int GetScrollIncrement(ScrollBar* source,
                         bool is_page,
                         bool is_positive) override;

 private:
  friend class test::ScrollViewTestApi;

  class Viewport;

  bool IsHorizontalScrollEnabled() const;
  bool IsVerticalScrollEnabled() const;

  // Forces |contents_viewport_| to have a Layer (assuming it doesn't already).
  void EnableViewportLayer();

  // Returns true if this or the viewport has a layer.
  bool DoesViewportOrScrollViewHaveLayer() const;

  // Updates or destroys the viewport layer as necessary. If any descendants
  // of the viewport have a layer, then the viewport needs to have a layer,
  // otherwise it doesn't.
  void UpdateViewportLayerForClipping();

  void SetContentsImpl(std::unique_ptr<View> a_view);
  void SetHeaderImpl(std::unique_ptr<View> a_header);

  // Used internally by SetHeaderImpl() and SetContentsImpl() to reset the view.
  // Sets |member| to |new_view|. If |new_view| is non-null it is added to
  // |parent|.
  void SetHeaderOrContents(View* parent,
                           std::unique_ptr<View> new_view,
                           View** member);

  // Scrolls the minimum amount necessary to make the specified rectangle
  // visible, in the coordinates of the contents view. The specified rectangle
  // is constrained by the bounds of the contents view. This has no effect if
  // the contents have not been set.
  void ScrollContentsRegionToBeVisible(const gfx::Rect& rect);

  // Computes the visibility of both scrollbars, taking in account the view port
  // and content sizes.
  void ComputeScrollBarsVisibility(const gfx::Size& viewport_size,
                                   const gfx::Size& content_size,
                                   bool* horiz_is_shown,
                                   bool* vert_is_shown) const;

  // Shows or hides the scrollbar/corner_view based on the value of
  // |should_show|.
  void SetControlVisibility(View* control, bool should_show);

  // Update the scrollbars positions given viewport and content sizes.
  void UpdateScrollBarPositions();

  // Helpers to get and set the current scroll offset (either from the ui::Layer
  // or from the |contents_| origin offset).
  gfx::ScrollOffset CurrentOffset() const;
  void ScrollToOffset(const gfx::ScrollOffset& offset);

  // Whether the ScrollView scrolls using ui::Layer APIs.
  bool ScrollsWithLayers() const;

  // Callback entrypoint when hosted Layers are scrolled by the Compositor.
  void OnLayerScrolled(const gfx::ScrollOffset&, const cc::ElementId&);

  // Updates accessory elements when |contents_| is scrolled.
  void OnScrolled(const gfx::ScrollOffset& offset);

  // Horizontally scrolls the header (if any) to match the contents.
  void ScrollHeader();

  void AddBorder();
  void UpdateBorder();

  void UpdateBackground();

  // Positions each overflow indicator against their respective content edge.
  void PositionOverflowIndicators();

  // Shows/hides the overflow indicators depending on the position of the
  // scrolling content within the viewport.
  void UpdateOverflowIndicatorVisibility(const gfx::ScrollOffset& offset);

  // The current contents and its viewport. |contents_| is contained in
  // |contents_viewport_|.
  View* contents_ = nullptr;
  View* contents_viewport_ = nullptr;

  // The current header and its viewport. |header_| is contained in
  // |header_viewport_|.
  View* header_ = nullptr;
  View* header_viewport_ = nullptr;

  // Horizontal scrollbar.
  std::unique_ptr<ScrollBar> horiz_sb_;

  // Vertical scrollbar.
  std::unique_ptr<ScrollBar> vert_sb_;

  // Corner view.
  std::unique_ptr<View> corner_view_;

  // Hidden content indicators
  // TODO(https://crbug.com/1166949): Use preferred width/height instead of
  // thickness members.
  std::unique_ptr<View> more_content_left_ = std::make_unique<Separator>();
  int more_content_left_thickness_ = Separator::kThickness;
  std::unique_ptr<View> more_content_top_ = std::make_unique<Separator>();
  int more_content_top_thickness_ = Separator::kThickness;
  std::unique_ptr<View> more_content_right_ = std::make_unique<Separator>();
  int more_content_right_thickness_ = Separator::kThickness;
  std::unique_ptr<View> more_content_bottom_ = std::make_unique<Separator>();
  int more_content_bottom_thickness_ = Separator::kThickness;

  // The min and max height for the bounded scroll view. These are negative
  // values if the view is not bounded.
  int min_height_ = -1;
  int max_height_ = -1;

  // See description of SetBackgroundColor() for details.
  base::Optional<SkColor> background_color_;
  base::Optional<ui::NativeTheme::ColorId> background_color_id_ =
      ui::NativeTheme::kColorId_DialogBackground;

  // How to handle the case when the contents overflow the viewport.
  ScrollBarMode horizontal_scroll_bar_mode_ = ScrollBarMode::kEnabled;
  ScrollBarMode vertical_scroll_bar_mode_ = ScrollBarMode::kEnabled;

  // Causes vertical scroll events (e.g. scrolling with the mousewheel) as
  // horizontal events, to make scrolling in horizontal-only scroll situations
  // easier for the user.
  bool treat_all_scroll_events_as_horizontal_ = false;

  // In Harmony, the indicator is a focus ring. Pre-Harmony, the indicator is a
  // different border painter.
  bool draw_focus_indicator_ = false;

  // Only needed for pre-Harmony. Remove when Harmony is default.
  bool draw_border_ = false;

  // Whether to draw a white separator on the four sides of the scroll view when
  // it overflows.
  bool draw_overflow_indicator_ = true;

  // Set to true if the scroll with layers feature is enabled.
  const bool scroll_with_layers_enabled_;

  // The focus ring for this ScrollView.
  FocusRing* focus_ring_ = nullptr;

  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(ScrollView);
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, ScrollView, View)
VIEW_BUILDER_VIEW_TYPE_PROPERTY(View, Contents)
VIEW_BUILDER_VIEW_TYPE_PROPERTY(View, Header)
VIEW_BUILDER_PROPERTY(base::Optional<ui::NativeTheme::ColorId>,
                      BackgroundThemeColorId)
VIEW_BUILDER_PROPERTY(ScrollView::ScrollBarMode, HorizontalScrollBarMode)
VIEW_BUILDER_PROPERTY(ScrollView::ScrollBarMode, VerticalScrollBarMode)
VIEW_BUILDER_PROPERTY(bool, TreatAllScrollEventsAsHorizontal)
VIEW_BUILDER_PROPERTY(bool, DrawOverflowIndicator)
VIEW_BUILDER_PROPERTY(base::Optional<SkColor>, BackgroundColor)
VIEW_BUILDER_VIEW_PROPERTY(ScrollBar, HorizontalScrollBar)
VIEW_BUILDER_VIEW_PROPERTY(ScrollBar, VerticalScrollBar)
VIEW_BUILDER_PROPERTY(bool, HasFocusIndicator)
END_VIEW_BUILDER

// VariableRowHeightScrollHelper is intended for views that contain rows of
// varying height. To use a VariableRowHeightScrollHelper create one supplying
// a Controller and delegate GetPageScrollIncrement and GetLineScrollIncrement
// to the helper. VariableRowHeightScrollHelper calls back to the
// Controller to determine row boundaries.
class VariableRowHeightScrollHelper {
 public:
  // The origin and height of a row.
  struct RowInfo {
    RowInfo(int origin, int height) : origin(origin), height(height) {}

    // Origin of the row.
    int origin;

    // Height of the row.
    int height;
  };

  // Used to determine row boundaries.
  class Controller {
   public:
    // Returns the origin and size of the row at the specified location.
    virtual VariableRowHeightScrollHelper::RowInfo GetRowInfo(int y) = 0;
  };

  // Creates a new VariableRowHeightScrollHelper. Controller is
  // NOT deleted by this VariableRowHeightScrollHelper.
  explicit VariableRowHeightScrollHelper(Controller* controller);
  virtual ~VariableRowHeightScrollHelper();

  // Delegate the View methods of the same name to these. The scroll amount is
  // determined by querying the Controller for the appropriate row to scroll
  // to.
  int GetPageScrollIncrement(ScrollView* scroll_view,
                             bool is_horizontal,
                             bool is_positive);
  int GetLineScrollIncrement(ScrollView* scroll_view,
                             bool is_horizontal,
                             bool is_positive);

 protected:
  // Returns the row information for the row at the specified location. This
  // calls through to the method of the same name on the controller.
  virtual RowInfo GetRowInfo(int y);

 private:
  Controller* controller_;

  DISALLOW_COPY_AND_ASSIGN(VariableRowHeightScrollHelper);
};

// FixedRowHeightScrollHelper is intended for views that contain fixed height
// height rows. To use a FixedRowHeightScrollHelper delegate
// GetPageScrollIncrement and GetLineScrollIncrement to it.
class FixedRowHeightScrollHelper : public VariableRowHeightScrollHelper {
 public:
  // Creates a FixedRowHeightScrollHelper. top_margin gives the distance from
  // the top of the view to the first row, and may be 0. row_height gives the
  // height of each row.
  FixedRowHeightScrollHelper(int top_margin, int row_height);

 protected:
  // Calculates the bounds of the row from the top margin and row height.
  RowInfo GetRowInfo(int y) override;

 private:
  int top_margin_;
  int row_height_;

  DISALLOW_COPY_AND_ASSIGN(FixedRowHeightScrollHelper);
};

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, ScrollView)

#endif  // UI_VIEWS_CONTROLS_SCROLL_VIEW_H_
