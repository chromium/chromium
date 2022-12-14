// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/tooltip_aura.h"

#include <algorithm>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/owned_window_anchor.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/tooltip_observer.h"

namespace {

// Paddings
constexpr int kHorizontalPadding = 8;
constexpr int kVerticalPaddingTop = 4;
constexpr int kVerticalPaddingBottom = 5;

// TODO(varkha): Update if native widget can be transparent on Linux.
bool CanUseTranslucentTooltipWidget() {
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || BUILDFLAG(IS_WIN)
  return false;
#else
  return true;
#endif
}

// TODO(oshima): Consider to use views::Label.
class TooltipView : public views::View {
 public:
  METADATA_HEADER(TooltipView);
  TooltipView() : render_text_(gfx::RenderText::CreateRenderText()) {
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(kVerticalPaddingTop, kHorizontalPadding,
                          kVerticalPaddingBottom, kHorizontalPadding)));

    render_text_->SetWordWrapBehavior(gfx::WRAP_LONG_WORDS);
    render_text_->SetMultiline(true);

    ResetDisplayRect();
  }

  TooltipView(const TooltipView&) = delete;
  TooltipView& operator=(const TooltipView&) = delete;
  ~TooltipView() override = default;

  // views:View:
  void OnPaint(gfx::Canvas* canvas) override {
    OnPaintBackground(canvas);
    gfx::Size text_size = size();
    gfx::Insets insets = GetInsets();
    text_size.Enlarge(-insets.width(), -insets.height());
    render_text_->SetDisplayRect(gfx::Rect(text_size));
    canvas->Save();
    canvas->Translate(gfx::Vector2d(insets.left(), insets.top()));
    render_text_->Draw(canvas);
    canvas->Restore();
    OnPaintBorder(canvas);
  }

  gfx::Size CalculatePreferredSize() const override {
    gfx::Size view_size = render_text_->GetStringSize();
    gfx::Insets insets = GetInsets();
    view_size.Enlarge(insets.width(), insets.height());
    return view_size;
  }

  void SetText(const std::u16string& text) {
    render_text_->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);

    // Replace tabs with whitespace to avoid placeholder character rendering
    // where previously it did not. crbug.com/993100
    std::u16string newText(text);
    base::ReplaceChars(newText, u"\t", u"        ", &newText);
    render_text_->SetText(newText);
    SchedulePaint();
  }

  void SetForegroundColor(SkColor color) { render_text_->SetColor(color); }

  void SetBackgroundColor(SkColor background_color, SkColor border_color) {
    if (CanUseTranslucentTooltipWidget()) {
      // Corner radius of tooltip background.
      const float kTooltipCornerRadius = 2.f;
      SetBackground(views::CreateBackgroundFromPainter(
          views::Painter::CreateRoundRectWith1PxBorderPainter(
              background_color, border_color, kTooltipCornerRadius)));
    } else {
      SetBackground(views::CreateSolidBackground(background_color));

      SetBorder(views::CreatePaddedBorder(
          views::CreateSolidBorder(1, border_color),
          gfx::Insets::TLBR(kVerticalPaddingTop - 1, kHorizontalPadding - 1,
                            kVerticalPaddingBottom - 1,
                            kHorizontalPadding - 1)));
    }

    // Force the text color to be readable when |background_color| is not
    // opaque.
    render_text_->set_subpixel_rendering_suppressed(
        SkColorGetA(background_color) != 0xFF);
  }

  void SetMaxWidth(int width) {
    max_width_ = width;
    ResetDisplayRect();
  }

  gfx::RenderText* render_text() { return render_text_.get(); }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kTooltip;
    node_data->SetNameChecked(render_text_->GetDisplayText());
  }

 private:
  void ResetDisplayRect() {
    render_text_->SetDisplayRect(gfx::Rect(0, 0, max_width_, 100000));
  }

  std::unique_ptr<gfx::RenderText> render_text_;
  int max_width_ = 0;
};

BEGIN_METADATA(TooltipView, views::View)
END_METADATA

}  // namespace

namespace views::corewm {

// static
const char TooltipAura::kWidgetName[] = "TooltipAura";

TooltipAura::TooltipAura() = default;

TooltipAura::~TooltipAura() {
  DestroyWidget();
  CHECK(!IsInObserverList());
}

void TooltipAura::AddObserver(wm::TooltipObserver* observer) {
  observers_.AddObserver(observer);
}

void TooltipAura::RemoveObserver(wm::TooltipObserver* observer) {
  observers_.RemoveObserver(observer);
}

class TooltipAura::TooltipWidget : public Widget {
 public:
  TooltipWidget() = default;
  ~TooltipWidget() override = default;

