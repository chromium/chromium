// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/public/cpp/notification.h"

#include <map>
#include <memory>

#include "build/chromeos_buildflags.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/metrics/histogram_functions.h"
#endif  // IS_CHROMEOS_ASH

namespace message_center {

namespace {

unsigned g_next_serial_number = 0;

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Histograms ------------------------------------------------------------------

constexpr char kNotificationImageMemorySizeHistogram[] =
    "Ash.Notification.ImageMemorySizeInKB";

constexpr char kNotificationSmallImageMemorySizeHistogram[] =
    "Ash.Notification.SmallImageMemorySizeInKB";

// Helpers ---------------------------------------------------------------------

// Returns the byte size of `image` on the 1.0 scale factor, based on the
// assumption that the color mode is RGBA.
// NOTE: We avoid using `gfx::Image::AsBitmap()` to reduce cost.
int CalculateImageByteSize(const gfx::Image& image) {
  constexpr int kBytesPerPixel = 4;
  return image.IsEmpty() ? 0 : image.Width() * image.Height() * kBytesPerPixel;
}

#endif  // IS_CHROMEOS_ASH

// Helpers ---------------------------------------------------------------------

const gfx::ImageSkia CreateSolidColorImage(int width,
                                           int height,
                                           SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

gfx::Image DeepCopyImage(const gfx::Image& image) {
  return image.IsEmpty() ? gfx::Image()
                         : gfx::Image(image.AsImageSkia().DeepCopy());
}

}  // namespace

NotificationItem::NotificationItem(const std::u16string& title,
                                   const std::u16string& message,
                                   ui::ImageModel icon)
    : title_(title), message_(message) {
  if (icon.IsEmpty()) {
    return;
  }

  icon_.emplace(std::move(icon));
}

NotificationItem::NotificationItem() = default;

NotificationItem::NotificationItem(const NotificationItem& other) = default;

NotificationItem::NotificationItem(NotificationItem&& other) = default;

NotificationItem& NotificationItem::operator=(const NotificationItem& other) =
    default;

NotificationItem& NotificationItem::operator=(NotificationItem&& other) =
    default;

NotificationItem::~NotificationItem() = default;

ButtonInfo::ButtonInfo(const std::u16string& title) : title(title) {}

ButtonInfo::ButtonInfo(const gfx::VectorIcon* vector_icon,
                       const std::u16string& accessible_name)
    : vector_icon(vector_icon), accessible_name(accessible_name) {}

ButtonInfo::ButtonInfo() = default;

ButtonInfo::ButtonInfo(const ButtonInfo& other) = default;

ButtonInfo::ButtonInfo(ButtonInfo&& other) = default;

ButtonInfo& ButtonInfo::operator=(const ButtonInfo& other) = default;

ButtonInfo& ButtonInfo::operator=(ButtonInfo&& other) = default;

ButtonInfo::~ButtonInfo() = default;

RichNotificationData::RichNotificationData() : timestamp(base::Time::Now()) {}

RichNotificationData::RichNotificationData(const RichNotificationData& other) =
    default;

RichNotificationData::~RichNotificationData() = default;

Notification::Notification(NotificationType type,
                           const std::string& id,
                           const std::u16string& title,
                           const std::u16string& message,
                           const ui::ImageModel& icon,
                           const std::u16string& display_source,
                           const GURL& origin_url,
                           const NotifierId& notifier_id,
                           const RichNotificationData& optional_fields,
                           scoped_refptr<NotificationDelegate> delegate)
    : type_(type),
      id_(id),
      title_(title),
      message_(message),
      icon_(icon),
      display_source_(display_source),
      origin_url_(origin_url),
      notifier_id_(notifier_id),
      optional_fields_(optional_fields),
      serial_number_(g_next_serial_number++),
      delegate_(std::move(delegate)) {}

Notification::Notification(scoped_refptr<NotificationDelegate> delegate,
                           const Notification& other)
    : Notification(other) {
  delegate_ = delegate;
}

Notification::Notification(const std::string& id, const Notification& other)
    : Notification(other) {
  id_ = id;
}

Notification::Notification(const Notification& other) = default;

Notification::Notification(Notification&& other) = default;

Notification& Notification::operator=(const Notification& other) = default;

Notification& Notification::operator=(Notification&& other) = default;

Notification::~Notification() = default;

// static
std::unique_ptr<Notification> Notification::DeepCopy(
    const Notification& notification,
    const ui::ColorProvider* color_provider,
    bool include_body_image,
    bool include_small_image,
    bool include_icon_images) {
  std::unique_ptr<Notification> notification_copy =
      std::make_unique<Notification>(notification);
  notification_copy->set_icon(ui::ImageModel::FromImageSkia(
      notification_copy->icon().Rasterize(color_provider)));
  notification_copy->SetImage(include_body_image
                                  ? DeepCopyImage(notification_copy->image())
                                  : gfx::Image());
  notification_copy->SetSmallImage(
      include_small_image ? DeepCopyImage(notification_copy->small_image())
                          : gfx::Image());
  for (size_t i = 0; i < notification_copy->buttons().size(); i++) {
    notification_copy->SetButtonIcon(
        i, include_icon_images
               ? DeepCopyImage(notification_copy->buttons()[i].icon)
               : gfx::Image());
  }
  return notification_copy;
}

void Notification::SetButtonIcon(size_t index, const gfx::Image& icon) {
  if (index >= optional_fields_.buttons.size()) {
    return;
  }
  optional_fields_.buttons[index].icon = icon;
}

void Notification::SetSystemPriority() {
  optional_fields_.priority = SYSTEM_PRIORITY;
  optional_fields_.never_timeout = true;
}

void Notification::SetGroupChild() {
  group_child_ = true;
  group_parent_ = false;
}

void Notification::SetGroupParent() {
  group_child_ = false;
  group_parent_ = true;
}

void Notification::ClearGroupChild() {
  DCHECK(!group_parent_);
  group_child_ = false;
}

void Notification::ClearGroupParent() {
  DCHECK(!group_child_);
  group_parent_ = false;
}

bool Notification::UseOriginAsContextMessage() const {
  return optional_fields_.context_message.empty() && origin_url_.is_valid() &&
         origin_url_.SchemeIsHTTPOrHTTPS();
}

void Notification::SetImage(const gfx::Image& image) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Record the memory size of `image` in KB.
  base::UmaHistogramMemoryKB(kNotificationImageMemorySizeHistogram,
                             CalculateImageByteSize(image) / 1024);
#endif  // IS_CHROMEOS_ASH

