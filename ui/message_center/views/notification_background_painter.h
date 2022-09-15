// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_BACKGROUND_PAINTER_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_BACKGROUND_PAINTER_H_

#include "ui/message_center/message_center_export.h"
#include "ui/views/painter.h"

namespace message_center {

// Background Painter for notification. This is for notifications with rounded
// corners inside the unified message center. This draws the rectangle with
// rounded corners.
class MESSAGE_CENTER_EXPORT NotificationBackgroundPainter
    : public views::Painter {
 public:
  NotificationBackgroundPainter(float top_radius,
                                float bottom_radius,
                                SkColor color);

  NotificationBackgroundPainter(const NotificationBackgroundPainter&) = delete;
  NotificationBackgroundPainter& operator=(
      const NotificationBackgroundPainter&) = delete;

  ~NotificationBackgroundPainter() override;

  // views::Painter
  gfx::Size GetMinimumSize() const override;
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override;

  void set_insets(const gfx::Insets& insets) { insets_ = insets; }

 private:
  const SkScalar top_radius_;
  const SkScalar bottom_radius_;
  const SkColor color_;

  gfx::Insets insets_;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_BACKGROUND_PAINTER_H_
