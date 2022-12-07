// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_context_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/background_sync/background_sync_controller_impl.h"
#include "components/blocked_content/safe_browsing_triggered_popup_blocker.h"
#include "components/client_hints/browser/client_hints.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/download/public/common/in_progress_download_manager.h"
#include "components/embedder_support/pref_names.h"
#include "components/heavy_ad_intervention/heavy_ad_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/origin_trials/browser/origin_trials.h"
#include "components/origin_trials/browser/prefservice_persistence_provider.h"
#include "components/payments/core/payment_prefs.h"
#include "components/permissions/permission_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/reduce_accept_language/browser/reduce_accept_language_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/site_isolation/pref_names.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/variations.mojom.h"
#include "components/variations/variations_client.h"
#include "components/variations/variations_ids_provider.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "weblayer/browser/background_fetch/background_fetch_delegate_factory.h"
#include "weblayer/browser/background_fetch/background_fetch_delegate_impl.h"
#include "weblayer/browser/background_sync/background_sync_controller_factory.h"
#include "weblayer/browser/browsing_data_remover_delegate.h"
#include "weblayer/browser/browsing_data_remover_delegate_factory.h"
#include "weblayer/browser/client_hints_factory.h"
#include "weblayer/browser/heavy_ad_service_factory.h"
#include "weblayer/browser/origin_trials_factory.h"
#include "weblayer/browser/permissions/permission_manager_factory.h"
#include "weblayer/browser/reduce_accept_language_factory.h"
#include "weblayer/browser/stateful_ssl_host_state_delegate_factory.h"
#include "weblayer/public/common/switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/path_utils.h"
#include "components/browser_ui/accessibility/android/font_size_prefs_android.h"
#include "components/cdm/browser/media_drm_storage_impl.h"  // nogncheck
#include "components/permissions/contexts/geolocation_permission_context_android.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/unified_consent/pref_names.h"
#elif BUILDFLAG(IS_WIN)
#include <windows.h>

#include <KnownFolders.h>
#include <shlobj.h>
#include "base/win/scoped_co_mem.h"
#elif BUILDFLAG(IS_POSIX)
#include "base/nix/xdg_util.h"
#endif

namespace weblayer {

namespace {

// Ignores origin security check. DownloadManagerImpl will provide its own
// implementation when InProgressDownloadManager object is passed to it.
bool IgnoreOriginSecurityCheck(const GURL& url) {
  return true;
}

void BindWakeLockProvider(
    mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) {
  content::GetDeviceService().BindWakeLockProvider(std::move(receiver));
}

}  // namespace

namespace prefs {
// Used to persist the public SettingType::NETWORK_PREDICTION_ENABLED API.
const char kNoStatePrefetchEnabled[] = "weblayer.network_prediction_enabled";

// Used to persist the public SettingType::UKM_ENABLED API.
const char kUkmEnabled[] = "weblayer.ukm_enabled";
}  // namespace prefs

class ResourceContextImpl : public content::ResourceContext {
 public:
  ResourceContextImpl() = default;

  ResourceContextImpl(const ResourceContextImpl&) = delete;
  ResourceContextImpl& operator=(const ResourceContextImpl&) = delete;

  ~ResourceContextImpl() override = default;
};

BrowserContextImpl::BrowserContextImpl(ProfileImpl* profile_impl,
                                       const base::FilePath& path)
    : profile_impl_(profile_impl),
      path_(path),
      simple_factory_key_(path, path.empty()),
      resource_context_(new ResourceContextImpl()),
      download_delegate_(GetDownloadManager()) {
  CreateUserPrefService();

  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      this);

  auto* heavy_ad_service = HeavyAdServiceFactory::GetForBrowserContext(this);
  if (IsOffTheRecord()) {
    heavy_ad_service->InitializeOffTheRecord();
  } else {
    heavy_ad_service->Initialize(GetPath());
  }

  site_isolation::SiteIsolationPolicy::ApplyPersistedIsolatedOrigins(this);

  // Ensure the delegate is initialized early to give it time to load its
  // persistence.
  GetOriginTrialsControllerDelegate();
}

BrowserContextImpl::~BrowserContextImpl() {
  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      this);
}

