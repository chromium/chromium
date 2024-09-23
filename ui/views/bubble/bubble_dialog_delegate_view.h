// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_DIALOG_DELEGATE_VIEW_H_
#define UI_VIEWS_BUBBLE_BUBBLE_DIALOG_DELEGATE_VIEW_H_

#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/base/class_property.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_utils.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/cocoa/bubble_closer.h"
#endif

namespace gfx {
class Rect;
}

namespace views {

class Button;

class VIEWS_EXPORT BubbleDialogDelegate : public DialogDelegate {
 public:
  BubbleDialogDelegate(
      View* anchor_view,
      BubbleBorder::Arrow arrow,
      BubbleBorder::Shadow shadow = BubbleBorder::DIALOG_SHADOW,
      bool autosize = false);
  BubbleDialogDelegate(const BubbleDialogDelegate& other) = delete;
  BubbleDialogDelegate& operator=(const BubbleDialogDelegate& other) = delete;
  ~BubbleDialogDelegate() override;

  // DialogDelegate:
  BubbleDialogDelegate* AsBubbleDialogDelegate() override;
  std::unique_ptr<NonClientFrameView> CreateNonClientFrameView(
      Widget* widget) override;
  ClientView* CreateClientView(Widget* widget) override;
  ax::mojom::Role GetAccessibleWindowRole() final;

  // Create and initialize the bubble Widget with proper bounds.
  // The default ownership for now is NATIVE_WIDGET_OWNS_WIDGET. If any other
  // ownership mode is used, the returned Widget's lifetime must be managed by
  // the caller. This is usually done by wrapping the pointer as a unique_ptr
  // using base::WrapUnique().
  static Widget* CreateBubble(
      std::unique_ptr<BubbleDialogDelegate> bubble_delegate,
      Widget::InitParams::Ownership ownership =
          Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);

  //////////////////////////////////////////////////////////////////////////////
  // The anchor view and rectangle:
  //
  // The anchor view takes priority over the anchor rectangle.
  // If the anchor moves, BubbleDialogDelegate will move its Widget to maintain
  // the same position relative to its anchor. If an anchor view is used this
  // happens automatically; if an anchor rect is used, the new anchor rect needs
  // to be supplied via SetAnchorRect().

  void SetAnchorView(View* view);
  View* GetAnchorView() const;

  void SetMainImage(ui::ImageModel main_image);
  const ui::ImageModel& GetMainImage() const { return main_image_; }

  // GetAnchorRect() takes into account the presence of an anchor view, while
  // anchor_rect() always returns the configured anchor rect, regardless of
  // whether there is also an anchor view. While it is possible to override
  // GetAnchorRect(), you should not need to do so; if you do, you must remember
  // to call OnAnchorBoundsChanged() when the return value of GetAnchorRect()
  // changes.
  //
  // TODO(ellyjones): Remove overrides of GetAnchorRect() and make this not
  // virtual.
  virtual gfx::Rect GetAnchorRect() const;
  const std::optional<gfx::Rect>& anchor_rect() const { return anchor_rect_; }
  void SetAnchorRect(const gfx::Rect& rect);

  //////////////////////////////////////////////////////////////////////////////
  // The anchor widget:
  //
  // The bubble will close when the anchor widget closes. Also, when the anchor
  // widget moves, the bubble will recompute its location from its anchor view.
  // The bubble will also cause its anchor widget to paint as active when the
  // bubble is active, and will optionally resize itself to fit within the
  // anchor widget if the anchor widget's size changes.
  //
  // The anchor widget is implied by the anchor view - bubbles with no anchor
  // view cannot be anchored to a widget.

  Widget* anchor_widget() { return anchor_widget_; }

  //////////////////////////////////////////////////////////////////////////////
  // The arrow:
  //
  // Each bubble has an "arrow", which describes the relationship between the
  // bubble's position and the position of its anchor view. The arrow also
  // supplies the - anchor offset eg, a top-left arrow puts the bubble below and
  // to the right of the anchor view, and so on. The "arrow" name is a holdover
  // from an earlier time when the arrow was an actual visual marker on the
  // bubble's border as well, but these days the arrow has no visual presence.
  //
  // The arrow is automatically flipped in RTL locales, and by default is
  // manually adjusted if necessary to fit the bubble on screen.

