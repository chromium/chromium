// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_dialog_delegate_view.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/base/class_property.h"
#include "ui/base/default_style.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/screen.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/bubble_histograms_variant.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_client_view.h"

#if BUILDFLAG(IS_WIN)
#include "ui/base/win/shell.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "ui/views/widget/widget_utils_mac.h"
#else
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

DEFINE_UI_CLASS_PROPERTY_TYPE(std::vector<views::BubbleDialogDelegate*>*)

namespace views {

namespace {

// Some anchors may have multiple bubbles associated with them. This can happen
// if a bubble does not have close_on_deactivate and another bubble appears
// that needs the same anchor view. This property maintains a vector of bubbles
// in anchored order where the last item is the primary anchored bubble. If the
// last item is removed, the new last item, if available, is notified that it is
// the new primary anchored bubble.
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::vector<views::BubbleDialogDelegate*>,
                                   kAnchorVector,
                                   nullptr)

std::vector<BubbleDialogDelegate*>& GetAnchorVector(View* view) {
  if (!view->GetProperty(kAnchorVector)) {
    return *(view->SetProperty(
        kAnchorVector, std::make_unique<std::vector<BubbleDialogDelegate*>>()));
  }

  return *(view->GetProperty(kAnchorVector));
}

// Override base functionality of Widget to give bubble dialogs access to the
// theme provider of the window they're anchored to.
class BubbleWidget : public Widget {
 public:
  BubbleWidget() = default;

  BubbleWidget(const BubbleWidget&) = delete;
  BubbleWidget& operator=(const BubbleWidget&) = delete;

  // Widget:
  const ui::ThemeProvider* GetThemeProvider() const override {
    const Widget* const anchor = GetAnchorWidget();
    return anchor ? anchor->GetThemeProvider() : Widget::GetThemeProvider();
  }

  ui::ColorProviderKey::ThemeInitializerSupplier* GetCustomTheme()
      const override {
    const Widget* const anchor = GetAnchorWidget();
    return anchor ? anchor->GetCustomTheme() : Widget::GetCustomTheme();
  }

  const ui::NativeTheme* GetNativeTheme() const override {
    const Widget* const anchor = GetAnchorWidget();
    return anchor ? anchor->GetNativeTheme() : Widget::GetNativeTheme();
  }

  using Widget::GetPrimaryWindowWidget;

  Widget* GetPrimaryWindowWidget() override {
    Widget* const anchor = GetAnchorWidget();
    return anchor ? anchor->GetPrimaryWindowWidget()
                  : Widget::GetPrimaryWindowWidget();
  }

  const ui::ColorProvider* GetColorProvider() const override {
    const Widget* const primary = GetPrimaryWindowWidget();
    return (primary && primary != this) ? primary->GetColorProvider()
                                        : Widget::GetColorProvider();
  }

 private:
  const Widget* GetAnchorWidget() const {
    // TODO(pbos): Could this use Widget::parent() instead of anchor_widget()?
    BubbleDialogDelegate* const bubble_delegate =
        static_cast<BubbleDialogDelegate*>(widget_delegate());
    return bubble_delegate ? bubble_delegate->anchor_widget() : nullptr;
  }
  Widget* GetAnchorWidget() {
    return const_cast<Widget*>(std::as_const(*this).GetAnchorWidget());
  }
};

// The frame view for bubble dialog widgets. These are not user-sizable so have
// simplified logic for minimum and maximum sizes to avoid repeated calls to
// CalculatePreferredSize().
class BubbleDialogFrameView : public BubbleFrameView {
 public:
  explicit BubbleDialogFrameView(const gfx::Insets& title_margins)
      : BubbleFrameView(title_margins, gfx::Insets()) {}

  BubbleDialogFrameView(const BubbleDialogFrameView&) = delete;
  BubbleDialogFrameView& operator=(const BubbleDialogFrameView&) = delete;

