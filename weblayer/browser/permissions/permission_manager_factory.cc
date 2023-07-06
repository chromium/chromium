// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/permissions/permission_manager_factory.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/background_sync/background_sync_permission_context.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/embedder_support/permission_context_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_util.h"
#include "components/webrtc/media_stream_device_enumerator_impl.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "weblayer/browser/background_fetch/background_fetch_permission_context.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/permissions/geolocation_permission_context_delegate.h"
#include "weblayer/browser/permissions/weblayer_camera_pan_tilt_zoom_permission_context_delegate.h"
#include "weblayer/browser/permissions/weblayer_nfc_permission_context_delegate.h"

namespace weblayer {
namespace {

// Permission context which denies all requests.
class DeniedPermissionContext : public permissions::PermissionContextBase {
 public:
  using PermissionContextBase::PermissionContextBase;

 protected:
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override {
    return CONTENT_SETTING_BLOCK;
  }
};

// A permission context with default behavior, which is restricted to secure
// origins.
class SafePermissionContext : public permissions::PermissionContextBase {
 public:
  using PermissionContextBase::PermissionContextBase;
  SafePermissionContext(const SafePermissionContext&) = delete;
  SafePermissionContext& operator=(const SafePermissionContext&) = delete;
};

// Used by the CameraPanTiltZoomPermissionContext to query which devices support
// that API.
// TODO(crbug.com/1219486): Move this elsewhere once we're using a custom
// implementation of MediaStreamDeviceEnumerator to expose this information to
// WebLayer embedders via an API.
webrtc::MediaStreamDeviceEnumerator* GetMediaStreamDeviceEnumerator() {
  static base::NoDestructor<webrtc::MediaStreamDeviceEnumeratorImpl> instance;
  return instance.get();
}

permissions::PermissionManager::PermissionContextMap CreatePermissionContexts(
    content::BrowserContext* browser_context) {
  embedder_support::PermissionContextDelegates delegates;

  delegates.camera_pan_tilt_zoom_permission_context_delegate =
      std::make_unique<WebLayerCameraPanTiltZoomPermissionContextDelegate>();
  delegates.geolocation_permission_context_delegate =
      std::make_unique<GeolocationPermissionContextDelegate>();
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/1200933): macOS and ChromeOS uses
  // GeolocationPermissionContextSystem which requires a GeolocationManager for
  // construction. In Chrome this object is owned by the BrowserProcess. An
  // equivalent object will need to be created in WebLayer and passed into the
  // PermissionContextDelegates here before it supports macOS.
  NOTREACHED();
#endif  // BUILDFLAG(IS_MAC)
  delegates.media_stream_device_enumerator = GetMediaStreamDeviceEnumerator();
  delegates.nfc_permission_context_delegate =
      std::make_unique<WebLayerNfcPermissionContextDelegate>();

  // Create default permission contexts initially.
  permissions::PermissionManager::PermissionContextMap permission_contexts =
      embedder_support::CreateDefaultPermissionContexts(
          browser_context,
          /*is_regular_profile=*/false, std::move(delegates));

  // Add additional WebLayer specific permission contexts. Please add a comment
  // when adding new contexts here explaining why it can't be shared with other
  // Content embedders by adding it to CreateDefaultPermissionContexts().

  // Similar to the Chrome implementation except we don't have access to the
  // DownloadRequestLimiter in WebLayer.
  permission_contexts[ContentSettingsType::BACKGROUND_FETCH] =
      std::make_unique<BackgroundFetchPermissionContext>(browser_context);

  // The Chrome implementation only checks for policies which we don't have in
  // WebLayer.
  permission_contexts[ContentSettingsType::MEDIASTREAM_CAMERA] =
      std::make_unique<SafePermissionContext>(
          browser_context, ContentSettingsType::MEDIASTREAM_CAMERA,
          blink::mojom::PermissionsPolicyFeature::kCamera);
  permission_contexts[ContentSettingsType::MEDIASTREAM_MIC] =
      std::make_unique<SafePermissionContext>(
          browser_context, ContentSettingsType::MEDIASTREAM_MIC,
          blink::mojom::PermissionsPolicyFeature::kMicrophone);

#if BUILDFLAG(IS_ANDROID)
  // The Chrome implementation has special cases for Chrome OS and Windows which
  // we don't support yet. On Android this will match Chrome's behaviour.
  permission_contexts[ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER] =
      std::make_unique<SafePermissionContext>(
          browser_context, ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
          blink::mojom::PermissionsPolicyFeature::kEncryptedMedia);
#endif

  // For now, all requests are denied. As features are added, their permission
  // contexts can be added here instead of DeniedPermissionContext.
  for (blink::PermissionType type : blink::GetAllPermissionTypes()) {
#if !BUILDFLAG(IS_ANDROID)
    // PROTECTED_MEDIA_IDENTIFIER is only supported on Android.
    if (type == blink::PermissionType::PROTECTED_MEDIA_IDENTIFIER)
      continue;
#endif
    ContentSettingsType content_settings_type =
        permissions::PermissionUtil::PermissionTypeToContentSettingType(type);
    if (permission_contexts.find(content_settings_type) ==
        permission_contexts.end()) {
      permission_contexts[content_settings_type] =
          std::make_unique<DeniedPermissionContext>(
              browser_context, content_settings_type,
              blink::mojom::PermissionsPolicyFeature::kNotFound);
    }
  }

  return permission_contexts;
}

}  // namespace

// static
permissions::PermissionManager* PermissionManagerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<permissions::PermissionManager*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
PermissionManagerFactory* PermissionManagerFactory::GetInstance() {
  static base::NoDestructor<PermissionManagerFactory> factory;
  return factory.get();
}

PermissionManagerFactory::PermissionManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "PermissionManagerFactory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

PermissionManagerFactory::~PermissionManagerFactory() = default;

KeyedService* PermissionManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new permissions::PermissionManager(context,
                                            CreatePermissionContexts(context));
}

content::BrowserContext* PermissionManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace weblayer