  // Sets the desired arrow for the bubble and updates the bubble's bounds.
  void SetArrow(BubbleBorder::Arrow arrow);
  BubbleBorder::Arrow arrow() const { return arrow_; }

  // Sets the arrow without recalculating or updating bounds. This could be used
  // before another function call which also sets bounds, so that bounds are
  // not set multiple times in a row. When animating bounds changes, setting
  // bounds twice in a row can make the widget position jump.
  // TODO(crbug.com/41470150) It would be good to be able to re-target the
  // animation rather than expect callers to use SetArrowWithoutResizing if they
  // are also changing the anchor rect, or similar.
  void SetArrowWithoutResizing(BubbleBorder::Arrow arrow);

  // Whether the arrow will be automatically adjusted if needed to fit the
  // bubble on screen. Has no effect if the bubble has no arrow.
  bool adjust_if_offscreen() const { return adjust_if_offscreen_; }
  void set_adjust_if_offscreen(bool adjust) { adjust_if_offscreen_ = adjust; }

  //////////////////////////////////////////////////////////////////////////////
  // Shadows:
  //
  // Bubbles may optionally have a shadow. Only some platforms support drawing
  // custom shadows on a bubble.

  BubbleBorder::Shadow GetShadow() const;
  void set_shadow(BubbleBorder::Shadow shadow) { shadow_ = shadow; }

  // Call this method to inform BubbleDialogDelegate that the return value of
  // GetAnchorRect() has changed. You only need to do this if you have
  // overridden GetAnchorRect() - if you are using an anchor view or anchor rect
  // normally, do not call this.
  virtual void OnAnchorBoundsChanged();

  // Call this method to update view shown time stamp of underneath input
  // protectors.
  void UpdateInputProtectorsTimeStamp();

  //////////////////////////////////////////////////////////////////////////////
  // Subtitle:
  //
  // Bubbles have an optional a Subtitle label under the Title.
  // This subtitle label is represented in BubbleFrameView.

  // This method is virtual for BubbleFrameViewUnitTest purposes.
  // Not intended to be overridden in production.
  virtual std::u16string GetSubtitle() const;
  void SetSubtitle(const std::u16string& subtitle);

  bool GetSubtitleAllowCharacterBreak() const;
  void SetSubtitleAllowCharacterBreak(bool allow);

  // No setter: autosize_ should not be changed after construction.
  bool is_autosized() const { return autosize_; }

  //////////////////////////////////////////////////////////////////////////////
  // Miscellaneous bubble behaviors:
  //

  // Represents a pin that prevents a widget from closing on deactivation, even
  // if `close_on_deactivate` is set to true. Prevents closing on deactivation
  // until its destruction; if it outlives the widget it does nothing.
  class VIEWS_EXPORT CloseOnDeactivatePin {
   public:
    virtual ~CloseOnDeactivatePin();

    CloseOnDeactivatePin(const CloseOnDeactivatePin&) = delete;
    void operator=(const CloseOnDeactivatePin&) = delete;

   private:
    class Pins;
    friend class BubbleDialogDelegate;
    explicit CloseOnDeactivatePin(base::WeakPtr<Pins> pins);

    const base::WeakPtr<Pins> pins_;
  };

  // Whether the bubble closes when it ceases to be the active window.
  void set_close_on_deactivate(bool close) { close_on_deactivate_ = close; }

  // Returns whether the bubble should close on deactivation. May not match
  // `close_on_deactivate` if PreventCloseOnDeactivate() has been called.
  bool ShouldCloseOnDeactivate() const;

  // Prevents close-on-deactivate for the duration of the lifetime of the pin
  // that is returned. The pin does nothing after the widget is closed.
  std::unique_ptr<CloseOnDeactivatePin> PreventCloseOnDeactivate();

