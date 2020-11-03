// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_PUBLIC_CPP_NOTIFIER_ID_H_
#define UI_MESSAGE_CENTER_PUBLIC_CPP_NOTIFIER_ID_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/message_center_public_export.h"
#include "url/gurl.h"

namespace message_center {

  // This enum is being used for histogram reporting and the elements should not
  // be re-ordered.
enum class NotifierType : int {
  APPLICATION = 0,
  ARC_APPLICATION = 1,
  WEB_PAGE = 2,
  SYSTEM_COMPONENT = 3,
  CROSTINI_APPLICATION = 4,
  kMaxValue = CROSTINI_APPLICATION,
};

// A struct that identifies the source of notifications. For example, a web page
// might send multiple notifications but they'd all have the same NotifierId.
struct MESSAGE_CENTER_PUBLIC_EXPORT NotifierId {
  // Default constructor needed for generated mojom files and tests.
  NotifierId();

  // Constructor for non WEB_PAGE type.
  NotifierId(NotifierType type, const std::string& id);

  // Constructor for WEB_PAGE type.
  explicit NotifierId(const GURL& url);

  NotifierId(const NotifierId& other);

  bool operator==(const NotifierId& other) const;
  // Allows NotifierId to be used as a key in std::map.
  bool operator<(const NotifierId& other) const;

  NotifierType type;

  // The identifier of the app notifier. Empty if it's WEB_PAGE.
  std::string id;

  // The URL pattern of the notifer.
  GURL url;

  // The identifier of the profile where the notification is created. This is
  // used for ChromeOS multi-profile support and can be empty.
  std::string profile_id;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_PUBLIC_CPP_NOTIFIER_ID_H_
