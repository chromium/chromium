// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/public/cpp/notifier_id.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace message_center {

#if BUILDFLAG(IS_CHROMEOS)
NotifierId::NotifierId()
    : type(NotifierType::SYSTEM_COMPONENT),
      catalog_name(ash::NotificationCatalogName::kNone) {}
#else
NotifierId::NotifierId() : type(NotifierType::SYSTEM_COMPONENT) {}
#endif  // IS_CHROMEOS

#if BUILDFLAG(IS_CHROMEOS_ASH)
NotifierId::NotifierId(NotifierType type,
                       const std::string& id,
                       ash::NotificationCatalogName catalog_name)
    : type(type), id(id), catalog_name(catalog_name) {
  if (type == NotifierType::SYSTEM_COMPONENT) {
    DCHECK_NE(catalog_name, ash::NotificationCatalogName::kNone)
        << " System Notifications must include a `catalog_name`.";
  }
  DCHECK_NE(type, NotifierType::WEB_PAGE);
  DCHECK(!id.empty());
}
#else
NotifierId::NotifierId(NotifierType type, const std::string& id)
    : type(type), id(id) {
  DCHECK_NE(type, NotifierType::WEB_PAGE);
  DCHECK(!id.empty());
}
#endif  // IS_CHROMEOS_ASH

NotifierId::NotifierId(const GURL& origin)
    : NotifierId(origin,
                 /*title=*/std::nullopt,
                 /*web_app_id=*/std::nullopt) {}

NotifierId::NotifierId(const GURL& url,
                       std::optional<std::u16string> title,
                       std::optional<std::string> web_app_id)
    : type(NotifierType::WEB_PAGE),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      catalog_name(ash::NotificationCatalogName::kNone),
#endif  // IS_CHROMEOS_ASH
      url(url),
      title(std::move(title)),
      web_app_id(std::move(web_app_id)) {
}

NotifierId::NotifierId(const NotifierId& other) = default;

NotifierId::~NotifierId() = default;

bool NotifierId::operator==(const NotifierId& other) const {
  if (type != other.type)
    return false;

  if (profile_id != other.profile_id)
    return false;

  if (type == NotifierType::WEB_PAGE) {
    return std::tie(url, web_app_id) == std::tie(other.url, other.web_app_id);
  }

  if (type == NotifierType::ARC_APPLICATION) {
    return std::tie(id, group_key) == std::tie(other.id, other.group_key);
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (type == NotifierType::SYSTEM_COMPONENT &&
      catalog_name != other.catalog_name) {
    return false;
  }
#endif

  return id == other.id;
}

bool NotifierId::operator<(const NotifierId& other) const {
  if (type != other.type)
    return type < other.type;

  if (profile_id != other.profile_id)
    return profile_id < other.profile_id;

  if (type == NotifierType::WEB_PAGE) {
    return std::tie(url, web_app_id) < std::tie(other.url, other.web_app_id);
  }

  if (type == NotifierType::ARC_APPLICATION) {
    return std::tie(id, group_key) < std::tie(other.id, other.group_key);
  }

  return id < other.id;
}

}  // namespace message_center
