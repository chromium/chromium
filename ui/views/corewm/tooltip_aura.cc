// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/tooltip_aura.h"

#include "base/macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
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
#if (defined(OS_LINUX) && !defined(OS_CHROMEOS)) || defined(OS_WIN)
  return false;
#else
  return true;
#endif
}

// Creates a widget of type TYPE_TOOLTIP
views::Widget* CreateTooltipWidget(aura::Window* tooltip_window,
                                   const gfx::Rect& bounds) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  // For aura, since we set the type to TYPE_TOOLTIP, the widget will get
  // auto-parented to the right container.
  params.type = views::Widget::InitParams::TYPE_TOOLTIP;
  params.context = tooltip_window;
  DCHECK(params.context);
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.accept_events = false;
  params.bounds = bounds;
  if (CanUseTranslucentTooltipWidget())
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  // Use software compositing to avoid using unnecessary hardware resources
  // which just amount to overkill for this UI.
  params.force_software_compositing = true;
  widget->Init(std::move(params));
  return widget;
}

}  // namespace

namespace views {
namespace corewm {

// TODO(oshima): Consider to use views::Label.
class TooltipAura::TooltipView : public views::View {
 public:
  TooltipView() : render_text_(gfx::RenderText::CreateHarfBuzzInstance()) {
    SetBorder(CreateEmptyBorder(kVerticalPaddingTop, kHorizontalPadding,
                                kVerticalPaddingBottom, kHorizontalPadding));

    set_owned_by_client();
    render_text_->SetWordWrapBehavior(gfx::WRAP_LONG_WORDS);
    render_text_->SetMultiline(true);

    ResetDisplayRect();
  }

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

  const char* GetClassName() const override {
    return "TooltipView";
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

  void SetForegroundColor(SkColor color) {
    render_text_->SetColor(color);
  }

  void SetBackgroundColor(SkColor background_color) {
    if (CanUseTranslucentTooltipWidget()) {
      // Corner radius of tooltip background.
      const float kTooltipCornerRadius = 2.f;
      SetBackground(views::CreateBackgroundFromPainter(
          views::Painter::CreateSolidRoundRectPainter(background_color,
                                                      kTooltipCornerRadius)));
    } else {
      SetBackground(views::CreateSolidBackground(background_color));

      auto border_color =
          color_utils::GetColorWithMaxContrast(background_color);
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

  DISALLOW_COPY_AND_ASSIGN(TooltipView);
};

TooltipAura::TooltipAura() : tooltip_view_(new TooltipView) {}

TooltipAura::~TooltipAura() {
  DestroyWidget();
}

gfx::RenderText* TooltipAura::GetRenderTextForTest() {
  return tooltip_view_->render_text_for_test();
}

void TooltipAura::GetAccessibleNodeDataForTest(ui::AXNodeData* node_data) {
  tooltip_view_->GetAccessibleNodeData(node_data);
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
  tooltip_view_->SetMaxWidth(GetMaxWidth(location));
  tooltip_view_->SetText(tooltip_text);

  const gfx::Rect adjusted_bounds =
      GetTooltipBounds(location, tooltip_view_->GetPreferredSize());

  if (!widget_) {
    widget_ = CreateTooltipWidget(tooltip_window_, adjusted_bounds);
    widget_->SetContentsView(tooltip_view_.get());
    widget_->AddObserver(this);
  } else {
    widget_->SetBounds(adjusted_bounds);
  }

  ui::NativeTheme* native_theme = widget_->GetNativeTheme();
  auto background_color =
      native_theme->GetSystemColor(ui::NativeTheme::kColorId_TooltipBackground);
  if (!CanUseTranslucentTooltipWidget())
    background_color = SkColorSetA(background_color, 0xFF);
  tooltip_view_->SetBackgroundColor(background_color);
  auto foreground_color =
      native_theme->GetSystemColor(ui::NativeTheme::kColorId_TooltipText);
  if (!CanUseTranslucentTooltipWidget())
    foreground_color = SkColorSetA(foreground_color, 0xFF);
  tooltip_view_->SetForegroundColor(foreground_color);
}

void TooltipAura::Show() {
  if (widget_) {
    widget_->Show();
    widget_->StackAtTop();
    tooltip_view_->NotifyAccessibilityEvent(ax::mojom::Event::kTooltipOpened,
                                            true);
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
    // TODO: Figure out why the old content is displayed despite the size
    // change.
    // http://crbug.com/998280
    DestroyWidget();
    tooltip_view_->NotifyAccessibilityEvent(ax::mojom::Event::kTooltipClosed,
                                            true);
  }
}

bool TooltipAura::IsVisible() {
  return widget_ && widget_->IsVisible();
}

void TooltipAura::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget_, widget);
  widget_ = nullptr;
  tooltip_window_ = nullptr;
}

}  // namespace corewm
}  // namespace views
