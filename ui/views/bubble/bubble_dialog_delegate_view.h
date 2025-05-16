// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_DIALOG_DELEGATE_VIEW_H_
#define UI_VIEWS_BUBBLE_BUBBLE_DIALOG_DELEGATE_VIEW_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/base/class_property.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_utils.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/color/color_variant.h"
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

class AccountChooserDialogView;
class AppDialogView;
class AnnouncementView;
class BruschettaUninstallerView;
class ChromeLabsBubbleView;
class ColorPickerViewTest;
class ContentSettingBubbleContents;
class CriticalNotificationBubbleView;
class CrostiniAnsibleSoftwareConfigView;
class CrostiniExpiredContainerWarningView;
class CrostiniForceCloseView;
class CrostiniPackageInstallFailureView;
class CrostiniRecoveryView;
class CrostiniUninstallerView;
class CrostiniUpdateFilesystemView;
class DiceWebSigninInterceptionBubbleView;
class ExtensionInstallDialogView;
class ExtensionInstallFrictionDialogView;
class ExtensionInstalledBubbleView;
class ExtensionPopup;
class ExtensionsMenuView;
class FlyingIndicator;
class GlobalErrorBubbleView;
class HomePageUndoBubble;
class MediaDialogView;
class HatsNextWebDialog;
class IncognitoClearBrowsingDataDialog;
class LocationBarBubbleDelegateView;
class NetworkProfileBubbleView;
class PageInfoBubbleViewBase;
class PermissionPromptBaseView;
class PluginVmInstallerView;
class ProfileCustomizationBubbleView;
class ProfileMenuViewBase;
class RemoveSuggestionBubbleDialogDelegateView;
class StoragePressureBubbleView;
class TabGroupEditorBubbleView;
class TabHoverCardBubbleView;
class TestBubbleView;
class ToolbarActionHoverCardBubbleView;
class ToolbarActionsBarBubbleViews;
class ScreenshotSurfaceTestDialog;
class WebBubbleView;
class WebUIBubbleDialogView;
FORWARD_DECLARE_TEST(InProcessBrowserTest,
                     RunsScheduledLayoutOnAnchoredBubbles);

namespace ambient_signin {
class AmbientSigninBubbleView;
}

namespace arc {
class ArcSplashScreenDialogView;
class BaseDialogDelegateView;
class ResizeConfirmationDialogView;
class RoundedCornerBubbleDialogDelegateView;

namespace input_overlay {
class DeleteEditShortcut;
class RichNudge;
}  // namespace input_overlay
}  // namespace arc

namespace ash {
class AnchoredNudge;
class ContextualNudge;
class DictationBubbleView;
class FaceGazeBubbleView;
class GameDashboardMainMenuView;
class HelpBubbleViewAsh;
class ImeModeIndicatorView;
class KioskAppInstructionBubble;
class MouseKeysBubbleView;
class NetworkInfoBubble;
class NetworkStateListInfoBubble;
class PaletteWelcomeBubbleView;
class QuickInsertCapsLockStateView;
class QuickInsertPreviewBubbleView;
class ShelfBubble;
class TestBubbleDialogDelegateView;
class TestBubbleDialogDelegate;
class TrayBubbleView;
FORWARD_DECLARE_TEST(OverviewSessionTest, DoNotHideBubbleTransient);
FORWARD_DECLARE_TEST(ResizeShadowAndCursorTest,
                     DefaultCursorOnBubbleWidgetCorners);
FORWARD_DECLARE_TEST(SnapGroupOverviewTest, BubbleTransientIsVisibleInOverview);
FORWARD_DECLARE_TEST(
    SnapGroupDesksTest,
    NoCrashWhenDraggingOverviewGroupItemWithBubbleToAnotherDesk);
FORWARD_DECLARE_TEST(SnapGroupTest,
                     NoCrashWhenReSnappingSecondaryToPrimaryWithTransient);

namespace sharesheet {
class SharesheetBubbleView;
}
}  // namespace ash

namespace autofill {
class CardUnmaskAuthenticationSelectionDialogView;
class CardUnmaskPromptViews;
class LocalCardMigrationDialogView;
class LocalCardMigrationErrorDialogView;
}  // namespace autofill

