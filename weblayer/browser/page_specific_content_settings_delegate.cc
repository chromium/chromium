// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/page_specific_content_settings_delegate.h"

#include "base/bind_helpers.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "content/public/browser/render_process_host.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "weblayer/common/renderer_configuration.mojom.h"

namespace weblayer {
namespace {

void SetContentSettingRules(content::RenderProcessHost* process,
                            const RendererContentSettingRules& rules) {
  mojo::AssociatedRemote<mojom::RendererConfiguration> rc_interface;
  process->GetChannel()->GetRemoteAssociatedInterface(&rc_interface);
  rc_interface->SetContentSettingRules(rules);
}

}  // namespace

PageSpecificContentSettingsDelegate::PageSpecificContentSettingsDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

PageSpecificContentSettingsDelegate::~PageSpecificContentSettingsDelegate() =
    default;

// static
void PageSpecificContentSettingsDelegate::UpdateRendererContentSettingRules(
    content::RenderProcessHost* process) {
  RendererContentSettingRules rules;
  GetRendererContentSettingRules(
      HostContentSettingsMapFactory::GetForBrowserContext(
          process->GetBrowserContext()),
      &rules);
  weblayer::SetContentSettingRules(process, rules);
}

void PageSpecificContentSettingsDelegate::UpdateLocationBar() {}

void PageSpecificContentSettingsDelegate::SetContentSettingRules(
    content::RenderProcessHost* process,
    const RendererContentSettingRules& rules) {
  weblayer::SetContentSettingRules(process, rules);
}

PrefService* PageSpecificContentSettingsDelegate::GetPrefs() {
  return static_cast<BrowserContextImpl*>(web_contents_->GetBrowserContext())
      ->pref_service();
}

HostContentSettingsMap* PageSpecificContentSettingsDelegate::GetSettingsMap() {
  return HostContentSettingsMapFactory::GetForBrowserContext(
      web_contents_->GetBrowserContext());
}

ContentSetting PageSpecificContentSettingsDelegate::GetEmbargoSetting(
    const GURL& request_origin,
    ContentSettingsType permission) {
  return PermissionDecisionAutoBlockerFactory::GetForBrowserContext(
             web_contents_->GetBrowserContext())
      ->GetEmbargoResult(request_origin, permission)
      .content_setting;
}

std::vector<storage::FileSystemType>
PageSpecificContentSettingsDelegate::GetAdditionalFileSystemTypes() {
  return {};
}

browsing_data::CookieHelper::IsDeletionDisabledCallback
PageSpecificContentSettingsDelegate::GetIsDeletionDisabledCallback() {
  return base::NullCallback();
}

bool PageSpecificContentSettingsDelegate::IsMicrophoneCameraStateChanged(
    content_settings::PageSpecificContentSettings::MicrophoneCameraState
        microphone_camera_state,
    const std::string& media_stream_selected_audio_device,
    const std::string& media_stream_selected_video_device) {
  return false;
}

content_settings::PageSpecificContentSettings::MicrophoneCameraState
PageSpecificContentSettingsDelegate::GetMicrophoneCameraState() {
  return content_settings::PageSpecificContentSettings::
      MICROPHONE_CAMERA_NOT_ACCESSED;
}

void PageSpecificContentSettingsDelegate::OnContentAllowed(
    ContentSettingsType type) {}

void PageSpecificContentSettingsDelegate::OnContentBlocked(
    ContentSettingsType type) {}

void PageSpecificContentSettingsDelegate::OnCacheStorageAccessAllowed(
    const url::Origin& origin) {}

void PageSpecificContentSettingsDelegate::OnCookieAccessAllowed(
    const net::CookieList& accessed_cookies) {}

void PageSpecificContentSettingsDelegate::OnDomStorageAccessAllowed(
    const url::Origin& origin) {}

void PageSpecificContentSettingsDelegate::OnFileSystemAccessAllowed(
    const url::Origin& origin) {}

void PageSpecificContentSettingsDelegate::OnIndexedDBAccessAllowed(
    const url::Origin& origin) {}

void PageSpecificContentSettingsDelegate::OnServiceWorkerAccessAllowed(
    const url::Origin& origin) {}

void PageSpecificContentSettingsDelegate::OnWebDatabaseAccessAllowed(
    const url::Origin& origin) {}

}  // namespace weblayer
