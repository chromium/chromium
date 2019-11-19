// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_dialog_delegate_view.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/default_style.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_client_view.h"

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#endif

#if defined(OS_MACOSX)
#include "ui/views/widget/widget_utils_mac.h"
#endif

namespace views {

// static
bool BubbleDialogDelegateView::devtools_dismiss_override_ = false;

namespace {

// Override base functionality of Widget to give bubble dialogs access to the
// theme provider of the window they're anchored to.
class BubbleWidget : public Widget {
 public:
  BubbleWidget() = default;

  // Widget:
  const ui::ThemeProvider* GetThemeProvider() const override {
    BubbleDialogDelegateView* const bubble_delegate =
        static_cast<BubbleDialogDelegateView*>(widget_delegate());
    if (!bubble_delegate || !bubble_delegate->anchor_widget())
      return Widget::GetThemeProvider();
    return bubble_delegate->anchor_widget()->GetThemeProvider();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BubbleWidget);
};

// The frame view for bubble dialog widgets. These are not user-sizable so have
// simplified logic for minimum and maximum sizes to avoid repeated calls to
// CalculatePreferredSize().
class BubbleDialogFrameView : public BubbleFrameView {
 public:
  explicit BubbleDialogFrameView(const gfx::Insets& title_margins)
      : BubbleFrameView(title_margins, gfx::Insets()) {}