namespace captions {
class CaptionBubble;
}

namespace chromeos {
class MultitaskMenu;
}

namespace gfx {
class Rect;
}

namespace lens {
class LensPreselectionBubble;
class LensRegionSearchInstructionsView;
}  // namespace lens

namespace media_router {
class CastDialogView;
class MediaRemotingDialogView;
}  // namespace media_router

namespace send_tab_to_self {
class SendTabToSelfToolbarBubbleView;
}

namespace toasts {
class ToastView;
}

namespace ui::ime {
class AnnouncementView;
class CandidateWindowView;
class GrammarSuggestionWindow;
class InfolistWindow;
class SuggestionWindowView;
class UndoWindow;
}  // namespace ui::ime

namespace user_education {
class HelpBubbleView;

namespace test {
class TestCustomHelpBubbleView;
}
}  // namespace user_education

namespace webid {
class AccountSelectionBubbleView;
}

namespace views {

class AnchorTestBubbleDialogDelegateView;
class Button;
class FocusManagerTestBubbleDialogDelegateView;
class FrameViewTestBubbleDialogDelegateView;
class InfoBubble;
class InteractionSequenceViewsTest;
class TestBubbleDialogDelegateView;
class TestBubbleView;
class TouchSelectionMenuViews;

namespace examples {
template <class DialogType>
class DialogExampleDelegate;
class ExampleBubble;
class LoginBubbleDialogView;
}  // namespace examples

namespace test {
class SimpleBubbleView;
class TestBubbleView;
class WidgetTestBubbleDialogDelegateView;
FORWARD_DECLARE_TEST(DesktopWidgetTestInteractive, FocusChangesOnBubble);
FORWARD_DECLARE_TEST(InteractionTestUtilViewsTest, ActivateSurface);
FORWARD_DECLARE_TEST(InteractionTestUtilViewsTest, Confirm);
}  // namespace test

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
  // The anchor widget can be explicitly set, or is implied by the anchor view.
  void SetAnchorWidget(views::Widget* anchor_widget);
  Widget* anchor_widget() { return anchor_widget_; }
  const Widget* anchor_widget() const { return anchor_widget_; }

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

