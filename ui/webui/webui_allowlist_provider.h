// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_WEBUI_ALLOWLIST_PROVIDER_H_
#define UI_WEBUI_WEBUI_ALLOWLIST_PROVIDER_H_

#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/common/content_settings.h"
#include "ui/webui/webui_allowlist.h"

class ContentSettingsPattern;

// A provider that supplies HostContentSettingsMap with a list of auto-granted
// permissions from the underlying WebUIAllowlist.
//
// PartitionKey is ignored by this provider because the content settings should
// apply across partitions.
class WebUIAllowlistProvider : public content_settings::ObservableProvider {
 public:
  explicit WebUIAllowlistProvider(scoped_refptr<WebUIAllowlist> allowlist);
  WebUIAllowlistProvider(const WebUIAllowlistProvider&) = delete;
  void operator=(const WebUIAllowlistProvider&) = delete;
  ~WebUIAllowlistProvider() override;

  void NotifyContentSettingChange(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type);

  // content_settings::ObservableProvider:
  // The following methods are thread-safe.
  std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      bool incognito,
      const content_settings::PartitionKey& partition_key) const override;
  std::unique_ptr<content_settings::Rule> GetRule(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      bool off_the_record,
      const content_settings::PartitionKey& partition_key) const override;
  void ShutdownOnUIThread() override;
  bool SetWebsiteSetting(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      base::Value&& value,
      const content_settings::ContentSettingConstraints& constraints,
      const content_settings::PartitionKey& partition_key) override;
  void ClearAllContentSettingsRules(
      ContentSettingsType content_type,
      const content_settings::PartitionKey& partition_key) override;

 private:
  const scoped_refptr<WebUIAllowlist> allowlist_;
};

#endif  // UI_WEBUI_WEBUI_ALLOWLIST_PROVIDER_H_