  // View:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }
  gfx::Size GetMaximumSize() const override { return gfx::Size(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(BubbleDialogFrameView);
};

bool CustomShadowsSupported() {
#if defined(OS_WIN)
  return ui::win::IsAeroGlassEnabled();
#else
  return true;
#endif
}

// Create a widget to host the bubble.
Widget* CreateBubbleWidget(BubbleDialogDelegateView* bubble) {
  Widget* bubble_widget = new BubbleWidget();
  Widget::InitParams bubble_params(Widget::InitParams::TYPE_BUBBLE);
  bubble_params.delegate = bubble;
  bubble_params.opacity = CustomShadowsSupported()
                              ? Widget::InitParams::TRANSLUCENT_WINDOW
                              : Widget::InitParams::OPAQUE_WINDOW;
  bubble_params.accept_events = bubble->accept_events();
  bubble_params.remove_standard_frame = true;
  // Use a window default shadow if the bubble doesn't provides its own.
  if (bubble->GetShadow() == BubbleBorder::NO_ASSETS)
    bubble_params.shadow_type = Widget::InitParams::ShadowType::kDefault;
  else if (CustomShadowsSupported())
    bubble_params.shadow_type = Widget::InitParams::ShadowType::kNone;
  else
    bubble_params.shadow_type = Widget::InitParams::ShadowType::kDrop;
  if (bubble->parent_window())
    bubble_params.parent = bubble->parent_window();
  else if (bubble->anchor_widget())
    bubble_params.parent = bubble->anchor_widget()->GetNativeView();
  bubble_params.activatable = bubble->CanActivate()
                                  ? Widget::InitParams::ACTIVATABLE_YES
                                  : Widget::InitParams::ACTIVATABLE_NO;
  bubble->OnBeforeBubbleWidgetInit(&bubble_params, bubble_widget);
  bubble_widget->Init(std::move(bubble_params));
#if !defined(OS_MACOSX)
  // On Mac, having a parent window creates a permanent stacking order, so
  // there's no need to do this. Also, calling StackAbove() on Mac shows the
  // bubble implicitly, for which the bubble is currently not ready.
  if (bubble_params.parent)
    bubble_widget->StackAbove(bubble_params.parent);
#endif
  return bubble_widget;
}

}  // namespace

BubbleDialogDelegateView::~BubbleDialogDelegateView() {
  if (GetWidget())
    GetWidget()->RemoveObserver(this);
  SetLayoutManager(nullptr);
  SetAnchorView(nullptr);
}

// static
Widget* BubbleDialogDelegateView::CreateBubble(
    BubbleDialogDelegateView* bubble_delegate) {
  bubble_delegate->Init();
  // Get the latest anchor widget from the anchor view at bubble creation time.
  bubble_delegate->SetAnchorView(bubble_delegate->GetAnchorView());
  Widget* bubble_widget = CreateBubbleWidget(bubble_delegate);

#if (defined(OS_LINUX) && !defined(OS_CHROMEOS)) || defined(OS_MACOSX)
  // Linux clips bubble windows that extend outside their parent window bounds.
  // Mac never adjusts.
  bubble_delegate->set_adjust_if_offscreen(false);
#endif

  bubble_delegate->SizeToContents();
  bubble_widget->AddObserver(bubble_delegate);
  return bubble_widget;
}

BubbleDialogDelegateView* BubbleDialogDelegateView::AsBubbleDialogDelegate() {
  return this;
}

bool BubbleDialogDelegateView::ShouldShowCloseButton() const {
  return false;
}

NonClientFrameView* BubbleDialogDelegateView::CreateNonClientFrameView(
    Widget* widget) {
  BubbleFrameView* frame = new BubbleDialogFrameView(title_margins_);
  LayoutProvider* provider = LayoutProvider::Get();

  frame->set_footnote_margins(
      provider->GetInsetsMetric(INSETS_DIALOG_SUBSECTION));
  frame->SetFootnoteView(CreateFootnoteView());

  std::unique_ptr<BubbleBorder> border =
      std::make_unique<BubbleBorder>(arrow(), GetShadow(), color());
  if (CustomShadowsSupported() && GetParams().round_corners) {
    border->SetCornerRadius(
        base::FeatureList::IsEnabled(features::kEnableMDRoundedCornersOnDialogs)
            ? provider->GetCornerRadiusMetric(views::EMPHASIS_HIGH)
            : 2);
  }

  frame->SetBubbleBorder(std::move(border));
  return frame;
}

bool BubbleDialogDelegateView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_DOWN ||
      accelerator.key_code() == ui::VKEY_UP) {
    // Move the focus up or down.
    GetFocusManager()->AdvanceFocus(accelerator.key_code() != ui::VKEY_DOWN);
    return true;
  }
  return DialogDelegateView::AcceleratorPressed(accelerator);
}

void BubbleDialogDelegateView::OnWidgetClosing(Widget* widget) {
  // To prevent keyboard focus traversal issues, the anchor view's
  // kAnchoredDialogKey property is cleared immediately upon Close(). This
  // avoids a bug that occured when a focused anchor view is made unfocusable
  // right after the bubble is closed. Previously, focus would advance into the
  // bubble then would be lost when the bubble was destroyed.
  if (widget == GetWidget() && GetAnchorView())
    GetAnchorView()->ClearProperty(kAnchoredDialogKey);
}

void BubbleDialogDelegateView::OnWidgetDestroying(Widget* widget) {
  if (anchor_widget() == widget)
    SetAnchorView(nullptr);
}

void BubbleDialogDelegateView::OnWidgetVisibilityChanging(Widget* widget,
                                                          bool visible) {
#if defined(OS_WIN)
  // On Windows we need to handle this before the bubble is visible or hidden.
  // Please see the comment on the OnWidgetVisibilityChanging function. On
  // other platforms it is fine to handle it after the bubble is shown/hidden.
  HandleVisibilityChanged(widget, visible);
#endif
}

void BubbleDialogDelegateView::OnWidgetVisibilityChanged(Widget* widget,
                                                         bool visible) {
#if !defined(OS_WIN)
  HandleVisibilityChanged(widget, visible);
#endif
}

void BubbleDialogDelegateView::OnWidgetActivationChanged(Widget* widget,
                                                         bool active) {
  if (devtools_dismiss_override_)
    return;

#if defined(OS_MACOSX)
  // Install |mac_bubble_closer_| the first time the widget becomes active.
  if (widget == GetWidget() && active && !mac_bubble_closer_) {
    mac_bubble_closer_ = std::make_unique<ui::BubbleCloser>(
        GetWidget()->GetNativeWindow().GetNativeNSWindow(),
        base::BindRepeating(&BubbleDialogDelegateView::OnDeactivate,
                            base::Unretained(this)));
  }
#endif
  if (widget == GetWidget() && !active)
    OnDeactivate();
}

void BubbleDialogDelegateView::OnWidgetBoundsChanged(
    Widget* widget,
    const gfx::Rect& new_bounds) {
  if (GetBubbleFrameView() && anchor_widget() == widget)
    SizeToContents();
}

BubbleBorder::Shadow BubbleDialogDelegateView::GetShadow() const {
  if (CustomShadowsSupported() || shadow_ == BubbleBorder::NO_ASSETS)
    return shadow_;
  return BubbleBorder::NO_SHADOW;
}

View* BubbleDialogDelegateView::GetAnchorView() const {
  return anchor_view_tracker_->view();
}

void BubbleDialogDelegateView::SetHighlightedButton(
    Button* highlighted_button) {
  bool visible = GetWidget() && GetWidget()->IsVisible();
  // If the Widget is visible, ensure the old highlight (if any) is removed
  // when the highlighted view changes.
  if (visible && highlighted_button != highlighted_button_tracker_.view())
    UpdateHighlightedButton(false);
  highlighted_button_tracker_.SetView(highlighted_button);
  if (visible)
    UpdateHighlightedButton(true);
}

void BubbleDialogDelegateView::SetArrow(BubbleBorder::Arrow arrow) {
  SetArrowWithoutResizing(arrow);
  // If SetArrow() is called before CreateWidget(), there's no need to update
  // the BubbleFrameView.
  if (GetBubbleFrameView())
    SizeToContents();
}

void BubbleDialogDelegateView::SetArrowWithoutResizing(
    BubbleBorder::Arrow arrow) {
  if (base::i18n::IsRTL())
    arrow = BubbleBorder::horizontal_mirror(arrow);
  if (arrow_ == arrow)
    return;
  arrow_ = arrow;

  // If SetArrow() is called before CreateWidget(), there's no need to update
  // the BubbleFrameView.
  if (GetBubbleFrameView())
    GetBubbleFrameView()->SetArrow(arrow);
}

gfx::Rect BubbleDialogDelegateView::GetAnchorRect() const {
  if (!GetAnchorView())
    return anchor_rect_;

  anchor_rect_ = GetAnchorView()->GetAnchorBoundsInScreen();
  anchor_rect_.Inset(anchor_view_insets_);
  return anchor_rect_;
}

void BubbleDialogDelegateView::OnBeforeBubbleWidgetInit(
    Widget::InitParams* params,
    Widget* widget) const {}

void BubbleDialogDelegateView::UseCompactMargins() {
  set_margins(gfx::Insets(6));
}

void BubbleDialogDelegateView::OnAnchorBoundsChanged() {
  SizeToContents();
}

BubbleDialogDelegateView::BubbleDialogDelegateView()
    : BubbleDialogDelegateView(nullptr, BubbleBorder::TOP_LEFT) {}

BubbleDialogDelegateView::BubbleDialogDelegateView(View* anchor_view,
                                                   BubbleBorder::Arrow arrow,
                                                   BubbleBorder::Shadow shadow)
    : close_on_deactivate_(true),
      anchor_view_tracker_(std::make_unique<ViewTracker>()),
      anchor_widget_(nullptr),
      shadow_(shadow),
      color_explicitly_set_(false),
      accept_events_(true),
      adjust_if_offscreen_(true),
      parent_window_(nullptr) {
  SetArrow(arrow);
  LayoutProvider* provider = LayoutProvider::Get();
  // An individual bubble should override these margins if its layout differs
  // from the typical title/text/buttons.
  set_margins(provider->GetDialogInsetsForContentType(TEXT, TEXT));
  title_margins_ = provider->GetInsetsMetric(INSETS_DIALOG_TITLE);
  if (anchor_view)
    SetAnchorView(anchor_view);
  UpdateColorsFromTheme();
  UMA_HISTOGRAM_BOOLEAN("Dialog.BubbleDialogDelegateView.Create", true);
}

gfx::Rect BubbleDialogDelegateView::GetBubbleBounds() {
  // The argument rect has its origin at the bubble's arrow anchor point;
  // its size is the preferred size of the bubble's client view (this view).
  bool anchor_minimized = anchor_widget() && anchor_widget()->IsMinimized();
  // If GetAnchorView() returns nullptr or GetAnchorRect() returns an empty rect
  // at (0, 0), don't try and adjust arrow if off-screen.
  gfx::Rect anchor_rect = GetAnchorRect();
  bool has_anchor = GetAnchorView() || anchor_rect != gfx::Rect();
  return GetBubbleFrameView()->GetUpdatedWindowBounds(
      anchor_rect, arrow(), GetWidget()->client_view()->GetPreferredSize(),
      adjust_if_offscreen_ && !anchor_minimized && has_anchor);
}

ax::mojom::Role BubbleDialogDelegateView::GetAccessibleWindowRole() {
  // If something in the dialog has initial focus, use the dialog role.
  // Screen readers understand what to announce when focus moves within one.
  if (GetInitiallyFocusedView())
    return ax::mojom::Role::kDialog;

  // Otherwise, return |ax::mojom::Role::kAlertDialog| which will make screen
  // readers announce the contents of the bubble dialog as soon as it appears,
  // as long as we also fire |ax::mojom::Event::kAlert|.
  return ax::mojom::Role::kAlertDialog;
}

void BubbleDialogDelegateView::OnPaintAsActiveChanged(bool paint_as_active) {
  if (!paint_as_active) {
    paint_as_active_lock_.reset();
    return;
  }

  if (!anchor_widget() || !anchor_widget()->GetTopLevelWidget())
    return;

  // When this bubble renders as active, its anchor widget should also render as
  // active.
  paint_as_active_lock_ =
      anchor_widget()->GetTopLevelWidget()->LockPaintAsActive();
}

gfx::Size BubbleDialogDelegateView::GetMinimumSize() const {
  // Note that although BubbleDialogFrameView will never invoke this, a subclass
  // may override CreateNonClientFrameView() to provide a NonClientFrameView
  // that does. See http://crbug.com/844359.
  return gfx::Size();
}

gfx::Size BubbleDialogDelegateView::GetMaximumSize() const {
  return gfx::Size();
}

void BubbleDialogDelegateView::OnThemeChanged() {
  UpdateColorsFromTheme();
}

void BubbleDialogDelegateView::Init() {}

void BubbleDialogDelegateView::SetAnchorView(View* anchor_view) {
  if (GetAnchorView())
    GetAnchorView()->ClearProperty(kAnchoredDialogKey);

  // When the anchor view gets set the associated anchor widget might
  // change as well.
  if (!anchor_view || anchor_widget() != anchor_view->GetWidget()) {
    if (anchor_widget()) {
      if (GetWidget() && GetWidget()->IsVisible())
        UpdateHighlightedButton(false);
      paint_as_active_lock_.reset();
      anchor_widget_->RemoveObserver(this);
      anchor_widget_ = nullptr;
    }
    if (anchor_view) {
      anchor_widget_ = anchor_view->GetWidget();
      if (anchor_widget_) {
        anchor_widget_->AddObserver(this);
        const bool visible = GetWidget() && GetWidget()->IsVisible();
        UpdateHighlightedButton(visible);
        // Have the anchor widget's paint-as-active state track this view's
        // widget - lock is only required if the bubble widget is active.
        if (anchor_widget_->GetTopLevelWidget() && GetWidget() &&
            GetWidget()->ShouldPaintAsActive()) {
          paint_as_active_lock_ =
              anchor_widget_->GetTopLevelWidget()->LockPaintAsActive();
        }
      }
    }
  }

  anchor_view_tracker_->SetView(anchor_view);

  if (anchor_view && GetWidget()) {
    // Do not update anchoring for NULL views; this could indicate
    // that our NativeWindow is being destroyed, so it would be
    // dangerous for us to update our anchor bounds at that
    // point. (It's safe to skip this, since if we were to update the
    // bounds when |anchor_view| is NULL, the bubble won't move.)
    OnAnchorBoundsChanged();
  }

  if (anchor_view && focus_traversable_from_anchor_view_) {
    // Make sure that focus can move into here from the anchor view (but not
    // out, focus will cycle inside the dialog once it gets here).
    anchor_view->SetProperty(kAnchoredDialogKey, this);
  }
}

void BubbleDialogDelegateView::SetAnchorRect(const gfx::Rect& rect) {
  anchor_rect_ = rect;
  if (GetWidget())
    OnAnchorBoundsChanged();
}

void BubbleDialogDelegateView::SizeToContents() {
  gfx::Rect bubble_bounds = GetBubbleBounds();
#if defined(OS_MACOSX)
  // GetBubbleBounds() doesn't take the Mac NativeWindow's style mask into
  // account, so we need to adjust the size.
  gfx::Size actual_size =
      GetWindowSizeForClientSize(GetWidget(), bubble_bounds.size());
  bubble_bounds.set_size(actual_size);
#endif

  GetWidget()->SetBounds(bubble_bounds);
}

void BubbleDialogDelegateView::UpdateColorsFromTheme() {
  if (!color_explicitly_set_)
    color_ = GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_BubbleBackground);
  BubbleFrameView* frame_view = GetBubbleFrameView();
  if (frame_view)
    frame_view->SetBackgroundColor(color());