base::FilePath BrowserContextImpl::GetDefaultDownloadDirectory() {
  // Note: if we wanted to productionize this on Windows/Linux, refactor
  // src/chrome's GetDefaultDownloadDirectory.
  base::FilePath download_dir;
#if BUILDFLAG(IS_ANDROID)
  base::android::GetDownloadsDirectory(&download_dir);
#elif BUILDFLAG(IS_WIN)
  base::win::ScopedCoMem<wchar_t> path_buf;
  if (SUCCEEDED(
          SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &path_buf))) {
    download_dir = base::FilePath(path_buf.get());
  }
#else
  download_dir = base::nix::GetXDGUserDirectory("DOWNLOAD", "Downloads");
#endif
  return download_dir;
}

std::unique_ptr<content::ZoomLevelDelegate>
BrowserContextImpl::CreateZoomLevelDelegate(const base::FilePath&) {
  return nullptr;
}

base::FilePath BrowserContextImpl::GetPath() {
  return path_;
}

bool BrowserContextImpl::IsOffTheRecord() {
  return path_.empty();
}

content::DownloadManagerDelegate*
BrowserContextImpl::GetDownloadManagerDelegate() {
  return &download_delegate_;
}

content::ResourceContext* BrowserContextImpl::GetResourceContext() {
  return resource_context_.get();
}

content::BrowserPluginGuestManager* BrowserContextImpl::GetGuestManager() {
  return nullptr;
}

storage::SpecialStoragePolicy* BrowserContextImpl::GetSpecialStoragePolicy() {
  return nullptr;
}

content::PlatformNotificationService*
BrowserContextImpl::GetPlatformNotificationService() {
  return nullptr;
}

content::PushMessagingService* BrowserContextImpl::GetPushMessagingService() {
  return nullptr;
}

content::StorageNotificationService*
BrowserContextImpl::GetStorageNotificationService() {
  return nullptr;
}

content::SSLHostStateDelegate* BrowserContextImpl::GetSSLHostStateDelegate() {
  return StatefulSSLHostStateDelegateFactory::GetForBrowserContext(this);
}

content::PermissionControllerDelegate*
BrowserContextImpl::GetPermissionControllerDelegate() {
  return PermissionManagerFactory::GetForBrowserContext(this);
}

content::ClientHintsControllerDelegate*
BrowserContextImpl::GetClientHintsControllerDelegate() {
  return ClientHintsFactory::GetForBrowserContext(this);
}

content::BackgroundFetchDelegate*
BrowserContextImpl::GetBackgroundFetchDelegate() {
  return BackgroundFetchDelegateFactory::GetForBrowserContext(this);
}

content::BackgroundSyncController*
BrowserContextImpl::GetBackgroundSyncController() {
  return BackgroundSyncControllerFactory::GetForBrowserContext(this);
}

content::BrowsingDataRemoverDelegate*
BrowserContextImpl::GetBrowsingDataRemoverDelegate() {
  return BrowsingDataRemoverDelegateFactory::GetForBrowserContext(this);
}

content::ReduceAcceptLanguageControllerDelegate*
BrowserContextImpl::GetReduceAcceptLanguageControllerDelegate() {
  return ReduceAcceptLanguageFactory::GetForBrowserContext(this);
}

content::OriginTrialsControllerDelegate*
BrowserContextImpl::GetOriginTrialsControllerDelegate() {
  return OriginTrialsFactory::GetForBrowserContext(this);
}

std::unique_ptr<download::InProgressDownloadManager>
BrowserContextImpl::RetrieveInProgressDownloadManager() {
  // Override this to provide a connection to the wake lock service.
  auto download_manager = std::make_unique<download::InProgressDownloadManager>(
      nullptr, path_,
      path_.empty() ? nullptr
                    : GetDefaultStoragePartition()->GetProtoDatabaseProvider(),
      base::BindRepeating(&IgnoreOriginSecurityCheck),
      base::BindRepeating(&content::DownloadRequestUtils::IsURLSafe),
      base::BindRepeating(&BindWakeLockProvider));

#if BUILDFLAG(IS_ANDROID)
  download_manager->set_default_download_dir(GetDefaultDownloadDirectory());
#endif

  return download_manager;
}

content::ContentIndexProvider* BrowserContextImpl::GetContentIndexProvider() {
  return nullptr;
}

