// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_dialog_delegate_view.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/base/default_style.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#endif

#if defined(OS_APPLE)
#include "ui/views/widget/widget_utils_mac.h"
#else
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#endif

namespace views {

// static
bool BubbleDialogDelegate::devtools_dismiss_override_ = false;

namespace {

// A BubbleFrameView will apply a masking path to its ClientView to ensure
// contents are appropriately clipped to the frame's rounded corners. If the
// bubble uses layers in its views hierarchy, these will not be clipped to
// the client mask unless the ClientView is backed by a textured ui::Layer.
// This flag tracks whether or not to to create a layer backed ClientView.
//
// TODO(tluk): Fix all cases where bubble transparency is used and have bubble
// ClientViews always paint to a layer.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kPaintClientToLayer, true)

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
Widget* CreateBubbleWidget(BubbleDialogDelegate* bubble) {
  Widget* bubble_widget = new BubbleWidget();
  Widget::InitParams bubble_params(Widget::InitParams::TYPE_BUBBLE);
  bubble_params.delegate = bubble;
  bubble_params.opacity = CustomShadowsSupported()
                              ? Widget::InitParams::WindowOpacity::kTranslucent
                              : Widget::InitParams::WindowOpacity::kOpaque;
  bubble_params.accept_events = bubble->accept_events();
  bubble_params.remove_standard_frame = true;
  bubble_params.layer_type = bubble->GetLayerType();

  // Use a window default shadow if the bubble doesn't provides its own.
  if (bubble->GetShadow() == BubbleBorder::NO_ASSETS)
    bubble_params.shadow_type = Widget::InitParams::ShadowType::kDefault;
  else if (CustomShadowsSupported())
    bubble_params.shadow_type = Widget::InitParams::ShadowType::kNone;
  else
    bubble_params.shadow_type = Widget::InitParams::ShadowType::kDrop;
  if (bubble->parent_window()) {
    bubble_params.parent = bubble->parent_window();
  } else if (bubble->anchor_widget()) {
    bubble_params.parent = bubble->anchor_widget()->GetNativeView();
  }
  bubble_params.activatable = bubble->CanActivate()
                                  ? Widget::InitParams::ACTIVATABLE_YES
                                  : Widget::InitParams::ACTIVATABLE_NO;
  bubble->OnBeforeBubbleWidgetInit(&bubble_params, bubble_widget);
  DCHECK(bubble_params.parent);
  bubble_widget->Init(std::move(bubble_params));
#if !defined(OS_APPLE)
  // On Mac, having a parent window creates a permanent stacking order, so
  // there's no need to do this. Also, calling StackAbove() on Mac shows the
  // bubble implicitly, for which the bubble is currently not ready.
  if (bubble_params.parent)
    bubble_widget->StackAbove(bubble_params.parent);
#endif
  return bubble_widget;
}

}  // namespace

class BubbleDialogDelegate::AnchorViewObserver : public ViewObserver {
 public:
  AnchorViewObserver(BubbleDialogDelegate* parent, View* anchor_view)
      : parent_(parent), anchor_view_(anchor_view) {
    anchor_view_->AddObserver(this);
  }

  AnchorViewObserver(const AnchorViewObserver&) = delete;
  AnchorViewObserver& operator=(const AnchorViewObserver&) = delete;

  ~AnchorViewObserver() override { anchor_view_->RemoveObserver(this); }

  View* anchor_view() const { return anchor_view_; }

  // ViewObserver:
  void OnViewIsDeleting(View* observed_view) override {
    // The anchor is being deleted, make sure the parent bubble no longer
    // observes it.
    DCHECK_EQ(anchor_view_, observed_view);
    parent_->SetAnchorView(nullptr);
  }

  void OnViewBoundsChanged(View* observed_view) override {
    // This code really wants to know the anchor bounds in screen coordinates
    // have changed. There isn't a good way to detect this outside of the view.
    // Observing View bounds changing catches some cases but not all of them.
    DCHECK_EQ(anchor_view_, observed_view);
    parent_->OnAnchorBoundsChanged();
  }

  // TODO(pbos): Consider observing View visibility changes and only updating
  // view bounds when the anchor is visible.

