// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_WEBUI_ALLOWLIST_H_
#define UI_WEBUI_WEBUI_ALLOWLIST_H_

#include <initializer_list>
#include <map>

#include "base/supports_user_data.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
}
class WebUIAllowlistProvider;

// This class is the underlying storage for WebUIAllowlistProvider, it stores a
// list of origins and permissions to be auto-granted to WebUIs. This class is
// created before HostContentSettingsMap is registered and has the same lifetime
// as the profile it's attached to. It outlives WebUIAllowlistProvider.
class WebUIAllowlist : public base::SupportsUserData::Data {
 public:
  static WebUIAllowlist* GetOrCreate(content::BrowserContext* browser_context);

  WebUIAllowlist();
  WebUIAllowlist(const WebUIAllowlist&) = delete;
  void operator=(const WebUIAllowlist&) = delete;
  ~WebUIAllowlist() override;

  // Register auto-granted |type| permission for |origin|.
  //
  // WebUIAllowlist comes with no permission by default. Users can deny
  // permissions (e.g. Settings > Site Settings) unless they are registered
  // here.
  //
  // Most WebUIs would want to declare these:
  //   COOKIES: use persistent storage (e.g. localStorage)
  //   JAVASCRIPT: run JavaScript
  //   IMAGES: show images
  //   SOUND: play sounds
  void RegisterAutoGrantedPermission(
      const url::Origin& origin,
      ContentSettingsType type,
      ContentSetting setting = CONTENT_SETTING_ALLOW);

  // Register auto-granted |types| permissions for |origin|.
  void RegisterAutoGrantedPermissions(
      const url::Origin& origin,
      std::initializer_list<ContentSettingsType> types);

  std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
      ContentSettingsType content_type) const;

  void SetWebUIAllowlistProvider(WebUIAllowlistProvider* provider);
  void ResetWebUIAllowlistProvider();

 private:
  std::map<ContentSettingsType, std::map<url::Origin, ContentSetting>>
      permissions_;
  WebUIAllowlistProvider* provider_ = nullptr;
};

#endif  // UI_WEBUI_WEBUI_ALLOWLIST_H_
