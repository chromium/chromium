// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_WEBUI_ALLOWLIST_H_
#define UI_WEBUI_WEBUI_ALLOWLIST_H_

#include <initializer_list>
#include <map>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_checker.h"
#include "components/content_settings/core/browser/content_settings_origin_value_map.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
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
class WebUIAllowlist : public base::RefCountedThreadSafe<WebUIAllowlist> {
 public:
  static WebUIAllowlist* GetOrCreate(content::BrowserContext* browser_context);

  WebUIAllowlist();
  WebUIAllowlist(const WebUIAllowlist&) = delete;
  void operator=(const WebUIAllowlist&) = delete;

  // Register auto-granted |type| permission for WebUI |origin|. The |origin|
  // will have the permission even if it's embedded in a different origin.
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

  // Register auto-granted |types| permissions for |origin|. See comments above.
  void RegisterAutoGrantedPermissions(
      const url::Origin& origin,
      std::initializer_list<ContentSettingsType> types);

  // Grant the use of third-party cookies on origins matching
  // |third_party_origin_pattern|. The third-party origins must be embedded
  // (e.g. an iframe), or being requested (e.g. Fetch API) by the WebUI's
  // |top_level_origin|.
  //
  // See ContentSettingsPattern for how to construct such a pattern.
  void RegisterAutoGrantedThirdPartyCookies(
      const url::Origin& top_level_origin,
      const std::vector<ContentSettingsPattern>& origin_patterns);

  // Returns a content_settings::RuleIterator. The iterator keeps this list
  // alive while it is alive. This method is thread-safe.
  std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
      ContentSettingsType content_type) const;

  // Returns the matching Rule with highest precedence or nullptr if no Rule
  // matched. This method is thread-safe.
  std::unique_ptr<content_settings::Rule> GetRule(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type) const;

  void SetWebUIAllowlistProvider(WebUIAllowlistProvider* provider);
  void ResetWebUIAllowlistProvider();

 private:
  friend class base::RefCountedThreadSafe<WebUIAllowlist>;
  ~WebUIAllowlist();

  THREAD_CHECKER(thread_checker_);

  content_settings::OriginValueMap value_map_;

  raw_ptr<WebUIAllowlistProvider> provider_
      GUARDED_BY_CONTEXT(thread_checker_) = nullptr;

  void SetContentSettingsAndNotifyProvider(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType type,
      ContentSetting value);
};

#endif  // UI_WEBUI_WEBUI_ALLOWLIST_H_