 private:
  BubbleDialogDelegate* const parent_;
  View* const anchor_view_;
};

// This class is responsible for observing events on a BubbleDialogDelegate's
// anchor widget and notifying the BubbleDialogDelegate of them.
#if defined(OS_APPLE)
class BubbleDialogDelegate::AnchorWidgetObserver : public WidgetObserver {
#else
class BubbleDialogDelegate::AnchorWidgetObserver : public WidgetObserver,
                                                   public aura::WindowObserver {
#endif

 public:
  AnchorWidgetObserver(BubbleDialogDelegate* owner, Widget* widget)
      : owner_(owner) {
    widget_observer_.Add(widget);
#if !defined(OS_APPLE)
    window_observer_.Add(widget->GetNativeWindow());
#endif
  }
  ~AnchorWidgetObserver() override = default;

  // WidgetObserver:
  void OnWidgetDestroying(Widget* widget) override {
#if !defined(OS_APPLE)
    window_observer_.Remove(widget->GetNativeWindow());
#endif
    widget_observer_.Remove(widget);
    owner_->OnAnchorWidgetDestroying();
    // |this| may be destroyed here!
  }

  void OnWidgetActivationChanged(Widget* widget, bool active) override {
    owner_->OnWidgetActivationChanged(widget, active);
  }

  void OnWidgetBoundsChanged(Widget* widget, const gfx::Rect&) override {
    owner_->OnAnchorBoundsChanged();
  }

#if !defined(OS_APPLE)
  // aura::WindowObserver:
  void OnWindowTransformed(aura::Window* window,
                           ui::PropertyChangeReason reason) override {
    if (window->is_destroying())
      return;

    // Update the anchor bounds when the transform animation is complete, or
    // when the transform is set without animation.
    if (!window->layer()->GetAnimator()->IsAnimatingOnePropertyOf(
            ui::LayerAnimationElement::TRANSFORM)) {
      owner_->OnAnchorBoundsChanged();
    }
  }
#endif

 private:
  BubbleDialogDelegate* owner_;
  ScopedObserver<views::Widget, views::WidgetObserver> widget_observer_{this};
#if !defined(OS_APPLE)
  ScopedObserver<aura::Window, aura::WindowObserver> window_observer_{this};
#endif
};

// This class is responsible for observing events on a BubbleDialogDelegate's
// widget and notifying the BubbleDialogDelegate of them.
class BubbleDialogDelegate::BubbleWidgetObserver : public WidgetObserver {
 public:
  BubbleWidgetObserver(BubbleDialogDelegate* owner, Widget* widget)
      : owner_(owner) {
    observer_.Add(widget);
  }
  ~BubbleWidgetObserver() override = default;

  void OnWidgetClosing(Widget* widget) override {
    owner_->OnBubbleWidgetClosing();
    owner_->OnWidgetClosing(widget);
  }

  void OnWidgetDestroying(Widget* widget) override {
    owner_->OnWidgetDestroying(widget);
  }

  void OnWidgetDestroyed(Widget* widget) override {
    observer_.Remove(widget);
    owner_->OnWidgetDestroyed(widget);
  }

  void OnWidgetBoundsChanged(Widget* widget, const gfx::Rect& bounds) override {
    owner_->OnWidgetBoundsChanged(widget, bounds);
  }

  void OnWidgetVisibilityChanging(Widget* widget, bool visible) override {
#if defined(OS_WIN)
    // On Windows we need to handle this before the bubble is visible or hidden.
    // Please see the comment on the OnWidgetVisibilityChanging function. On
    // other platforms it is fine to handle it after the bubble is shown/hidden.
    owner_->OnBubbleWidgetVisibilityChanged(visible);
#endif
  }

  void OnWidgetVisibilityChanged(Widget* widget, bool visible) override {
#if !defined(OS_WIN)
    owner_->OnBubbleWidgetVisibilityChanged(visible);
#endif
    owner_->OnWidgetVisibilityChanged(widget, visible);
  }

  void OnWidgetActivationChanged(Widget* widget, bool active) override {
    owner_->OnBubbleWidgetActivationChanged(active);
    owner_->OnWidgetActivationChanged(widget, active);
  }

