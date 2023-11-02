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
      bool incognito) const override;
  void ShutdownOnUIThread() override;
  bool SetWebsiteSetting(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      base::Value&& value,
      const content_settings::ContentSettingConstraints& constraints) override;
  void ClearAllContentSettingsRules(ContentSettingsType content_type) override;

 private:
  const scoped_refptr<WebUIAllowlist> allowlist_;
};

#endif  // UI_WEBUI_WEBUI_ALLOWLIST_PROVIDER_H_
