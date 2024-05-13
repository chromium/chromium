// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/notification_example.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/views/examples/examples_color_id.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "url/gurl.h"

namespace {

gfx::Image CreateTestImage(const gfx::Size& size,
                           const ui::ColorProvider* provider) {
  SkBitmap bitmap =
      gfx::test::CreateBitmap(size.width(), size.height(), SK_ColorTRANSPARENT);
  SkCanvas canvas(bitmap);
  SkScalar radius = std::min(size.width(), size.height()) * SK_ScalarHalf;
  SkPaint paint;
  paint.setColor(provider->GetColor(
      views::examples::ExamplesColorIds::kColorNotificationExampleImage));
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);
  canvas.drawCircle(radius, radius, radius, paint);
  return gfx::Image::CreateFrom1xBitmap(bitmap);
}

}  // namespace

namespace views::examples {

NotificationExample::NotificationExample()
    : ExampleBase(
          l10n_util::GetStringUTF8(IDS_NOTIFICATION_SELECT_LABEL).c_str()) {
  message_center::MessageCenter::Initialize();
}

NotificationExample::~NotificationExample() {
  message_center::MessageCenter::Shutdown();
  observer_.Reset();
}

void NotificationExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<FlexLayout>())
      ->SetCrossAxisAlignment(LayoutAlignment::kStart);
  observer_.Observe(container);
}

void NotificationExample::OnViewAddedToWidget(View* observed_view) {
  auto* const cp = observed_view->GetColorProvider();
  message_center::RichNotificationData data;
  data.settings_button_handler = message_center::SettingsButtonHandler::INLINE;
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, "id", u"Title", u"Message",
      ui::ImageModel::FromImage(CreateTestImage(gfx::Size(80, 80), cp)),
      std::u16string(), GURL(),
      message_center::NotifierId(
          GURL(), l10n_util::GetStringUTF16(IDS_NOTIFICATION_TITLE_LABEL),
          /*web_app_id=*/std::nullopt),
      data, base::MakeRefCounted<message_center::NotificationDelegate>());
  notification.SetSmallImage(CreateTestImage(gfx::Size(16, 16), cp));
  notification.SetImage(CreateTestImage(gfx::Size(320, 240), cp));
  std::vector<message_center::ButtonInfo> buttons = {
      message_center::ButtonInfo(u"No-op"),
      message_center::ButtonInfo(u"Text input")};
  buttons[1].placeholder = u"Placeholder";
  notification.set_buttons(buttons);
  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(notification));
  auto* const notification_view = observed_view->AddChildView(
      std::make_unique<message_center::NotificationView>(notification));
  notification_view->SetProperty(
      views::kFlexBehaviorKey,
      FlexSpecification(MinimumFlexSizeRule::kPreferredSnapToMinimum,
                        MaximumFlexSizeRule::kPreferred));
}

void NotificationExample::OnViewIsDeleting(View* observed_view) {
  observer_.Reset();
}

}  // namespace views::examples