 private:
  BubbleDialogDelegate* owner_;
  ScopedObserver<views::Widget, views::WidgetObserver> observer_{this};
};

BubbleDialogDelegate::BubbleDialogDelegate() = default;
BubbleDialogDelegate::BubbleDialogDelegate(View* anchor_view,
                                           BubbleBorder::Arrow arrow,
                                           BubbleBorder::Shadow shadow)
    : arrow_(arrow), shadow_(shadow) {}
BubbleDialogDelegate::~BubbleDialogDelegate() = default;

// static
Widget* BubbleDialogDelegate::CreateBubble(
    BubbleDialogDelegate* bubble_delegate) {
  // On Mac, MODAL_TYPE_WINDOW is implemented using sheets, which can't be
  // anchored at a specific point - they are always placed near the top center
  // of the window. To avoid unpleasant surprises, disallow setting an anchor
  // view or rectangle on these types of bubbles.
  if (bubble_delegate->GetModalType() == ui::MODAL_TYPE_WINDOW) {
    DCHECK(!bubble_delegate->GetAnchorView());
    DCHECK_EQ(bubble_delegate->GetAnchorRect(), gfx::Rect());
  }

  bubble_delegate->Init();
  // Get the latest anchor widget from the anchor view at bubble creation time.
  bubble_delegate->SetAnchorView(bubble_delegate->GetAnchorView());
  Widget* bubble_widget = CreateBubbleWidget(bubble_delegate);

#if (defined(OS_LINUX) && !defined(OS_CHROMEOS)) || defined(OS_APPLE)
  // Linux clips bubble windows that extend outside their parent window bounds.
  // Mac never adjusts.
  bubble_delegate->set_adjust_if_offscreen(false);
#endif

  bubble_delegate->SizeToContents();
  bubble_delegate->bubble_widget_observer_ =
      std::make_unique<BubbleWidgetObserver>(bubble_delegate, bubble_widget);
  bubble_delegate->paint_as_active_subscription_ =
      bubble_widget->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &BubbleDialogDelegate::OnBubbleWidgetPaintAsActiveChanged,
          base::Unretained(bubble_delegate)));
  return bubble_widget;
}

Widget* BubbleDialogDelegateView::CreateBubble(
    std::unique_ptr<BubbleDialogDelegateView> delegate) {
  return CreateBubble(delegate.release());
}
Widget* BubbleDialogDelegateView::CreateBubble(BubbleDialogDelegateView* view) {
  return BubbleDialogDelegate::CreateBubble(view);
}

BubbleDialogDelegateView::BubbleDialogDelegateView()
    : BubbleDialogDelegateView(nullptr, BubbleBorder::TOP_LEFT) {}

BubbleDialogDelegateView::BubbleDialogDelegateView(View* anchor_view,
                                                   BubbleBorder::Arrow arrow,
                                                   BubbleBorder::Shadow shadow)
    : BubbleDialogDelegate(anchor_view, arrow, shadow) {
  set_owned_by_client();
  SetOwnedByWidget(true);
  WidgetDelegate::SetShowCloseButton(false);

  SetArrow(arrow);
  LayoutProvider* provider = LayoutProvider::Get();
  // An individual bubble should override these margins if its layout differs
  // from the typical title/text/buttons.
  set_margins(provider->GetDialogInsetsForContentType(TEXT, TEXT));
  set_title_margins(provider->GetInsetsMetric(INSETS_DIALOG_TITLE));
  if (anchor_view)
    SetAnchorView(anchor_view);
  UpdateColorsFromTheme();
  UMA_HISTOGRAM_BOOLEAN("Dialog.BubbleDialogDelegateView.Create", true);
}

BubbleDialogDelegateView::~BubbleDialogDelegateView() {
  SetLayoutManager(nullptr);
  SetAnchorView(nullptr);
}

BubbleDialogDelegate* BubbleDialogDelegate::AsBubbleDialogDelegate() {
  return this;
}

