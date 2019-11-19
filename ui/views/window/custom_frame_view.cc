// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/custom_frame_view.h"

#include <algorithm>
#include <vector>

#include "base/containers/adapters.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/client_view.h"
#include "ui/views/window/frame_background.h"
#include "ui/views/window/window_button_order_provider.h"
#include "ui/views/window/window_resources.h"
#include "ui/views/window/window_shape.h"

#if defined(OS_WIN)
#include "ui/display/win/screen_win.h"
#include "ui/gfx/system_fonts_win.h"
#endif

namespace views {

namespace {

// The frame border is only visible in restored mode and is hardcoded to 4 px on
// each side regardless of the system window border size.
constexpr int kFrameBorderThickness = 4;
// In the window corners, the resize areas don't actually expand bigger, but the
// 16 px at the end of each edge triggers diagonal resizing.
constexpr int kResizeAreaCornerSize = 16;
// The titlebar never shrinks too short to show the caption button plus some
// padding below it.
constexpr int kCaptionButtonHeightWithPadding = 19;
// The titlebar has a 2 px 3D edge along the top and bottom.
constexpr int kTitlebarTopAndBottomEdgeThickness = 2;
// The icon is inset 2 px from the left frame border.
constexpr int kIconLeftSpacing = 2;
// The space between the window icon and the title text.
constexpr int kTitleIconOffsetX = 4;
// The space between the title text and the caption buttons.
constexpr int kTitleCaptionSpacing = 5;

#if defined(OS_CHROMEOS)
// Chrome OS uses a dark gray.
constexpr SkColor kDefaultColorFrame = SkColorSetRGB(109, 109, 109);
constexpr SkColor kDefaultColorFrameInactive = SkColorSetRGB(176, 176, 176);
#else
// Windows and Linux use a blue.
constexpr SkColor kDefaultColorFrame = SkColorSetRGB(66, 116, 201);
constexpr SkColor kDefaultColorFrameInactive = SkColorSetRGB(161, 182, 228);
#endif

void LayoutButton(ImageButton* button, const gfx::Rect& bounds) {
  button->SetVisible(true);
  button->SetImageVerticalAlignment(ImageButton::ALIGN_BOTTOM);
  button->SetBoundsRect(bounds);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// CustomFrameView, public:

CustomFrameView::CustomFrameView() : frame_background_(new FrameBackground()) {}

CustomFrameView::~CustomFrameView() = default;

void CustomFrameView::Init(Widget* frame) {
  frame_ = frame;

  close_button_ = InitWindowCaptionButton(IDS_APP_ACCNAME_CLOSE,
      IDR_CLOSE, IDR_CLOSE_H, IDR_CLOSE_P);
  minimize_button_ = InitWindowCaptionButton(IDS_APP_ACCNAME_MINIMIZE,
      IDR_MINIMIZE, IDR_MINIMIZE_H, IDR_MINIMIZE_P);
  maximize_button_ = InitWindowCaptionButton(IDS_APP_ACCNAME_MAXIMIZE,
      IDR_MAXIMIZE, IDR_MAXIMIZE_H, IDR_MAXIMIZE_P);
  restore_button_ = InitWindowCaptionButton(IDS_APP_ACCNAME_RESTORE,
      IDR_RESTORE, IDR_RESTORE_H, IDR_RESTORE_P);

  if (frame_->widget_delegate()->ShouldShowWindowIcon()) {
    window_icon_ = new ImageButton(this);
    AddChildView(window_icon_);
  }
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameView, NonClientFrameView implementation:

gfx::Rect CustomFrameView::GetBoundsForClientView() const {
  return client_view_bounds_;
}

gfx::Rect CustomFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  return gfx::Rect(client_bounds.x() - border_thickness,
                   client_bounds.y() - top_height,
                   client_bounds.width() + (2 * border_thickness),
                   client_bounds.height() + top_height + border_thickness);
}

int CustomFrameView::NonClientHitTest(const gfx::Point& point) {
  // Sanity check.
  if (!bounds().Contains(point))
    return HTNOWHERE;

  int frame_component = frame_->client_view()->NonClientHitTest(point);

  // See if we're in the sysmenu region.  (We check the ClientView first to be
  // consistent with OpaqueBrowserFrameView; it's not really necessary here.)
  gfx::Rect sysmenu_rect(IconBounds());
  // In maximized mode we extend the rect to the screen corner to take advantage
  // of Fitts' Law.
  if (frame_->IsMaximized())
    sysmenu_rect.SetRect(0, 0, sysmenu_rect.right(), sysmenu_rect.bottom());
  sysmenu_rect.set_x(GetMirroredXForRect(sysmenu_rect));
  if (sysmenu_rect.Contains(point))
    return (frame_component == HTCLIENT) ? HTCLIENT : HTSYSMENU;

  if (frame_component != HTNOWHERE)
    return frame_component;

  // Then see if the point is within any of the window controls.
  if (close_button_->GetMirroredBounds().Contains(point))
    return HTCLOSE;
  if (restore_button_->GetMirroredBounds().Contains(point))
    return HTMAXBUTTON;
  if (maximize_button_->GetMirroredBounds().Contains(point))
    return HTMAXBUTTON;
  if (minimize_button_->GetMirroredBounds().Contains(point))
    return HTMINBUTTON;
  if (window_icon_ && window_icon_->GetMirroredBounds().Contains(point))
    return HTSYSMENU;

  int window_component = GetHTComponentForFrame(point, FrameBorderThickness(),
      NonClientBorderThickness(), kResizeAreaCornerSize, kResizeAreaCornerSize,
      frame_->widget_delegate()->CanResize());
  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

void CustomFrameView::GetWindowMask(const gfx::Size& size,
                                    SkPath* window_mask) {
  DCHECK(window_mask);
  if (frame_->IsMaximized() || !ShouldShowTitleBarAndBorder())
    return;

  GetDefaultWindowMask(size, frame_->GetCompositor()->device_scale_factor(),
                       window_mask);
}

void CustomFrameView::ResetWindowControls() {
  restore_button_->SetState(Button::STATE_NORMAL);
  minimize_button_->SetState(Button::STATE_NORMAL);
  maximize_button_->SetState(Button::STATE_NORMAL);
  // The close button isn't affected by this constraint.
}

void CustomFrameView::UpdateWindowIcon() {
  if (window_icon_)
    window_icon_->SchedulePaint();
}

void CustomFrameView::UpdateWindowTitle() {
  if (frame_->widget_delegate()->ShouldShowWindowTitle() &&
      // If this is still unset, we haven't laid out window controls yet.
      maximum_title_bar_x_ > -1) {
    LayoutTitleBar();
    SchedulePaintInRect(title_bounds_);
  }
}

void CustomFrameView::SizeConstraintsChanged() {
  ResetWindowControls();
  LayoutWindowControls();
}

void CustomFrameView::PaintAsActiveChanged(bool active) {
  SchedulePaint();
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameView, View overrides:

void CustomFrameView::OnPaint(gfx::Canvas* canvas) {
  if (!ShouldShowTitleBarAndBorder())
    return;

  frame_background_->set_frame_color(GetFrameColor());
  frame_background_->set_use_custom_frame(true);
  frame_background_->set_is_active(ShouldPaintAsActive());
  frame_background_->set_incognito(false);
  const gfx::ImageSkia frame_image = GetFrameImage();
  frame_background_->set_theme_image(frame_image);
  frame_background_->set_top_area_height(frame_image.height());

  if (frame_->IsMaximized())
    PaintMaximizedFrameBorder(canvas);
  else
    PaintRestoredFrameBorder(canvas);
  PaintTitleBar(canvas);
  if (ShouldShowClientEdge())
    PaintRestoredClientEdge(canvas);
}

void CustomFrameView::Layout() {
  if (ShouldShowTitleBarAndBorder()) {
    LayoutWindowControls();
    LayoutTitleBar();
  }

  LayoutClientView();
}

gfx::Size CustomFrameView::CalculatePreferredSize() const {
  return frame_->non_client_view()->GetWindowBoundsForClientBounds(
      gfx::Rect(frame_->client_view()->GetPreferredSize())).size();
}

gfx::Size CustomFrameView::GetMinimumSize() const {
  return frame_->non_client_view()->GetWindowBoundsForClientBounds(
      gfx::Rect(frame_->client_view()->GetMinimumSize())).size();
}

gfx::Size CustomFrameView::GetMaximumSize() const {
  gfx::Size max_size = frame_->client_view()->GetMaximumSize();
  gfx::Size converted_size =
      frame_->non_client_view()->GetWindowBoundsForClientBounds(
          gfx::Rect(max_size)).size();
  return gfx::Size(max_size.width() == 0 ? 0 : converted_size.width(),
                   max_size.height() == 0 ? 0 : converted_size.height());
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameView, ButtonListener implementation:

void CustomFrameView::ButtonPressed(Button* sender, const ui::Event& event) {
  if (sender == close_button_)
    frame_->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
  else if (sender == minimize_button_)
    frame_->Minimize();
  else if (sender == maximize_button_)
    frame_->Maximize();
  else if (sender == restore_button_)
    frame_->Restore();
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameView, private:

int CustomFrameView::FrameBorderThickness() const {
  return frame_->IsMaximized() ? 0 : kFrameBorderThickness;
}

int CustomFrameView::NonClientBorderThickness() const {
  // In maximized mode, we don't show a client edge.
  return FrameBorderThickness() +
      (ShouldShowClientEdge() ? kClientEdgeThickness : 0);
}

int CustomFrameView::NonClientTopBorderHeight() const {
  return std::max(FrameBorderThickness() + IconSize(),
                  CaptionButtonY() + kCaptionButtonHeightWithPadding) +
      TitlebarBottomThickness();
}

int CustomFrameView::CaptionButtonY() const {
  // Maximized buttons start at window top so that even if their images aren't
  // drawn flush with the screen edge, they still obey Fitts' Law.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  return FrameBorderThickness();
#else
  return frame_->IsMaximized() ? FrameBorderThickness() : kFrameShadowThickness;
#endif
}

int CustomFrameView::TitlebarBottomThickness() const {
  return kTitlebarTopAndBottomEdgeThickness +
      (ShouldShowClientEdge() ? kClientEdgeThickness : 0);
}

int CustomFrameView::IconSize() const {
#if defined(OS_WIN)
  // This metric scales up if either the titlebar height or the titlebar font
  // size are increased.
  return display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYSMICON);
#else
  // The icon never shrinks below 16 px on a side.
  constexpr int kIconMinimumSize = 16;
  return std::max(GetWindowTitleFontList().GetHeight(), kIconMinimumSize);
#endif
}

gfx::Rect CustomFrameView::IconBounds() const {
  int size = IconSize();
  int frame_thickness = FrameBorderThickness();
  // Our frame border has a different "3D look" than Windows'.  Theirs has a
  // more complex gradient on the top that they push their icon/title below;
  // then the maximized window cuts this off and the icon/title are centered
  // in the remaining space.  Because the apparent shape of our border is
  // simpler, using the same positioning makes things look slightly uncentered
  // with restored windows, so when the window is restored, instead of
  // calculating the remaining space from below the frame border, we calculate
  // from below the 3D edge.
  int unavailable_px_at_top = frame_->IsMaximized() ?
      frame_thickness : kTitlebarTopAndBottomEdgeThickness;
  // When the icon is shorter than the minimum space we reserve for the caption
  // button, we vertically center it.  We want to bias rounding to put extra
  // space above the icon, since the 3D edge (+ client edge, for restored
  // windows) below looks (to the eye) more like additional space than does the
  // 3D edge (or nothing at all, for maximized windows) above; hence the +1.
  int y = unavailable_px_at_top + (NonClientTopBorderHeight() -
      unavailable_px_at_top - size - TitlebarBottomThickness() + 1) / 2;
  return gfx::Rect(frame_thickness + kIconLeftSpacing + minimum_title_bar_x_,
                   y, size, size);
}

bool CustomFrameView::ShouldShowTitleBarAndBorder() const {
  return !frame_->IsFullscreen() &&
         !ViewsDelegate::GetInstance()->WindowManagerProvidesTitleBar(
             frame_->IsMaximized());
}

bool CustomFrameView::ShouldShowClientEdge() const {
  return !frame_->IsMaximized() && ShouldShowTitleBarAndBorder();
}

void CustomFrameView::PaintRestoredFrameBorder(gfx::Canvas* canvas) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  frame_background_->SetCornerImages(
      rb.GetImageNamed(IDR_WINDOW_TOP_LEFT_CORNER).ToImageSkia(),
      rb.GetImageNamed(IDR_WINDOW_TOP_RIGHT_CORNER).ToImageSkia(),
      rb.GetImageNamed(IDR_WINDOW_BOTTOM_LEFT_CORNER).ToImageSkia(),
      rb.GetImageNamed(IDR_WINDOW_BOTTOM_RIGHT_CORNER).ToImageSkia());
  frame_background_->SetSideImages(
      rb.GetImageNamed(IDR_WINDOW_LEFT_SIDE).ToImageSkia(),
      rb.GetImageNamed(IDR_WINDOW_TOP_CENTER).ToImageSkia(),
      rb.GetImageNamed(IDR_WINDOW_RIGHT_SIDE).ToImageSkia(),
      rb.GetImageNamed(IDR_WINDOW_BOTTOM_CENTER).ToImageSkia());

  frame_background_->PaintRestored(canvas, this);
}

void CustomFrameView::PaintMaximizedFrameBorder(gfx::Canvas* canvas) {
  frame_background_->PaintMaximized(canvas, this);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  // TODO(jamescook): Migrate this into FrameBackground.
  // The bottom of the titlebar actually comes from the top of the Client Edge
  // graphic, with the actual client edge clipped off the bottom.
  const gfx::ImageSkia* titlebar_bottom = rb.GetImageNamed(
      IDR_APP_TOP_CENTER).ToImageSkia();
  int edge_height = titlebar_bottom->height() -
      (ShouldShowClientEdge() ? kClientEdgeThickness : 0);
  canvas->TileImageInt(*titlebar_bottom, 0,
      frame_->client_view()->y() - edge_height, width(), edge_height);
}

void CustomFrameView::PaintTitleBar(gfx::Canvas* canvas) {
  WidgetDelegate* delegate = frame_->widget_delegate();

  // It seems like in some conditions we can be asked to paint after the window
  // that contains us is WM_DESTROYed. At this point, our delegate is NULL. The
  // correct long term fix may be to shut down the RootView in WM_DESTROY.
  if (!delegate || !delegate->ShouldShowWindowTitle())
    return;

  gfx::Rect rect = title_bounds_;
  rect.set_x(GetMirroredXForRect(title_bounds_));
  canvas->DrawStringRect(delegate->GetWindowTitle(), GetWindowTitleFontList(),
                         SK_ColorWHITE, rect);
}

void CustomFrameView::PaintRestoredClientEdge(gfx::Canvas* canvas) {
  gfx::Rect client_area_bounds = frame_->client_view()->bounds();
  // The shadows have a 1 pixel gap on the inside, so draw them 1 pixel inwards.
  gfx::Rect shadowed_area_bounds = client_area_bounds;
  shadowed_area_bounds.Inset(gfx::Insets(1, 1, 1, 1));
  int shadowed_area_top = shadowed_area_bounds.y();

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  // Top: left, center, right sides.
  const gfx::ImageSkia* top_left = rb.GetImageSkiaNamed(IDR_APP_TOP_LEFT);
  const gfx::ImageSkia* top_center = rb.GetImageSkiaNamed(IDR_APP_TOP_CENTER);
  const gfx::ImageSkia* top_right = rb.GetImageSkiaNamed(IDR_APP_TOP_RIGHT);
  int top_edge_y = shadowed_area_top - top_center->height();
  canvas->DrawImageInt(*top_left,
                       shadowed_area_bounds.x() - top_left->width(),
                       top_edge_y);
  canvas->TileImageInt(*top_center,
                       shadowed_area_bounds.x(),
                       top_edge_y,
                       shadowed_area_bounds.width(),
                       top_center->height());
  canvas->DrawImageInt(*top_right, shadowed_area_bounds.right(), top_edge_y);

  // Right side.
  const gfx::ImageSkia* right = rb.GetImageSkiaNamed(IDR_CONTENT_RIGHT_SIDE);
  int shadowed_area_bottom =
      std::max(shadowed_area_top, shadowed_area_bounds.bottom());
  int shadowed_area_height = shadowed_area_bottom - shadowed_area_top;
  canvas->TileImageInt(*right,
                       shadowed_area_bounds.right(),
                       shadowed_area_top,
                       right->width(),
                       shadowed_area_height);

  // Bottom: left, center, right sides.
  const gfx::ImageSkia* bottom_left =
      rb.GetImageSkiaNamed(IDR_CONTENT_BOTTOM_LEFT_CORNER);
  const gfx::ImageSkia* bottom_center =
      rb.GetImageSkiaNamed(IDR_CONTENT_BOTTOM_CENTER);
  const gfx::ImageSkia* bottom_right =
      rb.GetImageSkiaNamed(IDR_CONTENT_BOTTOM_RIGHT_CORNER);

  canvas->DrawImageInt(*bottom_left,
                       shadowed_area_bounds.x() - bottom_left->width(),
                       shadowed_area_bottom);

  canvas->TileImageInt(*bottom_center,
                       shadowed_area_bounds.x(),
                       shadowed_area_bottom,
                       shadowed_area_bounds.width(),
                       bottom_right->height());

  canvas->DrawImageInt(*bottom_right,
                       shadowed_area_bounds.right(),
                       shadowed_area_bottom);
  // Left side.
  const gfx::ImageSkia* left = rb.GetImageSkiaNamed(IDR_CONTENT_LEFT_SIDE);
  canvas->TileImageInt(*left,
                       shadowed_area_bounds.x() - left->width(),
                       shadowed_area_top,
                       left->width(),
                       shadowed_area_height);
}

SkColor CustomFrameView::GetFrameColor() const {
  return frame_->IsActive() ? kDefaultColorFrame : kDefaultColorFrameInactive;
}

gfx::ImageSkia CustomFrameView::GetFrameImage() const {
  return *ui::ResourceBundle::GetSharedInstance()
              .GetImageNamed(frame_->IsActive() ? IDR_FRAME
                                                : IDR_FRAME_INACTIVE)
              .ToImageSkia();
}

void CustomFrameView::LayoutWindowControls() {
  minimum_title_bar_x_ = 0;
  maximum_title_bar_x_ = width();

  if (bounds().IsEmpty())
    return;

  int caption_y = CaptionButtonY();
  bool is_maximized = frame_->IsMaximized();
  // There should always be the same number of non-shadow pixels visible to the
  // side of the caption buttons.  In maximized mode we extend the edge button
  // to the screen corner to obey Fitts' Law.
  int extra_width = is_maximized ?
      (kFrameBorderThickness - kFrameShadowThickness) : 0;
  int next_button_x = FrameBorderThickness();

  bool is_restored = !is_maximized && !frame_->IsMinimized();
  ImageButton* invisible_button = is_restored ? restore_button_
                                              : maximize_button_;
  invisible_button->SetVisible(false);

  WindowButtonOrderProvider* button_order =
      WindowButtonOrderProvider::GetInstance();
  const std::vector<views::FrameButton>& leading_buttons =
      button_order->leading_buttons();
  const std::vector<views::FrameButton>& trailing_buttons =
      button_order->trailing_buttons();

  ImageButton* button = nullptr;
  for (auto frame_button : leading_buttons) {
    button = GetImageButton(frame_button);
    if (!button)
      continue;
    gfx::Rect target_bounds(gfx::Point(next_button_x, caption_y),
                            button->GetPreferredSize());
    if (frame_button == leading_buttons.front())
      target_bounds.set_width(target_bounds.width() + extra_width);
    LayoutButton(button, target_bounds);
    next_button_x += button->width();
    minimum_title_bar_x_ = std::min(width(), next_button_x);
  }

  // Trailing buttions are laid out in a RTL fashion
  next_button_x = width() - FrameBorderThickness();
  for (auto frame_button : base::Reversed(trailing_buttons)) {
    button = GetImageButton(frame_button);
    if (!button)
      continue;
    gfx::Rect target_bounds(gfx::Point(next_button_x, caption_y),
                            button->GetPreferredSize());
    if (frame_button == trailing_buttons.back())
      target_bounds.set_width(target_bounds.width() + extra_width);
    target_bounds.Offset(-target_bounds.width(), 0);
    LayoutButton(button, target_bounds);
    next_button_x = button->x();
    maximum_title_bar_x_ = std::max(minimum_title_bar_x_, next_button_x);
  }
}

void CustomFrameView::LayoutTitleBar() {
  DCHECK_GE(maximum_title_bar_x_, 0);
  // The window title position is calculated based on the icon position, even
  // when there is no icon.
  gfx::Rect icon_bounds(IconBounds());
  bool show_window_icon = window_icon_ != nullptr;
  if (show_window_icon)
    window_icon_->SetBoundsRect(icon_bounds);

  if (!frame_->widget_delegate()->ShouldShowWindowTitle())
    return;

  // The offset between the window left edge and the title text.
  int title_x = show_window_icon ? icon_bounds.right() + kTitleIconOffsetX
                                 : icon_bounds.x();
  int title_height = GetWindowTitleFontList().GetHeight();
  // We bias the title position so that when the difference between the icon and
  // title heights is odd, the extra pixel of the title is above the vertical
  // midline rather than below.  This compensates for how the icon is already
  // biased downwards (see IconBounds()) and helps prevent descenders on the
  // title from overlapping the 3D edge at the bottom of the titlebar.
  title_bounds_.SetRect(title_x,
      icon_bounds.y() + ((icon_bounds.height() - title_height - 1) / 2),
      std::max(0, maximum_title_bar_x_ - kTitleCaptionSpacing -
      title_x), title_height);
}

void CustomFrameView::LayoutClientView() {
  if (!ShouldShowTitleBarAndBorder()) {
    client_view_bounds_ = bounds();
    return;
  }

  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  client_view_bounds_.SetRect(border_thickness, top_height,
      std::max(0, width() - (2 * border_thickness)),
      std::max(0, height() - top_height - border_thickness));
}

ImageButton* CustomFrameView::InitWindowCaptionButton(
    int accessibility_string_id,
    int normal_image_id,
    int hot_image_id,
    int pushed_image_id) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  ImageButton* button = new ImageButton(this);
  button->SetAccessibleName(l10n_util::GetStringUTF16(accessibility_string_id));
  button->SetImage(Button::STATE_NORMAL,
                   rb.GetImageNamed(normal_image_id).ToImageSkia());
  button->SetImage(Button::STATE_HOVERED,
                   rb.GetImageNamed(hot_image_id).ToImageSkia());
  button->SetImage(Button::STATE_PRESSED,
                   rb.GetImageNamed(pushed_image_id).ToImageSkia());
  AddChildView(button);
  return button;
}

ImageButton* CustomFrameView::GetImageButton(views::FrameButton frame_button) {
  ImageButton* button = nullptr;
  switch (frame_button) {
    case views::FrameButton::kMinimize: {
      button = minimize_button_;
      // If we should not show the minimize button, then we return NULL as we
      // don't want this button to become visible and to be laid out.
      bool should_show = frame_->widget_delegate()->CanMinimize();
      button->SetVisible(should_show);
      if (!should_show)
        return nullptr;

      break;
    }
    case views::FrameButton::kMaximize: {
      bool is_restored = !frame_->IsMaximized() && !frame_->IsMinimized();
      button = is_restored ? maximize_button_ : restore_button_;
      // If we should not show the maximize/restore button, then we return
      // NULL as we don't want this button to become visible and to be laid
      // out.
      bool should_show = frame_->widget_delegate()->CanMaximize();
      button->SetVisible(should_show);
      if (!should_show)
        return nullptr;

      break;
    }
    case views::FrameButton::kClose: {
      button = close_button_;
      break;
    }
  }
  return button;
}

// static
gfx::FontList CustomFrameView::GetWindowTitleFontList() {
#if defined(OS_WIN)
  return gfx::FontList(gfx::win::GetSystemFont(gfx::win::SystemFont::kCaption));
#else
  return gfx::FontList();
#endif
}

}  // namespace views