  optional_fields_.image = image;
}

void Notification::SetSmallImage(const gfx::Image& image) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Record the memory size of `image` in KB.
  base::UmaHistogramMemoryKB(kNotificationSmallImageMemorySizeHistogram,
                             CalculateImageByteSize(image) / 1024);
#endif  // IS_CHROMEOS_ASH

  optional_fields_.small_image = image;
}

gfx::Image Notification::GenerateMaskedSmallIcon(
    int dip_size,
    SkColor mask_color,
    SkColor background_color,
    SkColor foreground_color) const {
  if (!vector_small_image().is_empty()) {
    return gfx::Image(
        gfx::CreateVectorIcon(vector_small_image(), dip_size, mask_color));
  }

  if (small_image().IsEmpty()) {
    return gfx::Image();
  }

  // If |vector_small_image| is not available, fallback to raster based
  // masking and resizing.
  gfx::ImageSkia image;
  if (small_image_needs_additional_masking()) {
    image = GetMaskedSmallImage(small_image().AsImageSkia(), background_color,
                                foreground_color)
                .AsImageSkia();
  } else {
    image = small_image().AsImageSkia();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool create_masked_image =
      !optional_fields_.ignore_accent_color_for_small_image;
#else
  bool create_masked_image = false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (create_masked_image) {
    image = gfx::ImageSkiaOperations::CreateMaskedImage(
        CreateSolidColorImage(image.width(), image.height(), mask_color),
        image);
  }
  gfx::ImageSkia resized = gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::ResizeMethod::RESIZE_BEST,
      gfx::Size(dip_size, dip_size));
  return gfx::Image(resized);
}

// Take the alpha channel of small_image, mask it with the foreground,
// then add the masked foreground on top of the background
gfx::Image Notification::GetMaskedSmallImage(const gfx::ImageSkia& small_image,
                                             SkColor background_color,
                                             SkColor foreground_color) const {
  int width = small_image.width();
  int height = small_image.height();

  const gfx::ImageSkia background =
      CreateSolidColorImage(width, height, background_color);
  const gfx::ImageSkia foreground =
      CreateSolidColorImage(width, height, foreground_color);
  const gfx::ImageSkia masked_small_image =
      gfx::ImageSkiaOperations::CreateMaskedImage(foreground, small_image);
  return gfx::Image(gfx::ImageSkiaOperations::CreateSuperimposedImage(
      background, masked_small_image));
}

}  // namespace message_center