  // Explicitly set the button to automatically highlight when the bubble is
  // shown. By default the anchor is highlighted, if it is a button.
  //
  // TODO(ellyjones): Is there ever a situation where this is the right thing to
  // do UX-wise? It seems very odd to highlight something other than the anchor
  // view.
  void SetHighlightedButton(Button* highlighted_button);

  // The bubble's parent window - this can only be usefully set before creating
  // the bubble's widget. If there is one, the bubble will be stacked above it,
  // and it will become the Views parent window for the bubble.
  //
  // TODO(ellyjones):
  // - When does one actually need to call this?
  // - Why is it separate from the anchor widget?
  // - Why do most bubbles seem to work fine without this?
  gfx::NativeView parent_window() const { return parent_window_; }
  void set_parent_window(gfx::NativeView window) { parent_window_ = window; }

  bool has_parent() { return has_parent_; }
  void set_has_parent(bool has_parent) { has_parent_ = has_parent; }

  // Whether the bubble accepts mouse events or not.
  bool accept_events() const { return accept_events_; }
  void set_accept_events(bool accept_events) { accept_events_ = accept_events; }

  // Whether focus can traverse from the anchor view into the bubble. Only
  // meaningful if there is an anchor view.
  // TODO(pbos): See if this can be inferred from if the bubble is activatable
  // or if there's anything focusable within the dialog. This is currently used
  // for bubbles that should never receive focus and we should be able have
  // focus go through a bubble if nothing's focusable within it. Without this
  // set to `false`, the existence of an InfoBubble in the QR reader bubble will
  // break focus order in the parent dialog. This is a bug for which
  // set_focus_traversable_from_anchor_view(false) is used as a workaround. See
  // if fixing that bug removes the need for this for other dialogs.
  void set_focus_traversable_from_anchor_view(bool focusable) {
    focus_traversable_from_anchor_view_ = focusable;
  }

  // If this is true and either:
  // - The anchor View is a Button, or
  // - The highlighted Button is set,
  // then BubbleDialogDelegate will ask the anchor View / highlighted button to
  // highlight itself when the BubbleDialogDelegate's Widget is shown.
  void set_highlight_button_when_shown(bool highlight) {
    highlight_button_when_shown_ = highlight;
  }

  //////////////////////////////////////////////////////////////////////////////
  // Layout & colors:
  //
  // In general you shouldn't need to call any setters. If the default bubble
  // look and feel does not work for your use case, BubbleDialogDelegate may not
  // be a good fit for the UI you are building.

  // Ensures the bubble's background color is up-to-date, then returns it.
  SkColor GetBackgroundColor();

  // Direct access to the background color. Only use the getter when you know
  // you don't need to worry about the color being out-of-date due to a recent
  // theme update.
  SkColor color() const { return color_; }
  void set_color(SkColor color) {
    color_ = color;
    color_explicitly_set_ = true;
  }

  void set_force_create_contents_background(
      bool force_create_contents_background) {
    force_create_contents_background_ = force_create_contents_background;
  }

  void set_title_margins(const gfx::Insets& title_margins) {
    title_margins_ = title_margins;
  }

  gfx::Insets footnote_margins() const { return footnote_margins_; }
  void set_footnote_margins(const gfx::Insets& footnote_margins) {
    footnote_margins_ = footnote_margins;
  }

  // Sets whether or not CreateClientView() returns a Layer backed ClientView.
  // TODO(pbos): Remove all calls to this, then remove `paint_client_to_layer_`.
  // See comment around `paint_client_to_layer_`.
  void SetPaintClientToLayer(bool paint_client_to_layer);

  // Sets the content margins to a default picked for smaller bubbles.
  void UseCompactMargins();

  // Override to configure the layer type of the bubble widget.
  virtual ui::LayerType GetLayerType() const;

  // Override to provide custom parameters before widget initialization.
  virtual void OnBeforeBubbleWidgetInit(Widget::InitParams* params,
                                        Widget* widget) const {}