std::unique_ptr<NonClientFrameView>
BubbleDialogDelegate::CreateNonClientFrameView(Widget* widget) {
  auto frame = std::make_unique<BubbleDialogFrameView>(title_margins_);
  LayoutProvider* provider = LayoutProvider::Get();

  frame->set_footnote_margins(
      provider->GetInsetsMetric(INSETS_DIALOG_SUBSECTION));
  frame->SetFootnoteView(DisownFootnoteView());

  std::unique_ptr<BubbleBorder> border =
      std::make_unique<BubbleBorder>(arrow(), GetShadow(), color());
  if (CustomShadowsSupported() && GetParams().round_corners)
    border->SetCornerRadius(GetCornerRadius());

  frame->SetBubbleBorder(std::move(border));
  return frame;
}

ClientView* BubbleDialogDelegate::CreateClientView(Widget* widget) {
  client_view_ = DialogDelegate::CreateClientView(widget);
  // In order for the |client_view|'s content view hierarchy to respect its
  // rounded corner clip we must paint the client view to a layer. This is
  // necessary because layers do not respect the clip of a non-layer backed
  // parent.
  if (base::FeatureList::IsEnabled(
          features::kEnableMDRoundedCornersOnDialogs) &&
      GetProperty(kPaintClientToLayer)) {
    client_view_->SetPaintToLayer();
    client_view_->layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF(GetCornerRadius()));
    client_view_->layer()->SetIsFastRoundedCorner(true);
  }

  return client_view_;
}

bool BubbleDialogDelegateView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_DOWN ||
      accelerator.key_code() == ui::VKEY_UP) {
    // Move the focus up or down.
    GetFocusManager()->AdvanceFocus(accelerator.key_code() != ui::VKEY_DOWN);
    return true;
  }
  return View::AcceleratorPressed(accelerator);
}

Widget* BubbleDialogDelegateView::GetWidget() {
  return View::GetWidget();
}

const Widget* BubbleDialogDelegateView::GetWidget() const {
  return View::GetWidget();
}

void BubbleDialogDelegateView::AddedToWidget() {
  if (ui::IsAlert(GetAccessibleWindowRole())) {
    GetWidget()->GetRootView()->NotifyAccessibilityEvent(
        ax::mojom::Event::kAlert, true);
  }
}

View* BubbleDialogDelegateView::GetContentsView() {
  return this;
}

void BubbleDialogDelegate::OnBubbleWidgetClosing() {
  // To prevent keyboard focus traversal issues, the anchor view's
  // kAnchoredDialogKey property is cleared immediately upon Close(). This
  // avoids a bug that occured when a focused anchor view is made unfocusable
  // right after the bubble is closed. Previously, focus would advance into the
  // bubble then would be lost when the bubble was destroyed.
  //
  // If kAnchoredDialogKey does not point to |this|, then |this| is not on the
  // focus traversal path. Don't reset kAnchoredDialogKey or we risk detaching
  // a widget from the traversal path.
  if (GetAnchorView() &&
      GetAnchorView()->GetProperty(kAnchoredDialogKey) == this)
    GetAnchorView()->ClearProperty(kAnchoredDialogKey);
}

void BubbleDialogDelegate::OnAnchorWidgetDestroying() {
  SetAnchorView(nullptr);
}

void BubbleDialogDelegate::OnBubbleWidgetActivationChanged(bool active) {
  if (devtools_dismiss_override_)
    return;

#if defined(OS_APPLE)
  // Install |mac_bubble_closer_| the first time the widget becomes active.
  if (active && !mac_bubble_closer_) {
    mac_bubble_closer_ = std::make_unique<ui::BubbleCloser>(
        GetWidget()->GetNativeWindow().GetNativeNSWindow(),
        base::BindRepeating(&BubbleDialogDelegate::OnDeactivate,
                            base::Unretained(this)));
  }
#endif

  if (!active)
    OnDeactivate();
}

void BubbleDialogDelegate::OnAnchorWidgetBoundsChanged() {
  if (GetBubbleFrameView())
    SizeToContents();
}