  // View:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }
  gfx::Size GetMaximumSize() const override { return gfx::Size(); }
};

// Create a widget to host the bubble.
Widget* CreateBubbleWidget(BubbleDialogDelegate* bubble,
                           Widget::InitParams::Ownership ownership =
                               Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET) {
  auto bubble_widget = std::make_unique<BubbleWidget>();
  Widget::InitParams bubble_params(ownership, Widget::InitParams::TYPE_BUBBLE);
  bubble_params.delegate = bubble;
  bubble_params.opacity = Widget::InitParams::WindowOpacity::kTranslucent;
  bubble_params.accept_events = bubble->accept_events();
  bubble_params.remove_standard_frame = true;
  bubble_params.layer_type = bubble->GetLayerType();
  // TODO(crbug.com/41493925): Remove CHECK once native frame dialogs support
  // autosize.
  CHECK(!bubble->is_autosized() || bubble->use_custom_frame())
      << "Autosizing native frame dialogs is not supported.";
  bubble_params.autosize = bubble->is_autosized();

  // Use a window default shadow if the bubble doesn't provides its own.
  if (bubble->GetShadow() == BubbleBorder::NO_SHADOW)
    bubble_params.shadow_type = Widget::InitParams::ShadowType::kDefault;
  else
    bubble_params.shadow_type = Widget::InitParams::ShadowType::kNone;
  gfx::NativeView parent = gfx::NativeView();
  if (bubble->has_parent()) {
    if (bubble->parent_window()) {
      parent = bubble->parent_window();
    } else if (bubble->anchor_widget()) {
      parent = bubble->anchor_widget()->GetNativeView();
    }
  }
  bubble_params.parent = parent;
  bubble_params.activatable = bubble->CanActivate()
                                  ? Widget::InitParams::Activatable::kYes
                                  : Widget::InitParams::Activatable::kNo;
  bubble->OnBeforeBubbleWidgetInit(&bubble_params, bubble_widget.get());
  DCHECK(bubble_params.parent || !bubble->has_parent());
  bubble_widget->Init(std::move(bubble_params));
  return bubble_widget.release();
}

}  // namespace

class BubbleDialogDelegate::AnchorViewObserver : public ViewObserver {
 public:
  AnchorViewObserver(BubbleDialogDelegate* parent, View* anchor_view)
      : parent_(parent), anchor_view_(anchor_view) {
    anchor_view_->AddObserver(this);
    AddToAnchorVector();
  }

  AnchorViewObserver(const AnchorViewObserver&) = delete;
  AnchorViewObserver& operator=(const AnchorViewObserver&) = delete;

  ~AnchorViewObserver() override {
    RemoveFromAnchorVector();
    anchor_view_->RemoveObserver(this);
  }

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
  void AddToAnchorVector() {
    auto& vector = GetAnchorVector(anchor_view_);
    DCHECK(!base::Contains(vector, parent_));
    vector.push_back(parent_);
  }

  void RemoveFromAnchorVector() {
    auto& vector = GetAnchorVector(anchor_view_);
    DCHECK(!vector.empty());

    auto iter = vector.cend() - 1;
    bool latest_anchor_bubble_will_change = (*iter == parent_);
    if (!latest_anchor_bubble_will_change)
      iter = std::find(vector.cbegin(), iter, parent_);

    DCHECK(iter != vector.cend());
    vector.erase(iter);

    if (!vector.empty() && latest_anchor_bubble_will_change)
      vector.back()->NotifyAnchoredBubbleIsPrimary();
  }

  const raw_ptr<BubbleDialogDelegate> parent_;
  const raw_ptr<View> anchor_view_;
};

// This class is responsible for observing events on a BubbleDialogDelegate's
// anchor widget and notifying the BubbleDialogDelegate of them.
#if BUILDFLAG(IS_MAC)
class BubbleDialogDelegate::AnchorWidgetObserver : public WidgetObserver {
#else
class BubbleDialogDelegate::AnchorWidgetObserver : public WidgetObserver,
                                                   public aura::WindowObserver {
#endif

 public:
  AnchorWidgetObserver(BubbleDialogDelegate* owner, Widget* widget)
      : owner_(owner) {
    widget_observation_.Observe(widget);
#if !BUILDFLAG(IS_MAC)
    window_observation_.Observe(widget->GetNativeWindow());
#endif
  }
  ~AnchorWidgetObserver() override = default;

  // WidgetObserver:
  void OnWidgetDestroying(Widget* widget) override {
#if !BUILDFLAG(IS_MAC)
    DCHECK(window_observation_.IsObservingSource(widget->GetNativeWindow()));
    window_observation_.Reset();
#endif
    DCHECK(widget_observation_.IsObservingSource(widget));
    widget_observation_.Reset();
    owner_->OnAnchorWidgetDestroying();
    // |this| may be destroyed here!
  }

  void OnWidgetActivationChanged(Widget* widget, bool active) override {
    owner_->OnWidgetActivationChanged(widget, active);
  }

  void OnWidgetBoundsChanged(Widget* widget, const gfx::Rect&) override {
    owner_->OnAnchorBoundsChanged();
  }

  void OnWidgetThemeChanged(Widget* widget) override {
    // TODO(dfried): Consider merging BubbleWidget with ThemeCopyingWidget
    // instead of observing the theme here.
    owner_->GetWidget()->ThemeChanged();
  }

#if !BUILDFLAG(IS_MAC)
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

  // If the native window is closed by the OS, OnWidgetDestroying() won't
  // fire. Instead, OnWindowDestroying() will fire before aura::Window
  // destruction. See //docs/ui/views/widget_destruction.md.
  void OnWindowDestroying(aura::Window* window) override {
    window_observation_.Reset();
  }
#endif

 private:
  raw_ptr<BubbleDialogDelegate> owner_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
#if !BUILDFLAG(IS_MAC)
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
#endif
};

// This class is responsible for observing events on a BubbleDialogDelegate's
// widget and notifying the BubbleDialogDelegate of them.
class BubbleDialogDelegate::BubbleWidgetObserver : public WidgetObserver {
 public:
  BubbleWidgetObserver(BubbleDialogDelegate* owner, Widget* widget)
      : owner_(owner) {
    observation_.Observe(widget);
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
    DCHECK(observation_.IsObservingSource(widget));
    observation_.Reset();
    owner_->OnWidgetDestroyed(widget);
  }

