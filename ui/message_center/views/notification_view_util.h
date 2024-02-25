// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_UTIL_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_UTIL_H_

#include <memory>
#include <optional>

#include "ui/message_center/message_center_export.h"

namespace ui {
class Event;
}  // namespace ui

namespace views {
class View;
}  // namespace views

namespace message_center::notification_view_util {

std::unique_ptr<ui::Event> ConvertToBoundedLocatedEvent(const ui::Event& event,
                                                        views::View* target);

// Returns the corner radius applied to the large image. Returns `std::nullopt`
// if rounded corners are not required.
MESSAGE_CENTER_EXPORT std::optional<size_t> GetLargeImageCornerRadius();

}  // namespace message_center::notification_view_util

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_UTIL_H_