void BrowserContextImpl::CreateUserPrefService() {
  auto pref_registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();
  RegisterPrefs(pref_registry.get());

  PrefServiceFactory pref_service_factory;
  if (IsOffTheRecord()) {
    pref_service_factory.set_user_prefs(
        base::MakeRefCounted<InMemoryPrefStore>());
  } else {
    pref_service_factory.set_user_prefs(base::MakeRefCounted<JsonPrefStore>(
        path_.Append(FILE_PATH_LITERAL("Preferences"))));
  }
  {
    // Creating the prefs service may require reading the preferences from disk.
    base::ScopedAllowBlocking allow_io;
    user_pref_service_ = pref_service_factory.Create(pref_registry);
  }
  // Note: UserPrefs::Set also ensures that the user_pref_service_ has not
  // been set previously.
  user_prefs::UserPrefs::Set(this, user_pref_service_.get());
}

void BrowserContextImpl::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* pref_registry) {
  pref_registry->RegisterBooleanPref(prefs::kNoStatePrefetchEnabled, true);
  pref_registry->RegisterBooleanPref(prefs::kUkmEnabled, false);

  // This pref is used by captive_portal::CaptivePortalService (as well as other
  // potential use cases in the future, as it is used for various purposes
  // through //chrome).
  pref_registry->RegisterBooleanPref(
      embedder_support::kAlternateErrorPagesEnabled, true);
  pref_registry->RegisterListPref(
      site_isolation::prefs::kUserTriggeredIsolatedOrigins);
  pref_registry->RegisterDictionaryPref(
      site_isolation::prefs::kWebTriggeredIsolatedOrigins);

  StatefulSSLHostStateDelegate::RegisterProfilePrefs(pref_registry);
  HostContentSettingsMap::RegisterProfilePrefs(pref_registry);
  safe_browsing::RegisterProfilePrefs(pref_registry);
  language::LanguagePrefs::RegisterProfilePrefs(pref_registry);
  translate::TranslatePrefs::RegisterProfilePrefs(pref_registry);
  blocked_content::SafeBrowsingTriggeredPopupBlocker::RegisterProfilePrefs(
      pref_registry);
  payments::RegisterProfilePrefs(pref_registry);
  pref_registry->RegisterBooleanPref(
      translate::prefs::kOfferTranslateEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#if BUILDFLAG(IS_ANDROID)
  site_engagement::SiteEngagementService::RegisterProfilePrefs(pref_registry);
  cdm::MediaDrmStorageImpl::RegisterProfilePrefs(pref_registry);
  permissions::GeolocationPermissionContextAndroid::RegisterProfilePrefs(
      pref_registry);
  pref_registry->RegisterBooleanPref(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);

  pref_registry->RegisterDoublePref(browser_ui::prefs::kWebKitFontScaleFactor,
                                    1.0);
  blink::web_pref::WebPreferences pref_defaults;
  pref_registry->RegisterBooleanPref(browser_ui::prefs::kWebKitForceEnableZoom,
                                     pref_defaults.force_enable_zoom);
#endif

  BrowserContextDependencyManager::GetInstance()
      ->RegisterProfilePrefsForServices(pref_registry);
}

class BrowserContextImpl::WebLayerVariationsClient
    : public variations::VariationsClient {
 public:
  explicit WebLayerVariationsClient(content::BrowserContext* browser_context)
      : browser_context_(browser_context) {}

  ~WebLayerVariationsClient() override = default;

  bool IsOffTheRecord() const override {
    return browser_context_->IsOffTheRecord();
  }

  variations::mojom::VariationsHeadersPtr GetVariationsHeaders()
      const override {
    // As the embedder supplies the set of ids, the signed-in state should be
    // ignored. The value supplied (`is_signed_in`) doesn't matter as
    // VariationsIdsProvider is configured to ignore the signed in state.
    const bool is_signed_in = true;
    DCHECK_EQ(variations::VariationsIdsProvider::Mode::kIgnoreSignedInState,
              variations::VariationsIdsProvider::GetInstance()->mode());
    return variations::VariationsIdsProvider::GetInstance()
        ->GetClientDataHeaders(is_signed_in);
  }

 private:
  raw_ptr<content::BrowserContext> browser_context_;
};

variations::VariationsClient* BrowserContextImpl::GetVariationsClient() {
  if (!weblayer_variations_client_) {
    weblayer_variations_client_ =
        std::make_unique<WebLayerVariationsClient>(this);
  }
  return weblayer_variations_client_.get();
}

}  // namespace weblayer
