// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/page_specific_content_settings_delegate.h"

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/content_settings_manager_delegate.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "weblayer/common/renderer_configuration.mojom.h"

namespace weblayer {

PageSpecificContentSettingsDelegate::PageSpecificContentSettingsDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

PageSpecificContentSettingsDelegate::~PageSpecificContentSettingsDelegate() =
    default;

// static
void PageSpecificContentSettingsDelegate::InitializeRenderer(
    content::RenderProcessHost* process) {
  mojo::AssociatedRemote<mojom::RendererConfiguration> rc_interface;
  process->GetChannel()->GetRemoteAssociatedInterface(&rc_interface);
  mojo::PendingRemote<content_settings::mojom::ContentSettingsManager>
      content_settings_manager;
  content_settings::ContentSettingsManagerImpl::Create(
      process, content_settings_manager.InitWithNewPipeAndPassReceiver(),
      std::make_unique<ContentSettingsManagerDelegate>());
  rc_interface->SetInitialConfiguration(std::move(content_settings_manager));
}

void PageSpecificContentSettingsDelegate::UpdateLocationBar() {}

PrefService* PageSpecificContentSettingsDelegate::GetPrefs() {
  return static_cast<BrowserContextImpl*>(web_contents_->GetBrowserContext())
      ->pref_service();
}

HostContentSettingsMap* PageSpecificContentSettingsDelegate::GetSettingsMap() {
  return HostContentSettingsMapFactory::GetForBrowserContext(
      web_contents_->GetBrowserContext());
}

std::unique_ptr<BrowsingDataModel::Delegate>
PageSpecificContentSettingsDelegate::CreateBrowsingDataModelDelegate() {
  return nullptr;
}

void PageSpecificContentSettingsDelegate::SetDefaultRendererContentSettingRules(
    content::RenderFrameHost* rfh,
    RendererContentSettingRules* rules) {}

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
  return {};
}

content::WebContents* PageSpecificContentSettingsDelegate::
    MaybeGetSyncedWebContentsForPictureInPicture(
        content::WebContents* web_contents) {
  return nullptr;
}

void PageSpecificContentSettingsDelegate::OnContentAllowed(
    ContentSettingsType type) {}

void PageSpecificContentSettingsDelegate::OnContentBlocked(
    ContentSettingsType type) {}

}  // namespace weblayer