  // Get the maximum available screen space to place a bubble anchored to
  // |anchor_view| at |arrow|. If offscreen adjustment is on, this would return
  // the max space corresponding to the possible arrow positions of the bubble.
  // NOTE: This function should not be called in ozone platforms where global
  // screen coordinates are not available.
  static gfx::Size GetMaxAvailableScreenSpaceToPlaceBubble(
      View* anchor_view,
      BubbleBorder::Arrow arrow,
      bool adjust_if_offscreen,
      BubbleFrameView::PreferredArrowAdjustment arrow_adjustment);

  // Get the available space to place a bubble anchored to |anchor_rect| at
  // |arrow| inside |screen_rect|.
  static gfx::Size GetAvailableSpaceToPlaceBubble(BubbleBorder::Arrow arrow,
                                                  gfx::Rect anchor_rect,
                                                  gfx::Rect screen_rect);

  // Resize the bubble to fit its contents, and maybe move it if needed to keep
  // it anchored properly. This does not need to be invoked normally. This
  // should be called only if you need to force update the bounds of the widget
  // and/or position of the bubble, for example if the size of the bubble's
  // content view changed.
  // TODO(crbug.com/41493925) Not recommended; Use autosize in the constructor
  // instead.
  void SizeToContents();

 protected:
  // A helper class for logging UMA metrics related to bubbles.
  // The class logs metrics to:
  // 1. An aggregated histogram for all bubbles.
  // 2. A histogram specific to a bubble subclass when its name is provided.
  class VIEWS_EXPORT BubbleUmaLogger {
   public:
    BubbleUmaLogger();
    ~BubbleUmaLogger();

    void set_delegate(views::BubbleDialogDelegate* delegate) {
      delegate_ = delegate;
    }
    void set_bubble_view(views::View* view) { bubble_view_ = view; }

    void set_allowed_class_names_for_testing(
        const base::span<const char* const>& value) {
      allowed_class_names_for_testing_ = value;
    }

    std::optional<std::string> GetBubbleName() const;

    base::WeakPtr<BubbleUmaLogger> GetWeakPtr();

    // Logs a metric value to UMA histograms. This method logs to:
    // - "Bubble.All.{histogram_name}" for the general bubble metric.
    // - "Bubble.{bubble_name}.{histogram_name}" for a specific bubble
    //   subclass, if `bubble_name` is set.
    template <typename Value>
    void LogMetric(void (*uma_func)(const std::string&, Value),
                   const std::string& histogram_name,
                   Value value) const;

   private:
    std::optional<raw_ptr<views::View>> bubble_view_;
    std::optional<raw_ptr<views::BubbleDialogDelegate>> delegate_;
    std::optional<base::raw_span<const char* const>>
        allowed_class_names_for_testing_;
    base::WeakPtrFactory<BubbleUmaLogger> weak_factory_{this};
  };

  // Override this method if you want to position the bubble regardless of its
  // anchor, while retaining the other anchor view logic.
  virtual gfx::Rect GetBubbleBounds();

  // Override this to perform initialization after the Widget is created but
  // before it is shown.
  // TODO(pbos): Turn this into a (Once?)Callback and add set_init(cb).
  virtual void Init() {}

  // TODO(ellyjones): Replace uses of this with uses of set_color(), and/or
  // otherwise get rid of this function.
  void set_color_internal(SkColor color) { color_ = color; }

  bool color_explicitly_set() const { return color_explicitly_set_; }

  BubbleUmaLogger& bubble_uma_logger() { return bubble_uma_logger_; }

  // Redeclarations of virtuals that BubbleDialogDelegate used to inherit from
  // WidgetObserver. These should not exist; do not add new overrides of them.
  // They exist to allow the WidgetObserver helper classes inside
  // BubbleDialogDelegate (AnchorWidgetObserver and BubbleWidgetObserver) to
  // forward specific events to BubbleDialogDelegate subclasses that were
  // overriding WidgetObserver methods from BubbleDialogDelegate. Whether they
  // are called for the anchor widget or the bubble widget and when is
  // deliberately unspecified.
  //
  // TODO(ellyjones): Get rid of these.
  virtual void OnWidgetClosing(Widget* widget) {}
  virtual void OnWidgetDestroying(Widget* widget) {}
  virtual void OnWidgetActivationChanged(Widget* widget, bool active) {}
  virtual void OnWidgetDestroyed(Widget* widget) {}
  virtual void OnWidgetBoundsChanged(Widget* widget, const gfx::Rect& bounds) {}
  virtual void OnWidgetVisibilityChanged(Widget* widget, bool visible) {}

