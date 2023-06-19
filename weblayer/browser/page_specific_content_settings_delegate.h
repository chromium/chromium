// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PAGE_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_
#define WEBLAYER_BROWSER_PAGE_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_

#include "base/memory/raw_ptr.h"
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

  static void InitializeRenderer(content::RenderProcessHost* process);

 private:
  // PageSpecificContentSettings::Delegate:
  void UpdateLocationBar() override;
  PrefService* GetPrefs() override;
  HostContentSettingsMap* GetSettingsMap() override;
  std::unique_ptr<BrowsingDataModel::Delegate> CreateBrowsingDataModelDelegate()
      override;
  void SetDefaultRendererContentSettingRules(
      content::RenderFrameHost* rfh,
      RendererContentSettingRules* rules) override;
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
  content::WebContents* MaybeGetSyncedWebContentsForPictureInPicture(
      content::WebContents* web_contents) override;
  void OnContentAllowed(ContentSettingsType type) override;
  void OnContentBlocked(ContentSettingsType type) override;

  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PAGE_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_
