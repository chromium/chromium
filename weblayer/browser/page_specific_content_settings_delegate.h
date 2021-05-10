// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PAGE_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_
#define WEBLAYER_BROWSER_PAGE_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_

#include "components/content_settings/browser/page_specific_content_settings.h"

namespace weblayer {

// Called by PageSpecificContentSettings to handle WebLayer specific logic.
class PageSpecificContentSettingsDelegate
    : public content_settings::PageSpecificContentSettings::Delegate {
 public:
  explicit PageSpecificContentSettingsDelegate(
      content::WebContents* web_contents);
  ~PageSpecificContentSettingsDelegate() override;
  PageSpecificContentSettingsDelegate(
      const PageSpecificContentSettingsDelegate&) = delete;
  PageSpecificContentSettingsDelegate& operator=(
      const PageSpecificContentSettingsDelegate&) = delete;

  static void UpdateRendererContentSettingRules(
      content::RenderProcessHost* process);

 private:
  // PageSpecificContentSettings::Delegate:
  void UpdateLocationBar() override;
  void SetContentSettingRules(
      content::RenderProcessHost* process,
      const RendererContentSettingRules& rules) override;
  PrefService* GetPrefs() override;
  HostContentSettingsMap* GetSettingsMap() override;
  ContentSetting GetEmbargoSetting(const GURL& request_origin,
                                   ContentSettingsType permission) override;
  std::vector<storage::FileSystemType> GetAdditionalFileSystemTypes() override;
  browsing_data::CookieHelper::IsDeletionDisabledCallback
  GetIsDeletionDisabledCallback() override;
  bool IsMicrophoneCameraStateChanged(
      content_settings::PageSpecificContentSettings::MicrophoneCameraState
          microphone_camera_state,
      const std::string& media_stream_selected_audio_device,
      const std::string& media_stream_selected_video_device) override;
  content_settings::PageSpecificContentSettings::MicrophoneCameraState
  GetMicrophoneCameraState() override;
  void OnContentAllowed(ContentSettingsType type) override;
  void OnContentBlocked(ContentSettingsType type) override;
  void OnCacheStorageAccessAllowed(const url::Origin& origin) override;
  void OnCookieAccessAllowed(const net::CookieList& accessed_cookies) override;
  void OnDomStorageAccessAllowed(const url::Origin& origin) override;
  void OnFileSystemAccessAllowed(const url::Origin& origin) override;
  void OnIndexedDBAccessAllowed(const url::Origin& origin) override;
  void OnServiceWorkerAccessAllowed(const url::Origin& origin) override;
  void OnWebDatabaseAccessAllowed(const url::Origin& origin) override;

  content::WebContents* web_contents_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PAGE_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_