  // When there's an opaque layer, the bubble border background won't show
  // through, so explicitly paint a background color.
  SetBackground(layer() && layer()->fills_bounds_opaquely()
                    ? CreateSolidBackground(color())
                    : nullptr);
}

void BubbleDialogDelegateView::EnableUpDownKeyboardAccelerators() {
  // The arrow keys can be used to tab between items.
  AddAccelerator(ui::Accelerator(ui::VKEY_DOWN, ui::EF_NONE));
  AddAccelerator(ui::Accelerator(ui::VKEY_UP, ui::EF_NONE));
}

void BubbleDialogDelegateView::HandleVisibilityChanged(Widget* widget,
                                                       bool visible) {
  if (widget == GetWidget())
    UpdateHighlightedButton(visible);

  // Fire ax::mojom::Event::kAlert for bubbles marked as
  // ax::mojom::Role::kAlertDialog; this instructs accessibility tools to read
  // the bubble in its entirety rather than just its title and initially focused
  // view.  See http://crbug.com/474622 for details.
  if (widget == GetWidget() && visible) {
    if (GetAccessibleWindowRole() == ax::mojom::Role::kAlert ||
        GetAccessibleWindowRole() == ax::mojom::Role::kAlertDialog) {
      widget->GetRootView()->NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                                                      true);
    }
  }
}

void BubbleDialogDelegateView::OnDeactivate() {
  if (close_on_deactivate() && GetWidget())
    GetWidget()->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
}

void BubbleDialogDelegateView::UpdateHighlightedButton(bool highlighted) {
  Button* button = Button::AsButton(highlighted_button_tracker_.view());
  button = button ? button : Button::AsButton(anchor_view_tracker_->view());
  if (button && highlight_button_when_shown_)
    button->SetHighlighted(highlighted);
}

BEGIN_METADATA(BubbleDialogDelegateView)
METADATA_PARENT_CLASS(DialogDelegateView)
END_METADATA()

}  // namespace views