  TooltipView* GetTooltipView() { return tooltip_view_; }

  void SetTooltipView(std::unique_ptr<TooltipView> tooltip_view) {
    tooltip_view_ = SetContentsView(std::move(tooltip_view));
  }

 private:
  raw_ptr<TooltipView> tooltip_view_ = nullptr;
};

gfx::RenderText* TooltipAura::GetRenderTextForTest() {
  DCHECK(widget_);
  return widget_->GetTooltipView()->render_text();
}

void TooltipAura::GetAccessibleNodeDataForTest(ui::AXNodeData* node_data) {
  DCHECK(widget_);
  widget_->GetTooltipView()->GetAccessibleNodeData(node_data);
}

gfx::Rect TooltipAura::GetTooltipBounds(const gfx::Size& tooltip_size,
                                        const gfx::Point& anchor_point,
                                        const TooltipTrigger trigger,
                                        ui::OwnedWindowAnchor* anchor) {
  gfx::Rect tooltip_rect(anchor_point, tooltip_size);
  // When the tooltip is showing up as a result of a cursor event, the tooltip
  // needs to show up at the bottom-right corner of the cursor. When it's not,
  // it has to be centered with the anchor point with pass it.
  switch (trigger) {
    case TooltipTrigger::kKeyboard:
      tooltip_rect.Offset(-tooltip_size.width() / 2, 0);
      break;
    case TooltipTrigger::kCursor: {
      const int x_offset =
          base::i18n::IsRTL() ? -tooltip_size.width() : kCursorOffsetX;
      tooltip_rect.Offset(x_offset, kCursorOffsetY);
      break;
    }
  }

  anchor->anchor_gravity = ui::OwnedWindowAnchorGravity::kBottomRight;
  anchor->anchor_position = trigger == TooltipTrigger::kCursor
                                ? ui::OwnedWindowAnchorPosition::kBottomRight
                                : ui::OwnedWindowAnchorPosition::kTop;
  anchor->constraint_adjustment =
      ui::OwnedWindowConstraintAdjustment::kAdjustmentSlideX |
      ui::OwnedWindowConstraintAdjustment::kAdjustmentSlideY |
      ui::OwnedWindowConstraintAdjustment::kAdjustmentFlipY;
  // TODO(msisov): handle RTL.
  anchor->anchor_rect =
      gfx::Rect(anchor_point, {kCursorOffsetX, kCursorOffsetY});

  display::Screen* screen = display::Screen::GetScreen();
  gfx::Rect display_bounds(
      screen->GetDisplayNearestPoint(anchor_point).bounds());

  // If tooltip is out of bounds on the x axis, we simply shift it
  // horizontally by the offset variation.
  if (tooltip_rect.x() < display_bounds.x()) {
    int delta = tooltip_rect.x() - display_bounds.x();
    tooltip_rect.Offset(delta, 0);
  }
  if (tooltip_rect.right() > display_bounds.right()) {
    int delta = tooltip_rect.right() - display_bounds.right();
    tooltip_rect.Offset(-delta, 0);
  }

  // If tooltip is out of bounds on the y axis, we flip it to appear above the
  // mouse cursor instead of below.
  if (tooltip_rect.bottom() > display_bounds.bottom())
    tooltip_rect.set_y(anchor_point.y() - tooltip_size.height());

  tooltip_rect.AdjustToFit(display_bounds);
  return tooltip_rect;
}

void TooltipAura::CreateTooltipWidget(const gfx::Rect& bounds,
                                      const ui::OwnedWindowAnchor& anchor) {
  DCHECK(!widget_);
  DCHECK(tooltip_window_);
  widget_ = new TooltipWidget;
  views::Widget::InitParams params;
  // For aura, since we set the type to TYPE_TOOLTIP, the widget will get
  // auto-parented to the right container.
  params.type = views::Widget::InitParams::TYPE_TOOLTIP;
  params.context = tooltip_window_;
  DCHECK(params.context);
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.accept_events = false;
  params.bounds = bounds;
  if (CanUseTranslucentTooltipWidget())
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  // Use software compositing to avoid using unnecessary hardware resources
  // which just amount to overkill for this UI.
  params.force_software_compositing = true;
  params.name = kWidgetName;

  params.init_properties_container.SetProperty(aura::client::kOwnedWindowAnchor,
                                               anchor);

  widget_->Init(std::move(params));
}

void TooltipAura::DestroyWidget() {
  if (widget_) {
    widget_->RemoveObserver(this);
    widget_->Close();
    widget_ = nullptr;
  }
}

int TooltipAura::GetMaxWidth(const gfx::Point& location) const {
  display::Screen* screen = display::Screen::GetScreen();
  gfx::Rect display_bounds(screen->GetDisplayNearestPoint(location).bounds());
  return std::min(kTooltipMaxWidth, (display_bounds.width() + 1) / 2);
}

void TooltipAura::Update(aura::Window* window,
                         const std::u16string& tooltip_text,
                         const gfx::Point& position,
                         const TooltipTrigger trigger) {
  // Hide() must be called before showing the next tooltip.  See also the
  // comment in Hide().
  DCHECK(!widget_);

  tooltip_window_ = window;

  auto new_tooltip_view = std::make_unique<TooltipView>();
  gfx::Point anchor_point =
      position + window->GetBoundsInScreen().OffsetFromOrigin();
  new_tooltip_view->SetMaxWidth(GetMaxWidth(anchor_point));
  new_tooltip_view->SetText(tooltip_text);
  ui::OwnedWindowAnchor anchor;
  auto bounds = GetTooltipBounds(new_tooltip_view->GetPreferredSize(),
                                 anchor_point, trigger, &anchor);
  CreateTooltipWidget(bounds, anchor);
  widget_->SetTooltipView(std::move(new_tooltip_view));
  widget_->AddObserver(this);

  const ui::ColorProvider* color_provider = widget_->GetColorProvider();
  auto background_color = color_provider->GetColor(ui::kColorTooltipBackground);
  if (!CanUseTranslucentTooltipWidget()) {
    background_color = color_utils::GetResultingPaintColor(
        background_color, color_provider->GetColor(ui::kColorWindowBackground));
  }
  auto foreground_color = color_provider->GetColor(ui::kColorTooltipForeground);
  if (!CanUseTranslucentTooltipWidget())
    foreground_color =
        color_utils::GetResultingPaintColor(foreground_color, background_color);
  TooltipView* tooltip_view = widget_->GetTooltipView();
  tooltip_view->SetBackgroundColor(background_color, foreground_color);
  tooltip_view->SetForegroundColor(foreground_color);
}

void TooltipAura::Show() {
  if (widget_) {
    widget_->Show();

    if (!base::FeatureList::IsEnabled(views::features::kWidgetLayering))
      widget_->StackAtTop();

    widget_->GetTooltipView()->NotifyAccessibilityEvent(
        ax::mojom::Event::kTooltipOpened, true);

    // Add distance between `tooltip_window_` and its toplevel window to bounds
    // to pass via NotifyTooltipShown() since client will use this bounds as
    // relative to wayland toplevel window.
    // TODO(crbug.com/1385219): Use `tooltip_window_` instead of its toplevel
    // window when WaylandWindow on ozone becomes available.
    aura::Window* toplevel_window = tooltip_window_->GetToplevelWindow();
    // `tooltip_window_`'s toplevel window may be null for testing.
    if (toplevel_window) {
      gfx::Rect bounds = widget_->GetWindowBoundsInScreen();
      aura::Window::ConvertRectToTarget(tooltip_window_, toplevel_window,
                                        &bounds);
      for (auto& observer : observers_) {
        observer.OnTooltipShown(
            toplevel_window, widget_->GetTooltipView()->render_text()->text(),
            bounds);
      }
    }
  }
}

void TooltipAura::Hide() {
  if (widget_) {
    // If we simply hide the widget there's a chance to briefly show outdated
    // information on the next Show() because the text isn't updated until
    // OnPaint() which happens asynchronously after the Show(). As a result,
    // we can just destroy the widget and create a new one each time which
    // guarantees we never show outdated information.
    // TODO(http://crbug.com/998280): Figure out why the old content is
    // displayed despite the size change.
    widget_->GetTooltipView()->NotifyAccessibilityEvent(
        ax::mojom::Event::kTooltipClosed, true);

    // TODO(crbug.com/1385219): Use `tooltip_window_` instead of its toplevel
    // window when WaylandWindow on ozone becomes available.
    aura::Window* toplevel_window = tooltip_window_->GetToplevelWindow();
    // `tooltip_window_`'s toplevel window may be null for testing.
    if (toplevel_window) {
      for (auto& observer : observers_)
        observer.OnTooltipHidden(toplevel_window);
    }
    DestroyWidget();
  }
  tooltip_window_ = nullptr;
}

bool TooltipAura::IsVisible() {
  return widget_ && widget_->IsVisible();
}

void TooltipAura::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget_, widget);
  if (widget_)
    widget_->RemoveObserver(this);
  widget_ = nullptr;
  tooltip_window_ = nullptr;
}

}  // namespace views::corewm
