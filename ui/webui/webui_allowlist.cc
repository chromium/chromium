// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/webui_allowlist.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/webui_allowlist_provider.h"
#include "url/gurl.h"

const char kWebUIAllowlistKeyName[] = "WebUIAllowlist";

namespace {

class AllowlistRuleIterator : public content_settings::RuleIterator {
  using MapType = std::map<url::Origin, ContentSetting>;

 public:
  // Hold a reference to `allowlist` to keep it alive during iteration.
  explicit AllowlistRuleIterator(scoped_refptr<const WebUIAllowlist> allowlist,
                                 const MapType& map,
                                 std::unique_ptr<base::AutoLock> auto_lock)
      : auto_lock_(std::move(auto_lock)),
        allowlist_(std::move(allowlist)),
        it_(map.cbegin()),
        end_(map.cend()) {}
  AllowlistRuleIterator(const AllowlistRuleIterator&) = delete;
  void operator=(const AllowlistRuleIterator&) = delete;
  ~AllowlistRuleIterator() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  bool HasNext() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return it_ != end_;
  }

  content_settings::Rule Next() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    const auto& origin = it_->first;
    const auto& setting = it_->second;
    it_++;
    return content_settings::Rule(
        ContentSettingsPattern::FromURLNoWildcard(origin.GetURL()),
        ContentSettingsPattern::Wildcard(), base::Value(setting), base::Time(),
        content_settings::SessionModel::Durable);
  }

 private:
  const std::unique_ptr<base::AutoLock> auto_lock_;
  const scoped_refptr<const WebUIAllowlist> allowlist_;

  SEQUENCE_CHECKER(sequence_checker_);
  MapType::const_iterator it_ GUARDED_BY_CONTEXT(sequence_checker_);
  MapType::const_iterator end_ GUARDED_BY_CONTEXT(sequence_checker_);
};

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

  // It doesn't make sense to grant a default content setting.
  DCHECK_NE(CONTENT_SETTING_DEFAULT, setting);

  // We only support auto-granting permissions to chrome://,
  // chrome-untrusted://, and devtools:// schemes.
  DCHECK(origin.scheme() == content::kChromeUIScheme ||
         origin.scheme() == content::kChromeUIUntrustedScheme ||
         origin.scheme() == content::kChromeDevToolsScheme);
  {
    base::AutoLock auto_lock(lock_);

    // If the same permission is already registered, do nothing. We don't want
    // to notify the provider of ContentSettingChange when it is unnecessary.
    if (permissions_[type][origin] == setting)
      return;

    permissions_[type][origin] = setting;
  }

  // Notify the provider. |provider_| can be nullptr if
  // HostContentSettingsRegistry is shutting down i.e. when Chrome shuts down.
  if (provider_) {
    auto primary_pattern =
        ContentSettingsPattern::FromURLNoWildcard(origin.GetURL());
    auto secondary_pattern = ContentSettingsPattern::Wildcard();
    provider_->NotifyContentSettingChange(primary_pattern, secondary_pattern,
                                          type);
  }
}

void WebUIAllowlist::RegisterAutoGrantedPermissions(
    const url::Origin& origin,
    std::initializer_list<ContentSettingsType> types) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  for (const ContentSettingsType& type : types)
    RegisterAutoGrantedPermission(origin, type);
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
  auto auto_lock_ = std::make_unique<base::AutoLock>(lock_);

  auto permissions_it = permissions_.find(content_type);
  if (permissions_it != permissions_.end()) {
    return std::make_unique<AllowlistRuleIterator>(this, permissions_it->second,
                                                   std::move(auto_lock_));
  }

  return nullptr;
}
