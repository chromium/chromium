// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/button_drag_utils.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/drag_utils.h"
#include "ui/views/paint_info.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace button_drag_utils {

// Maximum width of the link drag image in pixels.
static constexpr int kLinkDragImageMaxWidth = 150;

class ScopedWidget {
 public:
  explicit ScopedWidget(std::unique_ptr<views::Widget> widget)
      : widget_(std::move(widget)) {}

  ScopedWidget(const ScopedWidget&) = delete;
  ScopedWidget& operator=(const ScopedWidget&) = delete;

  ~ScopedWidget() = default;

  views::Widget* operator->() const { return widget_.get(); }
  views::Widget* get() const { return widget_.get(); }

 private:
  std::unique_ptr<views::Widget> widget_;
};

void SetURLAndDragImage(const GURL& url,
                        const std::u16string& title,
                        const gfx::ImageSkia& icon,
                        const gfx::Point* press_pt,
                        ui::OSExchangeData* data) {
  DCHECK(url.is_valid());
  DCHECK(data);
  data->SetURL(url, title);
  SetDragImage(url, title, icon, press_pt, data);
}

void SetDragImage(const GURL& url,
                  const std::u16string& title,
                  const gfx::ImageSkia& icon,
                  const gfx::Point* press_pt,
                  ui::OSExchangeData* data) {
  // Create a widget to render the drag image for us.
  ScopedWidget drag_widget(std::make_unique<views::Widget>());
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_DRAG);
  params.accept_events = false;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  drag_widget->Init(std::move(params));

  // Create a button to render the drag image for us.
  views::LabelButton* button =
      drag_widget->SetContentsView(std::make_unique<views::LabelButton>(
          views::Button::PressedCallback(),
          title.empty() ? base::UTF8ToUTF16(url.spec()) : title));
  button->SetTextSubpixelRenderingEnabled(false);
  const ui::ColorProvider* color_provider = drag_widget->GetColorProvider();
  button->SetTextColorId(views::Button::STATE_NORMAL,
                         ui::kColorTextfieldForeground);

  SkColor bg_color = color_provider->GetColor(ui::kColorTextfieldBackground);
  if (views::Widget::IsWindowCompositingSupported()) {
    button->SetTextShadows(gfx::ShadowValues(
        10, gfx::ShadowValue(gfx::Vector2d(0, 0), 2.0f, bg_color)));
  } else {
    button->SetBackground(views::CreateSolidBackground(bg_color));
    button->SetBorder(button->CreateDefaultBorder());
  }
  button->SetMaxSize(gfx::Size(kLinkDragImageMaxWidth, 0));
  if (icon.isNull()) {
    button->SetImageModel(views::Button::STATE_NORMAL,
                          ui::ImageModel::FromResourceId(IDR_DEFAULT_FAVICON));
  } else {
    button->SetImageModel(views::Button::STATE_NORMAL,
                          ui::ImageModel::FromImageSkia(icon));
  }

  gfx::Size size(button->GetPreferredSize({}));
  // drag_widget's size must be set to show the drag image in RTL.
  // However, on Windows, calling Widget::SetSize() resets
  // the LabelButton's bounds via OnNativeWidgetSizeChanged().
  // Therefore, call button->SetBoundsRect() after drag_widget->SetSize().
  drag_widget->SetSize(size);
  button->SetBoundsRect(gfx::Rect(size));

  gfx::Vector2d press_point;
  if (press_pt)
    press_point = press_pt->OffsetFromOrigin();
  else
    press_point = gfx::Vector2d(size.width() / 2, size.height() / 2);

  SkBitmap bitmap;
  float raster_scale = ScaleFactorForDragFromWidget(drag_widget.get());
  SkColor color = SK_ColorTRANSPARENT;
  button->Paint(views::PaintInfo::CreateRootPaintInfo(
      ui::CanvasPainter(&bitmap, size, raster_scale, color,
                        true /* is_pixel_canvas */)
          .context(),
      size));
  gfx::ImageSkia image = gfx::ImageSkia::CreateFromBitmap(bitmap, raster_scale);
  data->provider().SetDragImage(image, press_point);
}

}  // namespace button_drag_utils
