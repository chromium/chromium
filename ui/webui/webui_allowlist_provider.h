// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_WEBUI_ALLOWLIST_PROVIDER_H_
#define UI_WEBUI_WEBUI_ALLOWLIST_PROVIDER_H_

#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/common/content_settings.h"
#include "ui/webui/webui_allowlist.h"

class ContentSettingsPattern;

// A provider that supplies HostContentSettingsMap with a list of auto-granted
// permissions from the underlying WebUIAllowlist.
class WebUIAllowlistProvider : public content_settings::ObservableProvider {
 public:
  // Note, |allowlist| must outlive this instance.
  explicit WebUIAllowlistProvider(WebUIAllowlist* allowlist);
  WebUIAllowlistProvider(const WebUIAllowlistProvider&) = delete;
  void operator=(const WebUIAllowlistProvider&) = delete;
  ~WebUIAllowlistProvider() override;

  void NotifyContentSettingChange(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type);

  // content_settings::ObservableProvider:
  std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      bool incognito) const override;
  void ShutdownOnUIThread() override;
  bool SetWebsiteSetting(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      std::unique_ptr<base::Value>&& value,
      const content_settings::ContentSettingConstraints& constraints) override;
  void ClearAllContentSettingsRules(ContentSettingsType content_type) override;

 private:
  WebUIAllowlist* allowlist_;
};

#endif  // UI_WEBUI_WEBUI_ALLOWLIST_PROVIDER_H_