 private:
  class AnchorViewObserver;
  class AnchorWidgetObserver;
  class BubbleWidgetObserver;
  class ThemeObserver;

  FRIEND_TEST_ALL_PREFIXES(BubbleDialogDelegateViewTest,
                           VisibleWidgetShowsInkDropOnAttaching);
  FRIEND_TEST_ALL_PREFIXES(BubbleDialogDelegateViewTest,
                           AttachedWidgetShowsInkDropWhenVisible);
  FRIEND_TEST_ALL_PREFIXES(BubbleDialogDelegateViewTest,
                           MultipleBubbleAnchorHighlightTestInOrder);
  FRIEND_TEST_ALL_PREFIXES(BubbleDialogDelegateViewTest,
                           MultipleBubbleAnchorHighlightTestOutOfOrder);

  friend class AnchorViewObserver;
  friend class AnchorWidgetObserver;
  friend class BubbleWidgetObserver;
  friend class TestBubbleUmaLogger;
  friend class ThemeObserver;

  friend class BubbleBorderDelegate;
  friend class BubbleWindowTargeter;

  // Notify the BubbleDialogDelegate about changes in the anchor Widget. You do
  // not need to call these yourself.
  void OnAnchorWidgetDestroying();
  void OnAnchorWidgetBoundsChanged();

  // Notify the BubbleDialogDelegate about changes in the bubble Widget. You do
  // not need to call these yourself.
  void OnBubbleWidgetClosing();
  void OnBubbleWidgetVisibilityChanged(bool visible);
  void OnBubbleWidgetActivationChanged(bool active);
  void OnBubbleWidgetPaintAsActiveChanged();

  void OnDeactivate();

  // Update the bubble color from the NativeTheme unless it was explicitly set.
  void UpdateColorsFromTheme();

  // Notify this bubble that it is now the primary anchored bubble. When a new
  // bubble becomes the primary anchor, the previous primary silently loses its
  // primary status. This method is only called when this bubble becomes primary
  // after losing it.
  void NotifyAnchoredBubbleIsPrimary();

  void UpdateHighlightedButton(bool highlight);

  void SetAnchoredDialogKey();

  gfx::Rect GetDesiredBubbleBounds();

  gfx::Insets title_margins_;
  gfx::Insets footnote_margins_;
  BubbleBorder::Arrow arrow_ = BubbleBorder::NONE;
  BubbleBorder::Shadow shadow_;
  SkColor color_ = gfx::kPlaceholderColor;
  bool color_explicitly_set_ = false;
  raw_ptr<Widget> anchor_widget_ = nullptr;
  std::unique_ptr<AnchorViewObserver> anchor_view_observer_;
  std::unique_ptr<AnchorWidgetObserver> anchor_widget_observer_;
  std::unique_ptr<BubbleWidgetObserver> bubble_widget_observer_;
  std::unique_ptr<ThemeObserver> theme_observer_;
  bool adjust_if_offscreen_ = true;
  bool focus_traversable_from_anchor_view_ = true;
  ViewTracker highlighted_button_tracker_;
  ui::ImageModel main_image_;
  std::u16string subtitle_;
  bool subtitle_allow_character_break_ = false;

  // Whether the bubble should automatically resize to match its contents'
  // preferred size.
  bool autosize_ = false;

  // A flag controlling bubble closure on deactivation.
  bool close_on_deactivate_ = true;
  std::unique_ptr<CloseOnDeactivatePin::Pins> close_on_deactivate_pins_;

  // Whether the |anchor_widget_| (or the |highlighted_button_tracker_|, when
  // provided) should be highlighted when this bubble is shown.
  bool highlight_button_when_shown_ = true;

  mutable std::optional<gfx::Rect> anchor_rect_;

