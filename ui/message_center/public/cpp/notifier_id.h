// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_PUBLIC_CPP_NOTIFIER_ID_H_
#define UI_MESSAGE_CENTER_PUBLIC_CPP_NOTIFIER_ID_H_

#include <stddef.h>

#include <optional>
#include <string>
#include <vector>

#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/message_center_public_export.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/notifier_catalogs.h"
#endif

namespace message_center {

// This enum is being used for histogram reporting and the elements should not
// be re-ordered.
enum class NotifierType : int {
  APPLICATION = 0,
  ARC_APPLICATION = 1,
  WEB_PAGE = 2,
  SYSTEM_COMPONENT = 3,
  CROSTINI_APPLICATION = 4,
  PHONE_HUB = 5,
  kMaxValue = PHONE_HUB,
};

// A struct that identifies the source of notifications. For example, a web page
// might send multiple notifications but they'd all have the same NotifierId.
struct MESSAGE_CENTER_PUBLIC_EXPORT NotifierId {
  // Default constructor needed for generated mojom files and tests.
  NotifierId();

// Constructor for non WEB_PAGE type. `catalog_name` is required for CrOS system
// notifications.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  NotifierId(NotifierType type,
             const std::string& id,
             ash::NotificationCatalogName catalog_name =
                 ash::NotificationCatalogName::kNone);
#else
  NotifierId(NotifierType type, const std::string& id);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Constructor for WEB_PAGE type.
  explicit NotifierId(const GURL& url);

  // Constructor for WEB_PAGE type. The |title| must only be populated when a
  // trust relationship has been established, and it is appropriate to display
  // this instead of the |url|'s origin for attribution.
  NotifierId(const GURL& url,
             std::optional<std::u16string> title,
             std::optional<std::string> web_app_id);

  NotifierId(const NotifierId& other);
  ~NotifierId();

  bool operator==(const NotifierId& other) const;
  // Allows NotifierId to be used as a key in std::map.
  bool operator<(const NotifierId& other) const;

  NotifierType type;

  // Identifier in ARC notifications to assign notification groups.
  std::optional<std::string> group_key;

  // The identifier of the app notifier. Empty if it's WEB_PAGE.
  std::string id;

#if BUILDFLAG(IS_CHROMEOS)
  // Identifier for CrOS system notifications.
  ash::NotificationCatalogName catalog_name;
#endif

  // The URL pattern of the notifier.
  GURL url;

  // The title provided by the app identifier. This is used by desktop web
  // applications.
  std::optional<std::u16string> title;

  // Optional web app identifier for type WEB_PAGE.
  std::optional<std::string> web_app_id;

  // The identifier of the profile where the notification is created. This is
  // used for ChromeOS multi-profile support and can be empty.
  std::string profile_id;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_PUBLIC_CPP_NOTIFIER_ID_H_