  void OnWidgetBoundsChanged(Widget* widget, const gfx::Rect& bounds) override {
    owner_->OnWidgetBoundsChanged(widget, bounds);
  }

  void OnWidgetVisibilityChanged(Widget* widget, bool visible) override {
    owner_->OnBubbleWidgetVisibilityChanged(visible);
    owner_->OnWidgetVisibilityChanged(widget, visible);
  }

  void OnWidgetActivationChanged(Widget* widget, bool active) override {
    owner_->OnBubbleWidgetActivationChanged(active);
    owner_->OnWidgetActivationChanged(widget, active);
  }

 private:
  const raw_ptr<BubbleDialogDelegate> owner_;
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
};

class BubbleDialogDelegate::ThemeObserver : public ViewObserver {
 public:
  explicit ThemeObserver(BubbleDialogDelegate* delegate) : delegate_(delegate) {
    observation_.Observe(delegate->GetContentsView());
  }

  void OnViewThemeChanged(views::View* view) override {
    delegate_->UpdateColorsFromTheme();
  }

 private:
  const raw_ptr<BubbleDialogDelegate> delegate_;
  base::ScopedObservation<View, ViewObserver> observation_{this};
};

class BubbleDialogDelegateView::CloseOnDeactivatePin::Pins {
 public:
  Pins() = default;
  ~Pins() = default;

  bool is_pinned() const { return !pins_.empty(); }

  base::WeakPtr<Pins> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

  void AddPin(CloseOnDeactivatePin* pin) {
    const auto result = pins_.insert(pin);
    DCHECK(result.second);
  }

  void RemovePin(CloseOnDeactivatePin* pin) {
    const auto result = pins_.erase(pin);
    DCHECK(result);
  }