  bool accept_events_ = true;
  gfx::NativeView parent_window_ = gfx::NativeView();

  // By default, all BubbleDialogDelegates have parent windows.
  bool has_parent_ = true;

  // Pointer to this bubble's ClientView.
  raw_ptr<ClientView> client_view_ = nullptr;

  // A BubbleFrameView will apply a masking path to its ClientView to ensure
  // contents are appropriately clipped to the frame's rounded corners. If the
  // bubble uses layers in its views hierarchy, these will not be clipped to
  // the client mask unless the ClientView is backed by a textured ui::Layer.
  // This flag tracks whether or not to to create a layer backed ClientView.
  //
  // TODO(tluk): Fix all cases where bubble transparency is used and have bubble
  // ClientViews always paint to a layer.
  // TODO(tluk): Flip this to true for all bubbles.
  bool paint_client_to_layer_ = false;

  // If true, contents view will be forced to create a solid color background in
  // UpdateColorsFromTheme().
  bool force_create_contents_background_ = false;

#if BUILDFLAG(IS_MAC)
  // Special handler for close_on_deactivate() on Mac. Window (de)activation is
  // suppressed by the WindowServer when clicking rapidly, so the bubble must
  // monitor clicks as well for the desired behavior.
  std::unique_ptr<ui::BubbleCloser> mac_bubble_closer_;
#endif

  // Used to ensure the button remains anchored while this dialog is open.
  std::optional<Button::ScopedAnchorHighlight> button_anchor_highlight_;

  // The helper class that logs common bubble metrics.
  BubbleUmaLogger bubble_uma_logger_;

  std::optional<base::TimeTicks> bubble_created_time_;

  // Timestamp when the bubble turns visible.
  std::optional<base::TimeTicks> bubble_shown_time_;

