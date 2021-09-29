// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_WEBUI_ALLOWLIST_H_
#define UI_WEBUI_WEBUI_ALLOWLIST_H_

#include <initializer_list>
#include <map>

#include "base/memory/ref_counted.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_checker.h"
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
class WebUIAllowlist : public base::RefCountedThreadSafe<WebUIAllowlist> {
 public:
  static WebUIAllowlist* GetOrCreate(content::BrowserContext* browser_context);

  WebUIAllowlist();
  WebUIAllowlist(const WebUIAllowlist&) = delete;
  void operator=(const WebUIAllowlist&) = delete;

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

  // Returns a content_settings::RuleIterator, this method is thread-safe.
  //
  // This method acquires `lock_` and transfers it to the returned iterator.
  // NO_THREAD_SAFETY_ANALYSIS because the analyzer doesn't recognize acquiring
  // the lock in a unique_ptr.
  std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
      ContentSettingsType content_type) const NO_THREAD_SAFETY_ANALYSIS;

  void SetWebUIAllowlistProvider(WebUIAllowlistProvider* provider);
  void ResetWebUIAllowlistProvider();

 private:
  friend class base::RefCountedThreadSafe<WebUIAllowlist>;
  ~WebUIAllowlist();

  THREAD_CHECKER(thread_checker_);

  mutable base::Lock lock_;
  std::map<ContentSettingsType, std::map<url::Origin, ContentSetting>>
      permissions_ GUARDED_BY(lock_);

  WebUIAllowlistProvider* provider_ GUARDED_BY_CONTEXT(thread_checker_) =
      nullptr;
};

#endif  // UI_WEBUI_WEBUI_ALLOWLIST_H_
