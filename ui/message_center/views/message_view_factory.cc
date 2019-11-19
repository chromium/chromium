// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_view_factory.h"

#include <vector>

#include "base/lazy_instance.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/views/notification_view_md.h"

namespace message_center {

namespace {

using MessageViewCustomFactoryMap =
    std::map<std::string, MessageViewFactory::CustomMessageViewFactoryFunction>;

base::LazyInstance<MessageViewCustomFactoryMap>::Leaky g_custom_view_factories =
    LAZY_INSTANCE_INITIALIZER;

std::unique_ptr<MessageView> GetCustomNotificationView(
    const Notification& notification) {
  MessageViewCustomFactoryMap* factories = g_custom_view_factories.Pointer();
  auto iter = factories->find(notification.custom_view_type());
  DCHECK(iter != factories->end());
  return iter->second.Run(notification);
}

}  // namespace

// static
MessageView* MessageViewFactory::Create(const Notification& notification) {
  MessageView* notification_view = nullptr;
  switch (notification.type()) {
    case NOTIFICATION_TYPE_BASE_FORMAT:
    case NOTIFICATION_TYPE_IMAGE:
    case NOTIFICATION_TYPE_MULTIPLE:
    case NOTIFICATION_TYPE_SIMPLE:
    case NOTIFICATION_TYPE_PROGRESS:
      // Rely on default construction after the switch.
      break;
    case NOTIFICATION_TYPE_CUSTOM:
      notification_view = GetCustomNotificationView(notification).release();
      break;
    default:
      // If the caller asks for an unrecognized kind of view (entirely possible
      // if an application is running on an older version of this code that
      // doesn't have the requested kind of notification template), we'll fall
      // back to a notification instance that will provide at least basic
      // functionality.
      LOG(WARNING) << "Unable to fulfill request for unrecognized or"
                   << "unsupported notification type " << notification.type()
                   << ". Falling back to simple notification type.";
      break;
  }

  if (!notification_view)
    notification_view = new NotificationViewMD(notification);

  return notification_view;
}

// static
void MessageViewFactory::SetCustomNotificationViewFactory(
    const std::string& custom_view_type,
    const CustomMessageViewFactoryFunction& factory_function) {
  MessageViewCustomFactoryMap* factories = g_custom_view_factories.Pointer();
  DCHECK(factories->find(custom_view_type) == factories->end());
  factories->emplace(custom_view_type, factory_function);
}

// static
bool MessageViewFactory::HasCustomNotificationViewFactory(
    const std::string& custom_view_type) {
  MessageViewCustomFactoryMap* factories = g_custom_view_factories.Pointer();
  return factories->find(custom_view_type) != factories->end();
}

// static
void MessageViewFactory::ClearCustomNotificationViewFactoryForTest(
    const std::string& custom_view_type) {
  g_custom_view_factories.Get().erase(custom_view_type);
}

}  // namespace message_center