 protected:
  std::set<raw_ptr<CloseOnDeactivatePin, SetExperimental>> pins_;
  base::WeakPtrFactory<Pins> weak_ptr_factory_{this};
};

BubbleDialogDelegate::CloseOnDeactivatePin::CloseOnDeactivatePin(
    base::WeakPtr<Pins> pins)
    : pins_(pins) {
  pins_->AddPin(this);
}

BubbleDialogDelegate::CloseOnDeactivatePin::~CloseOnDeactivatePin() {
  Pins* const pins = pins_.get();
  if (pins)
    pins->RemovePin(this);
}

BubbleDialogDelegate::BubbleDialogDelegate(View* anchor_view,
                                           BubbleBorder::Arrow arrow,
                                           BubbleBorder::Shadow shadow,
                                           bool autosize)
    : arrow_(arrow),
      shadow_(shadow),
      autosize_(autosize),
      close_on_deactivate_pins_(std::make_unique<CloseOnDeactivatePin::Pins>()),
      bubble_created_time_(base::TimeTicks::Now()) {
  bubble_uma_logger().set_delegate(this);
  SetOwnedByWidget(true);
  SetAnchorView(anchor_view);
  SetArrow(arrow);
  SetShowCloseButton(false);

  LayoutProvider* const layout_provider = LayoutProvider::Get();
  set_margins(layout_provider->GetDialogInsetsForContentType(
      DialogContentType::kText, DialogContentType::kText));
  set_title_margins(layout_provider->GetInsetsMetric(INSETS_DIALOG_TITLE));
  set_footnote_margins(
      layout_provider->GetInsetsMetric(INSETS_DIALOG_FOOTNOTE));

  set_desired_bounds_delegate(base::BindRepeating(
      &BubbleDialogDelegate::GetDesiredBubbleBounds, base::Unretained(this)));

  RegisterWidgetInitializedCallback(base::BindOnce(
      [](BubbleDialogDelegate* bubble_delegate) {
        bubble_delegate->theme_observer_ =
            std::make_unique<ThemeObserver>(bubble_delegate);
        // Call the theme callback to make sure the initial theme is picked up
        // by the BubbleDialogDelegate.
        bubble_delegate->UpdateColorsFromTheme();
      },
      this));

  // Bind a callback to the compositor for logging time from bubble creation to
  // successful presentation of the next frame.
  RegisterWidgetInitializedCallback(base::BindOnce(
      [](BubbleDialogDelegate* delegate, base::TimeTicks bubble_created_time) {
        delegate->GetWidget()
            ->GetCompositor()
            ->RequestSuccessfulPresentationTimeForNextFrame(base::BindOnce(
                [](base::WeakPtr<BubbleDialogDelegate::BubbleUmaLogger>
                       uma_logger,
                   base::TimeTicks bubble_created_time,
                   const viz::FrameTimingDetails& frame_timing_details) {
                  base::TimeTicks presentation_timestamp =
                      frame_timing_details.presentation_feedback.timestamp;
                  if (!uma_logger) {
                    return;
                  }
                  uma_logger->LogMetric(
                      base::UmaHistogramTimes, "CreateToPresentationTime",
                      presentation_timestamp - bubble_created_time);
                },
                delegate->bubble_uma_logger().GetWeakPtr(),
                bubble_created_time));
      },
      base::Unretained(this), *bubble_created_time_));
}

BubbleDialogDelegate::~BubbleDialogDelegate() {
  SetAnchorView(nullptr);
}

// static
Widget* BubbleDialogDelegate::CreateBubble(
    std::unique_ptr<BubbleDialogDelegate> bubble_delegate_unique,
    Widget::InitParams::Ownership ownership) {
  BubbleDialogDelegate* const bubble_delegate = bubble_delegate_unique.get();

  // On Mac, MODAL_TYPE_WINDOW is implemented using sheets, which can't be
  // anchored at a specific point - they are always placed near the top center
  // of the window. To avoid unpleasant surprises, disallow setting an anchor
  // view or rectangle on these types of bubbles.
  if (bubble_delegate->GetModalType() == ui::mojom::ModalType::kWindow) {
    DCHECK(!bubble_delegate->GetAnchorView());
    DCHECK_EQ(bubble_delegate->GetAnchorRect(), gfx::Rect());
  }

  bubble_delegate->Init();
  // Get the latest anchor widget from the anchor view at bubble creation time.
  bubble_delegate->SetAnchorView(bubble_delegate->GetAnchorView());
  Widget* const bubble_widget =
      CreateBubbleWidget(bubble_delegate_unique.release(), ownership);

  bubble_delegate->set_adjust_if_offscreen(
      PlatformStyle::kAdjustBubbleIfOffscreen);

  bubble_delegate->SizeToContents();
  bubble_delegate->bubble_widget_observer_ =
      std::make_unique<BubbleWidgetObserver>(bubble_delegate, bubble_widget);
  return bubble_widget;
}

Widget* BubbleDialogDelegateView::CreateBubble(
    BubbleDialogDelegateView* delegate_view,
    Widget::InitParams::Ownership ownership) {
  return CreateBubble(base::WrapUnique(delegate_view), ownership);
}

BubbleDialogDelegateView::BubbleDialogDelegateView()
    : BubbleDialogDelegateView(nullptr, BubbleBorder::TOP_LEFT) {}

BubbleDialogDelegateView::BubbleDialogDelegateView(View* anchor_view,
                                                   BubbleBorder::Arrow arrow,
                                                   BubbleBorder::Shadow shadow,
                                                   bool autosize)
    : BubbleDialogDelegate(anchor_view, arrow, shadow, autosize) {
  bubble_uma_logger().set_bubble_view(this);
  SetOwnedByWidget(false);
}

BubbleDialogDelegateView::~BubbleDialogDelegateView() {
  // TODO(pbos): Investigate if this is actually still needed, and if so
  // document here why that's the case. If it's due to specific client layout
  // managers, push this down to client destructors.
  SetLayoutManager(nullptr);
  // TODO(pbos): See if we can resolve this better. This currently prevents a
  // crash that shows up in BubbleFrameViewTest.WidthSnaps. This crash seems to
  // happen at GetWidget()->IsVisible() inside SetAnchorView(nullptr) and seems
  // to be as a result of WidgetDelegate's widget_ not getting updated during
  // destruction when BubbleDialogDelegateView::DeleteDelegate() doesn't delete
  // itself, as Widget drops a reference to widget_delegate_ and can't inform it
  // of WidgetDeleting in ~Widget.
  SetAnchorView(nullptr);
}

BubbleDialogDelegate* BubbleDialogDelegate::AsBubbleDialogDelegate() {
  return this;
}

std::unique_ptr<NonClientFrameView>
BubbleDialogDelegate::CreateNonClientFrameView(Widget* widget) {
  auto frame = std::make_unique<BubbleDialogFrameView>(title_margins_);

  frame->SetFootnoteMargins(footnote_margins_);
  frame->SetFootnoteView(DisownFootnoteView());

  std::unique_ptr<BubbleBorder> border =
      std::make_unique<BubbleBorder>(arrow(), GetShadow());
  border->SetColor(color());
  if (GetParams().round_corners) {
    border->SetCornerRadius(GetCornerRadius());
  }

  frame->SetBubbleBorder(std::move(border));
  return frame;
}

ClientView* BubbleDialogDelegate::CreateClientView(Widget* widget) {
  client_view_ = DialogDelegate::CreateClientView(widget);
  // In order for the |client_view|'s content view hierarchy to respect its
  // rounded corner clip we must paint the client view to a layer. This is
  // necessary because layers do not respect the clip of a non-layer backed
  // parent.
  if (paint_client_to_layer_) {
    client_view_->SetPaintToLayer();
    client_view_->layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF(GetCornerRadius()));
    client_view_->layer()->SetIsFastRoundedCorner(true);
  }

