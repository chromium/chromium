// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/tooltip_aura.h"

#include <algorithm>
#include <utility>

#include "base/macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// Max visual tooltip width. If a tooltip is greater than this width, it will
// be wrapped.
constexpr int kTooltipMaxWidthPixels = 800;

// FIXME: get cursor offset from actual cursor size.
constexpr int kCursorOffsetX = 10;
constexpr int kCursorOffsetY = 15;

// Paddings
constexpr int kHorizontalPadding = 8;
constexpr int kVerticalPaddingTop = 4;
constexpr int kVerticalPaddingBottom = 5;

// TODO(varkha): Update if native widget can be transparent on Linux.
bool CanUseTranslucentTooltipWidget() {
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || defined(OS_WIN)
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
    SetBorder(views::CreateEmptyBorder(kVerticalPaddingTop, kHorizontalPadding,
                                       kVerticalPaddingBottom,
                                       kHorizontalPadding));

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
    gfx::Insets insets = border()->GetInsets();
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
    gfx::Insets insets = border()->GetInsets();
    view_size.Enlarge(insets.width(), insets.height());
    return view_size;
  }

  void SetText(const base::string16& text) {
    render_text_->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);

    // Replace tabs with whitespace to avoid placeholder character rendering
    // where previously it did not. crbug.com/993100
    base::string16 newText(text);
    base::ReplaceChars(newText, base::ASCIIToUTF16("\t"),
                       base::ASCIIToUTF16("        "), &newText);
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
          gfx::Insets(kVerticalPaddingTop - 1, kHorizontalPadding - 1,
                      kVerticalPaddingBottom - 1, kHorizontalPadding - 1)));
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

  gfx::RenderText* render_text_for_test() { return render_text_.get(); }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->SetName(render_text_->GetDisplayText());
    node_data->role = ax::mojom::Role::kTooltip;
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

namespace views {
namespace corewm {

TooltipAura::~TooltipAura() {
  DestroyWidget();
  CHECK(!IsInObserverList());
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
  TooltipView* tooltip_view_ = nullptr;
};

gfx::RenderText* TooltipAura::GetRenderTextForTest() {
  DCHECK(widget_);
  return widget_->GetTooltipView()->render_text_for_test();
}

void TooltipAura::GetAccessibleNodeDataForTest(ui::AXNodeData* node_data) {
  DCHECK(widget_);
  widget_->GetTooltipView()->GetAccessibleNodeData(node_data);
}

gfx::Rect TooltipAura::GetTooltipBounds(const gfx::Point& mouse_pos,
                                        const gfx::Size& tooltip_size) {
  gfx::Rect tooltip_rect(mouse_pos, tooltip_size);
  tooltip_rect.Offset(kCursorOffsetX, kCursorOffsetY);
  display::Screen* screen = display::Screen::GetScreen();
  gfx::Rect display_bounds(screen->GetDisplayNearestPoint(mouse_pos).bounds());

  // If tooltip is out of bounds on the x axis, we simply shift it
  // horizontally by the offset.
  if (tooltip_rect.right() > display_bounds.right()) {
    int h_offset = tooltip_rect.right() - display_bounds.right();
    tooltip_rect.Offset(-h_offset, 0);
  }

  // If tooltip is out of bounds on the y axis, we flip it to appear above the
  // mouse cursor instead of below.
  if (tooltip_rect.bottom() > display_bounds.bottom())
    tooltip_rect.set_y(mouse_pos.y() - tooltip_size.height());

  tooltip_rect.AdjustToFit(display_bounds);
  return tooltip_rect;
}

void TooltipAura::CreateTooltipWidget(const gfx::Rect& bounds) {
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
  return std::min(kTooltipMaxWidthPixels, (display_bounds.width() + 1) / 2);
}

void TooltipAura::SetText(aura::Window* window,
                          const base::string16& tooltip_text,
                          const gfx::Point& location) {
  tooltip_window_ = window;

  if (!widget_) {
    auto new_tooltip_view = std::make_unique<TooltipView>();
    new_tooltip_view->SetMaxWidth(GetMaxWidth(location));
    new_tooltip_view->SetText(tooltip_text);
    CreateTooltipWidget(
        GetTooltipBounds(location, new_tooltip_view->GetPreferredSize()));
    widget_->SetTooltipView(std::move(new_tooltip_view));
    widget_->AddObserver(this);
  } else {
    TooltipView* old_tooltip_view = widget_->GetTooltipView();
    old_tooltip_view->SetMaxWidth(GetMaxWidth(location));
    old_tooltip_view->SetText(tooltip_text);
    widget_->SetBounds(
        GetTooltipBounds(location, old_tooltip_view->GetPreferredSize()));
  }

  ui::NativeTheme* native_theme = widget_->GetNativeTheme();
  auto background_color =
      native_theme->GetSystemColor(ui::NativeTheme::kColorId_TooltipBackground);
  if (!CanUseTranslucentTooltipWidget()) {
    background_color = color_utils::GetResultingPaintColor(
        background_color, native_theme->GetSystemColor(
                              ui::NativeTheme::kColorId_WindowBackground));
  }
  auto foreground_color =
      native_theme->GetSystemColor(ui::NativeTheme::kColorId_TooltipText);
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
    widget_->StackAtTop();
    widget_->GetTooltipView()->NotifyAccessibilityEvent(
        ax::mojom::Event::kTooltipOpened, true);
  }
}

void TooltipAura::Hide() {
  tooltip_window_ = nullptr;
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
    DestroyWidget();
  }
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

}  // namespace corewm
}  // namespace views
