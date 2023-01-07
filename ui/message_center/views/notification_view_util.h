// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_UTIL_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_UTIL_H_

#include <memory>

namespace ui {
class Event;
}  // namespace ui

namespace views {
class View;
}  // namespace views

namespace message_center {

namespace notification_view_util {

std::unique_ptr<ui::Event> ConvertToBoundedLocatedEvent(const ui::Event& event,
                                                        views::View* target);

}  // namespace notification_view_util

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_UTIL_H_