  return client_view_;
}

Widget* BubbleDialogDelegateView::GetWidget() {
  return View::GetWidget();
}

const Widget* BubbleDialogDelegateView::GetWidget() const {
  return View::GetWidget();
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
      GetAnchorView()->GetProperty(kAnchoredDialogKey) == this) {
    GetAnchorView()->ClearProperty(kAnchoredDialogKey);
  }

  if (bubble_shown_time_.has_value()) {
    bubble_shown_duration_ += base::TimeTicks::Now() - *bubble_shown_time_;
    bubble_shown_time_.reset();
  }
  bubble_uma_logger().LogMetric(base::UmaHistogramLongTimes, "TimeVisible",
                                bubble_shown_duration_);

  bubble_uma_logger().LogMetric(base::UmaHistogramEnumeration, "CloseReason",
                                GetWidget()->closed_reason());
}

void BubbleDialogDelegate::OnAnchorWidgetDestroying() {
  SetAnchorView(nullptr);
}

void BubbleDialogDelegate::OnBubbleWidgetActivationChanged(bool active) {
#if BUILDFLAG(IS_MAC)
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


BubbleBorder::Shadow BubbleDialogDelegate::GetShadow() const {
  if (!Widget::IsWindowCompositingSupported()) {
    return BubbleBorder::Shadow::NO_SHADOW;
  }
  return shadow_;
}

View* BubbleDialogDelegate::GetAnchorView() const {
  if (!anchor_view_observer_)
    return nullptr;
  return anchor_view_observer_->anchor_view();
}

void BubbleDialogDelegate::SetMainImage(ui::ImageModel main_image) {
  // Adding a main image while the bubble is showing is not supported (but
  // changing it is). Adding an image while it's showing would require a jarring
  // re-layout.
  if (main_image_.IsEmpty())
    DCHECK(!GetBubbleFrameView());
  main_image_ = std::move(main_image);
  if (GetBubbleFrameView())
    GetBubbleFrameView()->UpdateMainImage();
}

bool BubbleDialogDelegate::ShouldCloseOnDeactivate() const {
  return close_on_deactivate_ && !close_on_deactivate_pins_->is_pinned();
}

std::unique_ptr<BubbleDialogDelegate::CloseOnDeactivatePin>
BubbleDialogDelegate::PreventCloseOnDeactivate() {
  return base::WrapUnique(
      new CloseOnDeactivatePin(close_on_deactivate_pins_->GetWeakPtr()));
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
  View* anchor_view = GetAnchorView();
  if (!anchor_view)
    return anchor_rect_.value_or(gfx::Rect());

  anchor_rect_ = anchor_view->GetAnchorBoundsInScreen();

#if !BUILDFLAG(IS_MAC)
  // GetAnchorBoundsInScreen returns values that take anchor widget's
  // translation into account, so undo that here. Without this, features which
  // apply transforms on windows such as ChromeOS overview mode will see bubbles
  // offset.
  if (anchor_widget_) {
    gfx::Transform transform =
        anchor_widget_->GetNativeWindow()->layer()->GetTargetTransform();
    if (!transform.IsIdentity())
      anchor_rect_->Offset(
          -gfx::ToRoundedVector2d(transform.To2dTranslation()));
  }
#endif

  // Remove additional whitespace padding that was added to the view
  // so that anchor_rect centers on the anchor and not skewed by the whitespace
  BubbleFrameView* frame_view = GetBubbleFrameView();
  if (frame_view && frame_view->GetDisplayVisibleArrow()) {
    gfx::Insets* padding = anchor_view->GetProperty(kInternalPaddingKey);
    if (padding != nullptr)
      anchor_rect_->Inset(*padding);
  }

  return anchor_rect_.value();
}

SkColor BubbleDialogDelegate::GetBackgroundColor() {
  UpdateColorsFromTheme();
  return color();
}

ui::LayerType BubbleDialogDelegate::GetLayerType() const {
  return ui::LAYER_TEXTURED;
}

void BubbleDialogDelegate::SetPaintClientToLayer(bool paint_client_to_layer) {
  DCHECK(!client_view_);
  paint_client_to_layer_ = paint_client_to_layer;
}

void BubbleDialogDelegate::UseCompactMargins() {
  set_margins(gfx::Insets(6));
}

// static
gfx::Size BubbleDialogDelegate::GetMaxAvailableScreenSpaceToPlaceBubble(
    View* anchor_view,
    BubbleBorder::Arrow arrow,
    bool adjust_if_offscreen,
    BubbleFrameView::PreferredArrowAdjustment arrow_adjustment) {
  // TODO(sanchit.abrol@microsoft.com): Implement for other arrows.
  DCHECK(arrow == BubbleBorder::TOP_LEFT || arrow == BubbleBorder::TOP_RIGHT ||
         arrow == BubbleBorder::BOTTOM_RIGHT ||
         arrow == BubbleBorder::BOTTOM_LEFT);
  DCHECK_EQ(arrow_adjustment,
            BubbleFrameView::PreferredArrowAdjustment::kMirror);

#if BUILDFLAG(IS_OZONE)
  // This function should not be called in ozone platforms where global screen
  // coordinates are not available.
  DCHECK(ui::OzonePlatform::GetInstance()
             ->GetPlatformProperties()
             .supports_global_screen_coordinates);
#endif

  gfx::Rect anchor_rect = anchor_view->GetAnchorBoundsInScreen();
  gfx::Rect screen_rect =
      display::Screen::GetScreen()
          ->GetDisplayNearestPoint(anchor_rect.CenterPoint())
          .work_area();

  gfx::Size max_available_space;

  if (adjust_if_offscreen) {
    max_available_space = GetAvailableSpaceToPlaceBubble(
        BubbleBorder::TOP_LEFT, anchor_rect, screen_rect);
    max_available_space.SetToMax(GetAvailableSpaceToPlaceBubble(
        BubbleBorder::TOP_RIGHT, anchor_rect, screen_rect));
    max_available_space.SetToMax(GetAvailableSpaceToPlaceBubble(
        BubbleBorder::BOTTOM_RIGHT, anchor_rect, screen_rect));
    max_available_space.SetToMax(GetAvailableSpaceToPlaceBubble(
        BubbleBorder::BOTTOM_LEFT, anchor_rect, screen_rect));
  } else {
    max_available_space =
        GetAvailableSpaceToPlaceBubble(arrow, anchor_rect, screen_rect);
  }

  return max_available_space;
}

// static
gfx::Size BubbleDialogDelegate::GetAvailableSpaceToPlaceBubble(
    BubbleBorder::Arrow arrow,
    gfx::Rect anchor_rect,
    gfx::Rect screen_rect) {
  int available_height_below = screen_rect.bottom() - anchor_rect.bottom();

  int available_height_above = anchor_rect.y() - screen_rect.y();

  int available_width_on_left = anchor_rect.right() - screen_rect.x();

  int available_width_on_right = screen_rect.right() - anchor_rect.x();

  return {BubbleBorder::is_arrow_on_left(arrow) ? available_width_on_right
                                                : available_width_on_left,
          BubbleBorder::is_arrow_on_top(arrow) ? available_height_below
                                               : available_height_above};
}

void BubbleDialogDelegate::OnAnchorBoundsChanged() {
  if (!GetWidget())
    return;
  // TODO(pbos): Reconsider whether to update the anchor when the view isn't
  // drawn.
  SizeToContents();

  // We will not accept input event a short time after anchored view changed.
  UpdateInputProtectorsTimeStamp();
}

void BubbleDialogDelegate::UpdateInputProtectorsTimeStamp() {
  if (auto* dialog = GetDialogClientView())
    dialog->UpdateInputProtectorTimeStamp();

  GetBubbleFrameView()->UpdateInputProtectorTimeStamp();
}

BubbleDialogDelegate::BubbleUmaLogger::BubbleUmaLogger() = default;

BubbleDialogDelegate::BubbleUmaLogger::~BubbleUmaLogger() = default;

base::WeakPtr<BubbleDialogDelegate::BubbleUmaLogger>
BubbleDialogDelegate::BubbleUmaLogger::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::optional<std::string>
BubbleDialogDelegate::BubbleUmaLogger::GetBubbleName() const {
  // Some dialogs might only use BDD and not BDDV. In those cases, the class
  // name should be based on BDDs' content view.
  if (delegate_.has_value()) {
    std::string class_name =
        delegate_.value()->GetContentsView()->GetClassName();
    if (class_name != "View") {
      return class_name;
    }
  }

  if (bubble_view_.has_value()) {
    return bubble_view_.value()->GetClassName();
  }
  return std::optional<std::string>();
}

template <typename Value>
void BubbleDialogDelegate::BubbleUmaLogger::LogMetric(
    void (*uma_func)(const std::string&, Value),
    const std::string& histogram_name,
    Value value) const {
  if (!base::FeatureList::IsEnabled(::features::kBubbleMetricsApi)) {
    return;
  }
  // Record histogram for all BDDV subclasses under a generic name
  uma_func(base::StrCat({"Bubble.All.", histogram_name}), value);
  // Record histograms for specific BDDV subclasses
  std::optional<std::string> bubble_name = GetBubbleName();
  if (!bubble_name.has_value()) {
    return;
  }

  if (allowed_class_names_for_testing_.has_value()) {
    if (!base::Contains(allowed_class_names_for_testing_.value(),
                        bubble_name.value())) {
      return;
    }
  } else if (!views_metrics::IsValidBubbleName(bubble_name.value())) {
    return;
  }

  uma_func(base::StrCat({"Bubble.", bubble_name.value(), ".", histogram_name}),
           value);
}

// Instantiate template function to be able to use in views_unittests.
template VIEWS_EXPORT void BubbleDialogDelegate::BubbleUmaLogger::LogMetric<
    base::TimeDelta>(void (*uma_func)(const std::string&, base::TimeDelta),
                     const std::string& histogram_name,
                     base::TimeDelta value) const;

gfx::Rect BubbleDialogDelegate::GetBubbleBounds() {
  // The argument rect has its origin at the bubble's arrow anchor point;
  // its size is the preferred size of the bubble's client view (this view).
  bool anchor_minimized = anchor_widget() && anchor_widget()->IsMinimized();
  // If GetAnchorView() returns nullptr or GetAnchorRect() returns an empty rect
  // at (0, 0), don't try and adjust arrow if off-screen.
  gfx::Rect anchor_rect = GetAnchorRect();
  bool has_anchor = GetAnchorView() || anchor_rect != gfx::Rect();
  return GetBubbleFrameView()->GetUpdatedWindowBounds(
      anchor_rect, arrow(), GetWidget()->client_view()->GetPreferredSize({}),
      adjust_if_offscreen_ && !anchor_minimized && has_anchor);
}

ax::mojom::Role BubbleDialogDelegate::GetAccessibleWindowRole() {
  const ax::mojom::Role accessible_role =
      WidgetDelegate::GetAccessibleWindowRole();
  // If the accessible role has been explicitly set to anything else than its
  // default, use it. The rest translates kWindow to kDialog or kAlertDialog.
  if (accessible_role != ax::mojom::Role::kWindow)
    return accessible_role;

  // If something in the dialog has initial focus, use the dialog role.
  // Screen readers understand what to announce when focus moves within one.
  if (GetInitiallyFocusedView())
    return ax::mojom::Role::kDialog;

  // Otherwise, return |ax::mojom::Role::kAlertDialog| which will make screen
  // readers announce the contents of the bubble dialog as soon as it appears,
  // as long as we also fire |ax::mojom::Event::kAlert|.
  return ax::mojom::Role::kAlertDialog;
}

gfx::Rect BubbleDialogDelegate::GetDesiredBubbleBounds() {
  CHECK(use_custom_frame())
      << "GetBubbleBounds() for native frame dialogs is not supported.";

  gfx::Rect bubble_bounds = GetBubbleBounds();

#if BUILDFLAG(IS_MAC)
  // GetBubbleBounds() doesn't take the Mac NativeWindow's style mask into
  // account, so we need to adjust the size.
  gfx::Size actual_size =
      GetWindowSizeForClientSize(GetWidget(), bubble_bounds.size());
  bubble_bounds.set_size(actual_size);
#endif

  return bubble_bounds;
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
      anchor_widget_ = nullptr;
    }
    if (anchor_view) {
      anchor_widget_ = anchor_view->GetProperty(kWidgetForAnchoringKey);
      if (!anchor_widget_) {
        anchor_widget_ = anchor_view->GetWidget();
      }
      if (anchor_widget_) {
        const bool visible = GetWidget() && GetWidget()->IsVisible();
        UpdateHighlightedButton(visible);
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

    SetAnchoredDialogKey();
  }
}

void BubbleDialogDelegate::SetAnchorRect(const gfx::Rect& rect) {
  anchor_rect_ = rect;
  if (GetWidget())
    OnAnchorBoundsChanged();
}

void BubbleDialogDelegate::SizeToContents() {
  GetWidget()->SetBounds(GetDesiredWidgetBounds());
}

std::u16string BubbleDialogDelegate::GetSubtitle() const {
  return subtitle_;
}

void BubbleDialogDelegate::SetSubtitle(const std::u16string& subtitle) {
  if (subtitle_ == subtitle)
    return;
  subtitle_ = subtitle;
  BubbleFrameView* frame_view = GetBubbleFrameView();
  if (frame_view)
    frame_view->UpdateSubtitle();
}

bool BubbleDialogDelegate::GetSubtitleAllowCharacterBreak() const {
  return subtitle_allow_character_break_;
}

void BubbleDialogDelegate::SetSubtitleAllowCharacterBreak(bool allow) {
  if (subtitle_allow_character_break_ == allow) {
    return;
  }
  subtitle_allow_character_break_ = allow;
  BubbleFrameView* frame_view = GetBubbleFrameView();
  if (frame_view) {
    frame_view->UpdateSubtitle();
  }
}

void BubbleDialogDelegate::UpdateColorsFromTheme() {
  View* const contents_view = GetContentsView();
  DCHECK(contents_view);
  if (!color_explicitly_set()) {
    set_color_internal(contents_view->GetColorProvider()->GetColor(
        ui::kColorBubbleBackground));
  }
  BubbleFrameView* frame_view = GetBubbleFrameView();
  if (frame_view)
    frame_view->SetBackgroundColor(color());

  // When there's an opaque layer, the bubble border background won't show
  // through, so explicitly paint a background color.
  const bool contents_layer_opaque =
      contents_view->layer() && contents_view->layer()->fills_bounds_opaquely();
  contents_view->SetBackground(contents_layer_opaque ||
                                       force_create_contents_background_
                                   ? CreateSolidBackground(color())
                                   : nullptr);
}

void BubbleDialogDelegate::OnBubbleWidgetVisibilityChanged(bool visible) {
  // Log time from bubble dialog delegate creation to bubble becoming
  // visible.
  if (visible) {
    if (GetWidget()->IsClosed()) {
      return;
    }
    if (bubble_created_time_.has_value()) {
      GetWidget()
          ->GetCompositor()
          ->RequestSuccessfulPresentationTimeForNextFrame(base::BindOnce(
              [](base::WeakPtr<BubbleDialogDelegate::BubbleUmaLogger>
                     uma_logger,
                 base::TimeTicks bubble_created_time,
                 const viz::FrameTimingDetails& frame_timing_details) {
                base::TimeTicks presentation_timestamp =
                    frame_timing_details.presentation_feedback.timestamp;
                if (!uma_logger) {
                  return;
                }
                uma_logger->LogMetric(
                    base::UmaHistogramMediumTimes, "CreateToVisibleTime",
                    presentation_timestamp - bubble_created_time);
              },
              bubble_uma_logger().GetWeakPtr(), *bubble_created_time_));
      bubble_created_time_.reset();
    }
    bubble_shown_time_ = base::TimeTicks::Now();
  } else {
    if (bubble_shown_time_.has_value()) {
      bubble_shown_duration_ += base::TimeTicks::Now() - *bubble_shown_time_;
      bubble_shown_time_.reset();
    }
  }

  UpdateHighlightedButton(visible);

  // Fire ax::mojom::Event::kAlert for bubbles marked as
  // ax::mojom::Role::kAlertDialog; this instructs accessibility tools to read
  // the bubble in its entirety rather than just its title and initially focused
  // view.  See http://crbug.com/474622 for details.
  if (visible && ui::IsAlert(GetAccessibleWindowRole())) {
    GetWidget()->GetRootView()->GetViewAccessibility().SetRole(
        GetAccessibleWindowRole());
    GetWidget()->GetRootView()->NotifyAccessibilityEvent(
        ax::mojom::Event::kAlert, true);
  }
}

void BubbleDialogDelegate::OnDeactivate() {
  if (ShouldCloseOnDeactivate() && GetWidget()) {
    GetWidget()->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
  }
}

void BubbleDialogDelegate::NotifyAnchoredBubbleIsPrimary() {
  const bool visible = GetWidget() && GetWidget()->IsVisible();
  UpdateHighlightedButton(visible);
  SetAnchoredDialogKey();
}

void BubbleDialogDelegate::SetAnchoredDialogKey() {
  auto* anchor_view = GetAnchorView();
  DCHECK(anchor_view);
  if (focus_traversable_from_anchor_view_) {
    // Make sure that focus can move into here from the anchor view (but not
    // out, focus will cycle inside the dialog once it gets here).
    // It is possible that a view anchors more than one widgets,
    // but among them there should be at most one widget that is focusable.
    auto* old_anchored_dialog = anchor_view->GetProperty(kAnchoredDialogKey);
    if (old_anchored_dialog && old_anchored_dialog != this)
      DLOG(WARNING) << "|anchor_view| has already anchored a focusable widget.";
    anchor_view->SetProperty(kAnchoredDialogKey,
                             static_cast<DialogDelegate*>(this));
  }
}

void BubbleDialogDelegate::UpdateHighlightedButton(bool highlighted) {
  Button* button = Button::AsButton(highlighted_button_tracker_.view());
  button = button ? button : Button::AsButton(GetAnchorView());
  if (button && highlight_button_when_shown_) {
    if (highlighted) {
      button_anchor_highlight_ = button->AddAnchorHighlight();
    } else {
      button_anchor_highlight_.reset();
    }
  }
}

BEGIN_METADATA(BubbleDialogDelegateView)
END_METADATA

}  // namespace views