  ui::ColorVariant background_color() const { return color_; }
  void SetBackgroundColor(ui::ColorVariant color);

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
    void LogMetric(void (*uma_func)(std::string_view, Value),
                   std::string_view histogram_name,
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
  void UpdateFrameColor();

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
  ui::ColorVariant color_ = ui::kColorBubbleBackground;
  raw_ptr<Widget> anchor_widget_ = nullptr;
  std::unique_ptr<AnchorViewObserver> anchor_view_observer_;
  std::unique_ptr<AnchorWidgetObserver> anchor_widget_observer_;
  std::unique_ptr<BubbleWidgetObserver> bubble_widget_observer_;
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

  // If true, contents view will be forced to create a solid color background in
  // `UpdateFrameColor()`.
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
//
// DEPRECATED: Using this class makes it more challenging to reason about object
// ownership/lifetimes and promotes writing "fat" views that also contain
// business logic. Instead, use DialogModel if possible; otherwise, use separate
// subclasses of BubbleDialogDelegate and View to handle those interfaces'
// respective concerns.
//
// TODO(pbos): Migrate existing uses of BubbleDialogDelegateView to directly
// inherit or use BubbleDialogDelegate.
class VIEWS_EXPORT BubbleDialogDelegateView : public View,
                                              public BubbleDialogDelegate {
  METADATA_HEADER(BubbleDialogDelegateView, View)

 public:
  // Not named `PassKey` as `View::PassKey` already exists in this hierarchy.
  using BddvPassKey = base::PassKey<BubbleDialogDelegateView>;

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

  // For use with std::make_unique<>(). Callers still must be in the friend list
  // below, just as with the private constructor.
  explicit BubbleDialogDelegateView(
      BddvPassKey,
      View* anchor_view = nullptr,
      BubbleBorder::Arrow arrow = views::BubbleBorder::TOP_LEFT,
      BubbleBorder::Shadow shadow = BubbleBorder::DIALOG_SHADOW,
      bool autosize = false)
      : BubbleDialogDelegateView(anchor_view, arrow, shadow, autosize) {}

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

  // DO NOT ADD TO THIS LIST!
  // These existing cases are "grandfathered in", but there shouldn't be more.
  // See comments atop class.
  friend class ::AccountChooserDialogView;
  friend class ::AnnouncementView;
  friend class ::AppDialogView;
  friend class ::BruschettaUninstallerView;
  friend class ::ChromeLabsBubbleView;
  friend class ::ColorPickerViewTest;
  friend class ::ContentSettingBubbleContents;
  friend class ::CriticalNotificationBubbleView;
  friend class ::CrostiniAnsibleSoftwareConfigView;
  friend class ::CrostiniExpiredContainerWarningView;
  friend class ::CrostiniForceCloseView;
  friend class ::CrostiniPackageInstallFailureView;
  friend class ::CrostiniRecoveryView;
  friend class ::CrostiniUninstallerView;
  friend class ::CrostiniUpdateFilesystemView;
  friend class ::DiceWebSigninInterceptionBubbleView;
  friend class ::ExtensionInstallDialogView;
  friend class ::ExtensionInstallFrictionDialogView;
  friend class ::ExtensionInstalledBubbleView;
  friend class ::ExtensionPopup;
  friend class ::ExtensionsMenuView;
  friend class ::FlyingIndicator;
  friend class ::GlobalErrorBubbleView;
  friend class ::HomePageUndoBubble;
  friend class ::MediaDialogView;
  friend class ::HatsNextWebDialog;
  friend class ::IncognitoClearBrowsingDataDialog;
  friend class ::LocationBarBubbleDelegateView;
  friend class ::NetworkProfileBubbleView;
  friend class ::PageInfoBubbleViewBase;
  friend class ::PermissionPromptBaseView;
  friend class ::PluginVmInstallerView;
  friend class ::ProfileCustomizationBubbleView;
  friend class ::ProfileMenuViewBase;
  friend class ::RemoveSuggestionBubbleDialogDelegateView;
  friend class ::StoragePressureBubbleView;
  friend class ::TabGroupEditorBubbleView;
  friend class ::TabHoverCardBubbleView;
  friend class ::TestBubbleView;
  friend class ::ToolbarActionHoverCardBubbleView;
  friend class ::ToolbarActionsBarBubbleViews;
  friend class ::ScreenshotSurfaceTestDialog;
  friend class ::WebBubbleView;
  friend class ::WebUIBubbleDialogView;
  FRIEND_TEST_ALL_PREFIXES(::InProcessBrowserTest,
                           RunsScheduledLayoutOnAnchoredBubbles);
  friend class ::ambient_signin::AmbientSigninBubbleView;
  friend class ::arc::ArcSplashScreenDialogView;
  friend class ::arc::BaseDialogDelegateView;
  friend class ::arc::ResizeConfirmationDialogView;
  friend class ::arc::RoundedCornerBubbleDialogDelegateView;
  friend class ::arc::input_overlay::DeleteEditShortcut;
  friend class ::arc::input_overlay::RichNudge;
  friend class ::ash::AnchoredNudge;
  friend class ::ash::ContextualNudge;
  friend class ::ash::DictationBubbleView;
  friend class ::ash::FaceGazeBubbleView;
  friend class ::ash::GameDashboardMainMenuView;
  friend class ::ash::HelpBubbleViewAsh;
  friend class ::ash::ImeModeIndicatorView;
  friend class ::ash::KioskAppInstructionBubble;
  friend class ::ash::MouseKeysBubbleView;
  friend class ::ash::NetworkInfoBubble;
  friend class ::ash::NetworkStateListInfoBubble;
  friend class ::ash::PaletteWelcomeBubbleView;
  friend class ::ash::QuickInsertCapsLockStateView;
  friend class ::ash::QuickInsertPreviewBubbleView;
  friend class ::ash::ShelfBubble;
  friend class ::ash::TestBubbleDialogDelegateView;
  friend class ::ash::TestBubbleDialogDelegate;
  friend class ::ash::TrayBubbleView;
  FRIEND_TEST_ALL_PREFIXES(::ash::OverviewSessionTest,
                           DoNotHideBubbleTransient);
  FRIEND_TEST_ALL_PREFIXES(::ash::ResizeShadowAndCursorTest,
                           DefaultCursorOnBubbleWidgetCorners);
  FRIEND_TEST_ALL_PREFIXES(::ash::SnapGroupOverviewTest,
                           BubbleTransientIsVisibleInOverview);
  FRIEND_TEST_ALL_PREFIXES(
      ::ash::SnapGroupDesksTest,
      NoCrashWhenDraggingOverviewGroupItemWithBubbleToAnotherDesk);
  FRIEND_TEST_ALL_PREFIXES(
      ::ash::SnapGroupTest,
      NoCrashWhenReSnappingSecondaryToPrimaryWithTransient);
  friend class ::ash::sharesheet::SharesheetBubbleView;
  friend class ::autofill::CardUnmaskAuthenticationSelectionDialogView;
  friend class ::autofill::CardUnmaskPromptViews;
  friend class ::autofill::LocalCardMigrationDialogView;
  friend class ::autofill::LocalCardMigrationErrorDialogView;
  friend class ::captions::CaptionBubble;
  friend class ::chromeos::MultitaskMenu;
  friend class ::lens::LensPreselectionBubble;
  friend class ::lens::LensRegionSearchInstructionsView;
  friend class ::media_router::CastDialogView;
  friend class ::media_router::MediaRemotingDialogView;
  friend class ::send_tab_to_self::SendTabToSelfToolbarBubbleView;
  friend class ::toasts::ToastView;
  friend class ::ui::ime::AnnouncementView;
  friend class ::ui::ime::CandidateWindowView;
  friend class ::ui::ime::GrammarSuggestionWindow;
  friend class ::ui::ime::InfolistWindow;
  friend class ::ui::ime::SuggestionWindowView;
  friend class ::ui::ime::UndoWindow;
  friend class ::user_education::HelpBubbleView;
  friend class ::user_education::test::TestCustomHelpBubbleView;
  friend class ::webid::AccountSelectionBubbleView;
  friend class AnchorTestBubbleDialogDelegateView;
  friend class FocusManagerTestBubbleDialogDelegateView;
  friend class FrameViewTestBubbleDialogDelegateView;
  friend class InfoBubble;
  friend class InteractionSequenceViewsTest;
  friend class TestBubbleDialogDelegateView;
  friend class TestBubbleView;
  friend class TouchSelectionMenuViews;
  FRIEND_TEST_ALL_PREFIXES(BubbleDialogDelegateViewInteractiveTest,
                           BubbleAndParentNotActiveSimultaneously);
  FRIEND_TEST_ALL_PREFIXES(BubbleDialogDelegateViewTest,
                           ClientViewIsPaintedToLayer);
  FRIEND_TEST_ALL_PREFIXES(WidgetFocusObserverTest, Bubble);
  friend class examples::DialogExampleDelegate<BubbleDialogDelegateView>;
  friend class examples::ExampleBubble;
  friend class examples::LoginBubbleDialogView;
  friend class test::SimpleBubbleView;
  friend class test::TestBubbleView;
  friend class test::WidgetTestBubbleDialogDelegateView;
  FRIEND_TEST_ALL_PREFIXES(test::DesktopWidgetTestInteractive,
                           FocusChangesOnBubble);
  FRIEND_TEST_ALL_PREFIXES(test::InteractionTestUtilViewsTest, ActivateSurface);
  FRIEND_TEST_ALL_PREFIXES(test::InteractionTestUtilViewsTest, Confirm);

  // |shadow| usually doesn't need to be explicitly set, just uses the default
  // argument. Unless on Mac when the bubble needs to use Views base shadow,
  // override it with suitable bubble border type.
  explicit BubbleDialogDelegateView(
      View* anchor_view = nullptr,
      BubbleBorder::Arrow arrow = views::BubbleBorder::TOP_LEFT,
      BubbleBorder::Shadow shadow = BubbleBorder::DIALOG_SHADOW,
      bool autosize = false);

  static BddvPassKey CreatePassKey() { return BddvPassKey(); }
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