void BubbleDialogDelegate::OnBubbleWidgetPaintAsActiveChanged() {
  // It's possible for GetWidget() to return null here when the Widget's
  // ownership model is WIDGET_OWNS_NATIVE_WIDGET.  In that case, the View
  // hierarchy is torn down, which detaches rather than destroys |this| due to
  // set_owned_by_client().  Then the native widget is destroyed, which calls
  // back here.  Since GetWidget() is implemented in terms of View::GetWidget(),
  // which no longer has a RootView, it returns null.  While there are other
  // ways to address this, they all seem more fragile than null-checking.
  if (!GetWidget() || !GetWidget()->ShouldPaintAsActive()) {
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

BubbleBorder::Shadow BubbleDialogDelegate::GetShadow() const {
  if (CustomShadowsSupported() || shadow_ == BubbleBorder::NO_ASSETS)
    return shadow_;
  return BubbleBorder::NO_SHADOW;
}

View* BubbleDialogDelegate::GetAnchorView() const {
  if (!anchor_view_observer_)
    return nullptr;
  return anchor_view_observer_->anchor_view();
}

void BubbleDialogDelegate::SetHighlightedButton(Button* highlighted_button) {
  bool visible = GetWidget() && GetWidget()->IsVisible();
  // If the Widget is visible, ensure the old highlight (if any) is removed
  // when the highlighted view changes.
  if (visible && highlighted_button != highlighted_button_tracker_.view())
    UpdateHighlightedButton(false);
  highlighted_button_tracker_.SetView(highlighted_button);
  if (visible)
    UpdateHighlightedButton(true);
}

void BubbleDialogDelegate::SetArrow(BubbleBorder::Arrow arrow) {
  SetArrowWithoutResizing(arrow);
  // If SetArrow() is called before CreateWidget(), there's no need to update
  // the BubbleFrameView.
  if (GetBubbleFrameView())
    SizeToContents();
}

void BubbleDialogDelegate::SetArrowWithoutResizing(BubbleBorder::Arrow arrow) {
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

gfx::Rect BubbleDialogDelegate::GetAnchorRect() const {
  // TODO(tluk) eliminate the need for GetAnchorRect() to return an empty rect
  // if neither an |anchor_rect_| or an anchor view have been set.
  if (!GetAnchorView())
    return anchor_rect_.value_or(gfx::Rect());

  anchor_rect_ = GetAnchorView()->GetAnchorBoundsInScreen();
  anchor_rect_->Inset(anchor_view_insets_);

#if !defined(OS_APPLE)
  // GetAnchorBoundsInScreen returns values that take anchor widget's
  // translation into account, so undo that here. Without this, features which
  // apply transforms on windows such as ChromeOS overview mode will see bubbles
  // offset.
  // TODO(sammiequon): Investigate if we can remove |anchor_widget_| and just
  // replace its calls with GetAnchorView()->GetWidget().
  DCHECK_EQ(anchor_widget_, GetAnchorView()->GetWidget());
  gfx::Transform transform =
      anchor_widget_->GetNativeWindow()->layer()->GetTargetTransform();
  if (!transform.IsIdentity())
    anchor_rect_->Offset(-gfx::ToRoundedVector2d(transform.To2dTranslation()));
#endif

  return anchor_rect_.value();
}

ui::LayerType BubbleDialogDelegate::GetLayerType() const {
  return ui::LAYER_TEXTURED;
}

void BubbleDialogDelegate::SetPaintClientToLayer(bool paint_client_to_layer) {
  DCHECK(!client_view_);
  SetProperty(kPaintClientToLayer, paint_client_to_layer);
}

void BubbleDialogDelegate::UseCompactMargins() {
  set_margins(gfx::Insets(6));
}

void BubbleDialogDelegate::OnAnchorBoundsChanged() {
  if (!GetWidget())
    return;
  // TODO(pbos): Reconsider whether to update the anchor when the view isn't
  // drawn.
  SizeToContents();
}

gfx::Rect BubbleDialogDelegate::GetBubbleBounds() {
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

ax::mojom::Role BubbleDialogDelegate::GetAccessibleWindowRole() {
  // If something in the dialog has initial focus, use the dialog role.
  // Screen readers understand what to announce when focus moves within one.
  if (GetInitiallyFocusedView())
    return ax::mojom::Role::kDialog;

  // Otherwise, return |ax::mojom::Role::kAlertDialog| which will make screen
  // readers announce the contents of the bubble dialog as soon as it appears,
  // as long as we also fire |ax::mojom::Event::kAlert|.
  return ax::mojom::Role::kAlertDialog;
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
  View::OnThemeChanged();
  UpdateColorsFromTheme();
}

void BubbleDialogDelegateView::Init() {}

void BubbleDialogDelegate::SetAnchorView(View* anchor_view) {
  if (anchor_view && anchor_view->GetWidget()) {
    anchor_widget_observer_ =
        std::make_unique<AnchorWidgetObserver>(this, anchor_view->GetWidget());
  } else {
    anchor_widget_observer_.reset();
  }
  if (GetAnchorView()) {
    if (GetAnchorView()->GetProperty(kAnchoredDialogKey) == this)
      GetAnchorView()->ClearProperty(kAnchoredDialogKey);
    anchor_view_observer_.reset();
  }

  // When the anchor view gets set the associated anchor widget might
  // change as well.
  if (!anchor_view || anchor_widget() != anchor_view->GetWidget()) {
    if (anchor_widget()) {
      if (GetWidget() && GetWidget()->IsVisible())
        UpdateHighlightedButton(false);
      paint_as_active_lock_.reset();
      anchor_widget_ = nullptr;
    }
    if (anchor_view) {
      anchor_widget_ = anchor_view->GetWidget();
      if (anchor_widget_) {
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

  if (anchor_view) {
    anchor_view_observer_ =
        std::make_unique<AnchorViewObserver>(this, anchor_view);
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
    // It is possible that a view anchors more than one widgets,
    // but among them there should be at most one widget that is focusable.
    auto* old_anchored_dialog = anchor_view->GetProperty(kAnchoredDialogKey);
    if (old_anchored_dialog && old_anchored_dialog != this)
      DLOG(WARNING) << "|anchor_view| has already anchored a focusable widget.";
    anchor_view->SetProperty(kAnchoredDialogKey, this);
  }
}

void BubbleDialogDelegate::SetAnchorRect(const gfx::Rect& rect) {
  anchor_rect_ = rect;
  if (GetWidget())
    OnAnchorBoundsChanged();
}

void BubbleDialogDelegate::SizeToContents() {
  gfx::Rect bubble_bounds = GetBubbleBounds();
#if defined(OS_APPLE)
  // GetBubbleBounds() doesn't take the Mac NativeWindow's style mask into
  // account, so we need to adjust the size.
  gfx::Size actual_size =
      GetWindowSizeForClientSize(GetWidget(), bubble_bounds.size());
  bubble_bounds.set_size(actual_size);
#endif

  GetWidget()->SetBounds(bubble_bounds);
}

void BubbleDialogDelegateView::UpdateColorsFromTheme() {
  if (!color_explicitly_set()) {
    set_color_internal(GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_BubbleBackground));
  }
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

void BubbleDialogDelegate::OnBubbleWidgetVisibilityChanged(bool visible) {
  UpdateHighlightedButton(visible);

  // Fire ax::mojom::Event::kAlert for bubbles marked as
  // ax::mojom::Role::kAlertDialog; this instructs accessibility tools to read
  // the bubble in its entirety rather than just its title and initially focused
  // view.  See http://crbug.com/474622 for details.
  if (visible) {
    if (ui::IsAlert(GetAccessibleWindowRole())) {
      GetWidget()->GetRootView()->NotifyAccessibilityEvent(
          ax::mojom::Event::kAlert, true);
    }
  }
}

void BubbleDialogDelegate::OnDeactivate() {
  if (close_on_deactivate_ && GetWidget())
    GetWidget()->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
}

void BubbleDialogDelegate::UpdateHighlightedButton(bool highlighted) {
  Button* button = Button::AsButton(highlighted_button_tracker_.view());
  button = button ? button : Button::AsButton(GetAnchorView());
  if (button && highlight_button_when_shown_)
    button->SetHighlighted(highlighted);
}

BEGIN_METADATA(BubbleDialogDelegateView, View)
END_METADATA

}  // namespace views
