// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/webui_allowlist.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "ui/webui/webui_allowlist_provider.h"
#include "url/gurl.h"

const char kWebUIAllowlistKeyName[] = "WebUIAllowlist";

namespace {
struct WebUIAllowlistHolder : base::SupportsUserData::Data {
  explicit WebUIAllowlistHolder(scoped_refptr<WebUIAllowlist> list)
      : allow_list(std::move(list)) {}
  const scoped_refptr<WebUIAllowlist> allow_list;
};

}  // namespace

// static
WebUIAllowlist* WebUIAllowlist::GetOrCreate(
    content::BrowserContext* browser_context) {
  if (!browser_context->GetUserData(kWebUIAllowlistKeyName)) {
    auto list = base::MakeRefCounted<WebUIAllowlist>();
    browser_context->SetUserData(
        kWebUIAllowlistKeyName,
        std::make_unique<WebUIAllowlistHolder>(std::move(list)));
  }
  return static_cast<WebUIAllowlistHolder*>(
             browser_context->GetUserData(kWebUIAllowlistKeyName))
      ->allow_list.get();
}

WebUIAllowlist::WebUIAllowlist() = default;

WebUIAllowlist::~WebUIAllowlist() = default;

void WebUIAllowlist::RegisterAutoGrantedPermission(const url::Origin& origin,
                                                   ContentSettingsType type,
                                                   ContentSetting setting) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DCHECK(content::HasWebUIOrigin(origin));

  // It doesn't make sense to grant a default content setting.
  DCHECK_NE(CONTENT_SETTING_DEFAULT, setting);

  SetContentSettingsAndNotifyProvider(
      ContentSettingsPattern::FromURLNoWildcard(origin.GetURL()),
      ContentSettingsPattern::Wildcard(), type, setting);
}

void WebUIAllowlist::RegisterAutoGrantedPermissions(
    const url::Origin& origin,
    std::initializer_list<ContentSettingsType> types) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  for (const ContentSettingsType& type : types)
    RegisterAutoGrantedPermission(origin, type);
}

void WebUIAllowlist::RegisterAutoGrantedThirdPartyCookies(
    const url::Origin& top_level_origin,
    const std::vector<ContentSettingsPattern>& origin_patterns) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DCHECK(content::HasWebUIOrigin(top_level_origin));

  const auto top_level_origin_pattern =
      ContentSettingsPattern::FromURLNoWildcard(top_level_origin.GetURL());
  for (const auto& pattern : origin_patterns) {
    // For COOKIES content setting, |primary_pattern| is the origin setting the
    // cookie, |secondary_pattern| is the top-level document's origin.
    SetContentSettingsAndNotifyProvider(pattern, top_level_origin_pattern,
                                        ContentSettingsType::COOKIES,
                                        CONTENT_SETTING_ALLOW);
  }
}

void WebUIAllowlist::SetWebUIAllowlistProvider(
    WebUIAllowlistProvider* provider) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  provider_ = provider;
}

void WebUIAllowlist::ResetWebUIAllowlistProvider() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  provider_ = nullptr;
}

std::unique_ptr<content_settings::RuleIterator> WebUIAllowlist::GetRuleIterator(
    ContentSettingsType content_type) const {
  return value_map_.GetRuleIterator(content_type);
}

std::unique_ptr<content_settings::Rule> WebUIAllowlist::GetRule(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type) const {
  base::AutoLock lock(value_map_.GetLock());
  return value_map_.GetRule(primary_url, secondary_url, content_type);
}

void WebUIAllowlist::SetContentSettingsAndNotifyProvider(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType type,
    ContentSetting setting) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  {
    base::AutoLock auto_lock(value_map_.GetLock());
    if (!value_map_.SetValue(primary_pattern, secondary_pattern, type,
                             base::Value(setting),
                             /* metadata */ {})) {
      return;
    }
  }

  // Notify the provider. |provider_| can be nullptr if
  // HostContentSettingsRegistry is shutting down i.e. when Chrome shuts down.
  if (provider_) {
    provider_->NotifyContentSettingChange(primary_pattern, secondary_pattern,
                                          type);
  }
}