  // Cumulated time of bubble being visible.
  base::TimeDelta bubble_shown_duration_;
};

// BubbleDialogDelegateView is a BubbleDialogDelegate that is also a View.
// Prefer using a BubbleDialogDelegate that sets a separate View as its contents
// view.
// TODO(pbos): Migrate existing uses of BubbleDialogDelegateView to directly
// inherit or use BubbleDialogDelegate.
class VIEWS_EXPORT BubbleDialogDelegateView : public View,
                                              public BubbleDialogDelegate {
  METADATA_HEADER(BubbleDialogDelegateView, View)

 public:
  template <typename T>
  static bool IsBubbleDialogDelegateView(const BubbleDialogDelegateView* view) {
    return ui::metadata::IsClass<T, BubbleDialogDelegateView>(view);
  }

  // Create and initialize the bubble Widget(s) with proper bounds.
  // Like BubbleDialogDelegate::CreateBubble, the default ownership for now is
  // NATIVE_WIDGET_OWNS_WIDGET. If any other ownership mode is used, the
  // returned Widget's lifetime must be managed by the caller. This is usually
  // done by wrapping the pointer as a unique_ptr using base::WrapUnique().
  template <typename T>
  static Widget* CreateBubble(
      std::unique_ptr<T> delegate,
      Widget::InitParams::Ownership ownership =
          Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET) {
    CHECK(IsBubbleDialogDelegateView<T>(delegate.get()));
    return BubbleDialogDelegate::CreateBubble(std::move(delegate), ownership);
  }
  static Widget* CreateBubble(
      BubbleDialogDelegateView* bubble_delegate,
      Widget::InitParams::Ownership ownership =
          Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);

  BubbleDialogDelegateView();
  // |shadow| usually doesn't need to be explicitly set, just uses the default
  // argument. Unless on Mac when the bubble needs to use Views base shadow,
  // override it with suitable bubble border type.
  BubbleDialogDelegateView(
      View* anchor_view,
      BubbleBorder::Arrow arrow,
      BubbleBorder::Shadow shadow = BubbleBorder::DIALOG_SHADOW,
      bool autosize = false);
  BubbleDialogDelegateView(const BubbleDialogDelegateView&) = delete;
  BubbleDialogDelegateView& operator=(const BubbleDialogDelegateView&) = delete;
  ~BubbleDialogDelegateView() override;

  // BubbleDialogDelegate:
  View* GetContentsView() override;

  // View:
  Widget* GetWidget() override;
  const Widget* GetWidget() const override;

 protected:
  // Disallow overrides of GetMinimumSize and GetMaximumSize(). These would only
  // be called by the FrameView, but the BubbleFrameView ignores these. Bubbles
  // are not user-sizable and always size to their preferred size (plus any
  // border / frame).
  // View:
  gfx::Size GetMinimumSize() const final;
  gfx::Size GetMaximumSize() const final;

 private:
  FRIEND_TEST_ALL_PREFIXES(BubbleDelegateTest, CreateDelegate);
  FRIEND_TEST_ALL_PREFIXES(BubbleDelegateTest, NonClientHitTest);
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, BubbleDialogDelegateView, View)
VIEW_BUILDER_PROPERTY(ax::mojom::Role, AccessibleWindowRole)
VIEW_BUILDER_PROPERTY(std::u16string, AccessibleTitle)
VIEW_BUILDER_PROPERTY(bool, CanMaximize)
VIEW_BUILDER_PROPERTY(bool, CanMinimize)
VIEW_BUILDER_PROPERTY(bool, CanResize)
VIEW_BUILDER_VIEW_TYPE_PROPERTY(views::View, ExtraView)
VIEW_BUILDER_VIEW_TYPE_PROPERTY(views::View, FootnoteView)
VIEW_BUILDER_PROPERTY(bool, FocusTraversesOut)
VIEW_BUILDER_PROPERTY(bool, EnableArrowKeyTraversal)
VIEW_BUILDER_PROPERTY(ui::ImageModel, Icon)
VIEW_BUILDER_PROPERTY(ui::ImageModel, AppIcon)
VIEW_BUILDER_PROPERTY(ui::ImageModel, MainImage)
VIEW_BUILDER_PROPERTY(ui::mojom::ModalType, ModalType)
VIEW_BUILDER_PROPERTY(bool, OwnedByWidget)
VIEW_BUILDER_PROPERTY(bool, ShowCloseButton)
VIEW_BUILDER_PROPERTY(bool, ShowIcon)
VIEW_BUILDER_PROPERTY(bool, ShowTitle)
VIEW_BUILDER_OVERLOAD_METHOD_CLASS(WidgetDelegate,
                                   SetTitle,
                                   const std::u16string&)
VIEW_BUILDER_OVERLOAD_METHOD_CLASS(WidgetDelegate, SetTitle, int)
#if defined(USE_AURA)
VIEW_BUILDER_PROPERTY(bool, CenterTitle)
#endif
VIEW_BUILDER_PROPERTY(int, Buttons)
VIEW_BUILDER_PROPERTY(int, DefaultButton)
VIEW_BUILDER_METHOD(SetButtonLabel, ui::mojom::DialogButton, std::u16string)
VIEW_BUILDER_METHOD(SetButtonEnabled, ui::mojom::DialogButton, bool)
VIEW_BUILDER_METHOD(set_margins, gfx::Insets)
VIEW_BUILDER_METHOD(set_use_round_corners, bool)
VIEW_BUILDER_METHOD(set_corner_radius, int)
VIEW_BUILDER_METHOD(set_draggable, bool)
VIEW_BUILDER_METHOD(set_use_custom_frame, bool)
VIEW_BUILDER_METHOD(set_fixed_width, int)
VIEW_BUILDER_METHOD(set_highlight_button_when_shown, bool)
VIEW_BUILDER_PROPERTY(base::OnceClosure, AcceptCallback)
VIEW_BUILDER_PROPERTY(base::OnceClosure, CancelCallback)
VIEW_BUILDER_PROPERTY(base::OnceClosure, CloseCallback)
VIEW_BUILDER_PROPERTY(const gfx::Insets&, ButtonRowInsets)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, BubbleDialogDelegateView)

#endif  // UI_VIEWS_BUBBLE_BUBBLE_DIALOG_DELEGATE_VIEW_H_
