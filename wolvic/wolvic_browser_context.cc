// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/wolvic_browser_context.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/cdm/browser/media_drm_storage_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/origin_trials/browser/leveldb_persistence_provider.h"
#include "components/origin_trials/browser/origin_trials.h"
#include "components/origin_trials/common/features.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/segregated_pref_store.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_manager_builder.h"
#include "components/user_prefs/user_prefs.h"
#include "components/visitedlink/browser/visitedlink_writer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_content_index_provider.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "content/shell/browser/shell_federated_permission_context.h"
#include "content/shell/browser/shell_paths.h"
#include "content/shell/browser/shell_permission_manager.h"
#include "content/shell/common/shell_switches.h"
#include "content/test/mock_background_sync_controller.h"
#include "content/test/mock_reduce_accept_language_controller_delegate.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "wolvic/browser/autocomplete/wolvic_image_decoder.h"
#include "wolvic/browser/autocomplete/wolvic_password_store_backend.h"
#include "wolvic/browser/downloads/wolvic_download_manager_delegate.h"
#include "wolvic/wolvic_permission_manager.h"

namespace wolvic {

WolvicBrowserContext::WolvicBrowserContext(bool off_the_record)
    : off_the_record_(off_the_record) {
  LOG(ERROR) << "WolvicLifecycle WolvicBrowserContext()";
  InitWhileIOAllowed();
  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      this);
}

WolvicBrowserContext::~WolvicBrowserContext() {
  NotifyWillBeDestroyed();

  // The SimpleDependencyManager should always be passed after the
  // BrowserContextDependencyManager. This is because the KeyedService instances
  // in the BrowserContextDependencyManager's dependency graph can depend on the
  // ones in the SimpleDependencyManager's graph.
  DependencyManager::PerformInterlockedTwoPhaseShutdown(
      BrowserContextDependencyManager::GetInstance(), this,
      SimpleDependencyManager::GetInstance(), key_.get());

  SimpleKeyMap::GetInstance()->Dissociate(this);
  ShutdownStoragePartitions();
}

void WolvicBrowserContext::InitWhileIOAllowed() {
  CHECK(base::PathService::Get(content::SHELL_DIR_USER_DATA, &path_));
  LOG(ERROR) << "ShellBrowserContext data path=" << path_;

  FinishInitWhileIOAllowed();
}

void WolvicBrowserContext::FinishInitWhileIOAllowed() {
  key_ = std::make_unique<SimpleFactoryKey>(path_, off_the_record_);
  SimpleKeyMap::GetInstance()->Associate(this, key_.get());
  CreateUserPrefService();

  visitedlink_writer_ =
      std::make_unique<visitedlink::VisitedLinkWriter>(this, this, true);
  visitedlink_writer_->Init();
}

void WolvicBrowserContext::CreateSigninClient() {
  signin_client_ = std::make_unique<WolvicSigninClient>(this);
}

void WolvicBrowserContext::CreateAutocompleteHistoryManager() {
  autocomplete_history_manager_ = std::make_unique<autofill::AutocompleteHistoryManager>();
  auto local_storage = scoped_refptr<autofill::AutofillWebDataService>(nullptr);
  autocomplete_history_manager_->Init(local_storage, GetPrefService(), IsOffTheRecord());
}

void WolvicBrowserContext::CreatePasswordStore() {
  if (IsOffTheRecord()) {
    return;
  }

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();
  auto backend_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  password_store_ = new password_manager::PasswordStore(
      std::make_unique<WolvicPasswordStoreBackend>());
  password_store_->Init(GetPrefService(), nullptr);
}

void WolvicBrowserContext::CreateIdentityManger() {
  signin::IdentityManagerBuildParams params;

  params.signin_client = GetSigninClient();
  params.local_state = GetPrefService();
  params.network_connection_tracker = content::GetNetworkConnectionTracker();
  params.pref_service = GetPrefService();
  params.profile_path = GetPrefStorePath();
  params.image_decoder = std::make_unique<WolvicImageDecoder>();
  identity_manager_ = signin::BuildIdentityManager(&params);
}

void WolvicBrowserContext::CreateFieldInfoManager() {
  field_info_manager_ =
      std::make_unique<password_manager::FieldInfoManager>(
          base::SingleThreadTaskRunner::GetCurrentDefault());
}

base::FilePath WolvicBrowserContext::GetPrefStorePath() {
  // TODO(zvoit): Assign path based on profile
  base::FilePath pref_store_path;
  base::PathService::Get(base::DIR_ANDROID_APP_DATA, &pref_store_path);
  pref_store_path =
      pref_store_path.Append(FILE_PATH_LITERAL("Default/Preferences"));

  return pref_store_path;
}

void WolvicBrowserContext::CreateUserPrefService() {
  auto pref_registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();

  PrefNameSet persistent_prefs;
  RegisterPrefs(pref_registry.get(), &persistent_prefs);
  PrefServiceFactory pref_service_factory;

  pref_service_factory.set_user_prefs(base::MakeRefCounted<SegregatedPrefStore>(
      base::MakeRefCounted<InMemoryPrefStore>(),
      base::MakeRefCounted<JsonPrefStore>(GetPrefStorePath()),
      std::move(persistent_prefs)));

  user_pref_service_ = pref_service_factory.Create(pref_registry);

  user_prefs::UserPrefs::Set(this, user_pref_service_.get());
}

void WolvicBrowserContext::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry,
    PrefNameSet* persistent_prefs) {
  cdm::MediaDrmStorageImpl::RegisterProfilePrefs(registry);
  persistent_prefs->insert(cdm::prefs::kMediaDrmStorage);

  password_manager::PasswordManager::RegisterProfilePrefs(registry);
  signin::IdentityManager::RegisterProfilePrefs(registry);
  safe_browsing::RegisterProfilePrefs(registry);
}

std::unique_ptr<content::ZoomLevelDelegate>
WolvicBrowserContext::CreateZoomLevelDelegate(const base::FilePath&) {
  return nullptr;
}

base::FilePath WolvicBrowserContext::GetPath() {
  return path_;
}

bool WolvicBrowserContext::IsOffTheRecord() {
  return off_the_record_;
}

content::DownloadManagerDelegate*
WolvicBrowserContext::GetDownloadManagerDelegate() {
  if (!download_manager_delegate_) {
    download_manager_delegate_ =
        std::make_unique<WolvicDownloadManagerDelegate>();
  }
  return download_manager_delegate_.get();
}

content::BrowserPluginGuestManager* WolvicBrowserContext::GetGuestManager() {
  return nullptr;
}

storage::SpecialStoragePolicy* WolvicBrowserContext::GetSpecialStoragePolicy() {
  return nullptr;
}

content::PlatformNotificationService*
WolvicBrowserContext::GetPlatformNotificationService() {
  return nullptr;
}

content::PushMessagingService* WolvicBrowserContext::GetPushMessagingService() {
  return nullptr;
}

content::StorageNotificationService*
WolvicBrowserContext::GetStorageNotificationService() {
  return nullptr;
}

content::SSLHostStateDelegate* WolvicBrowserContext::GetSSLHostStateDelegate() {
  return nullptr;
}

content::PermissionControllerDelegate*
WolvicBrowserContext::GetPermissionControllerDelegate() {
  return WolvicPermissionManager::GetInstance(off_the_record_);
}

content::ClientHintsControllerDelegate*
WolvicBrowserContext::GetClientHintsControllerDelegate() {
  return nullptr;
}

content::BackgroundFetchDelegate*
WolvicBrowserContext::GetBackgroundFetchDelegate() {
  return nullptr;
}

content::BackgroundSyncController*
WolvicBrowserContext::GetBackgroundSyncController() {
  // if (!background_sync_controller_) {
  // background_sync_controller_ =
  // std::make_unique<MockBackgroundSyncController>();
  // }
  // return background_sync_controller_.get();
  return nullptr;
}

content::BrowsingDataRemoverDelegate*
WolvicBrowserContext::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

content::ContentIndexProvider* WolvicBrowserContext::GetContentIndexProvider() {
  return nullptr;
}

content::FederatedIdentityApiPermissionContextDelegate*
WolvicBrowserContext::GetFederatedIdentityApiPermissionContext() {
  return nullptr;
}

content::FederatedIdentityPermissionContextDelegate*
WolvicBrowserContext::GetFederatedIdentityPermissionContext() {
  return nullptr;
}

content::ReduceAcceptLanguageControllerDelegate*
WolvicBrowserContext::GetReduceAcceptLanguageControllerDelegate() {
  // if (!reduce_accept_lang_controller_delegate_) {
  // reduce_accept_lang_controller_delegate_ =
  // std::make_unique<MockReduceAcceptLanguageControllerDelegate>(
  // GetShellLanguage());
  // }
  // return reduce_accept_lang_controller_delegate_.get();
  return nullptr;
}

content::OriginTrialsControllerDelegate*
WolvicBrowserContext::GetOriginTrialsControllerDelegate() {
  return nullptr;
}

// static
WolvicBrowserContext* WolvicBrowserContext::FromWebContents(content::WebContents& web_contents) {
  // Safe as we're the only implementation.
  return static_cast<WolvicBrowserContext*>(web_contents.GetBrowserContext());
}

void
WolvicBrowserContext::AddVisitedURLs(const std::vector<GURL>& urls) {
  DCHECK(visitedlink_writer_);
  visitedlink_writer_->AddURLs(urls);
}

void
WolvicBrowserContext::RebuildTable(
    const scoped_refptr<URLEnumerator>& enumerator) {
  // Android WebView rebuilds from WebChromeClient.getVisitedHistory. The client
  // can change in the lifetime of this WebView and may not yet be set here.
  // Therefore this initialization path is not used.
  enumerator->OnComplete(true);
}

autofill::AutocompleteHistoryManager* WolvicBrowserContext::GetAutocompleteHistoryManager() {
  if (!autocomplete_history_manager_)
    CreateAutocompleteHistoryManager();
  return autocomplete_history_manager_.get();
}

password_manager::PasswordStore* WolvicBrowserContext::GetPasswordStore() {
  if (!password_store_)
    CreatePasswordStore();
  return password_store_.get();
}

password_manager::FieldInfoManager* WolvicBrowserContext::GetFieldInfoManager() {
  if (!field_info_manager_)
    CreateFieldInfoManager();
  return field_info_manager_.get();
}

signin::IdentityManager* WolvicBrowserContext::GetIdentityManager() {
  if (!identity_manager_)
    CreateIdentityManger();
  return identity_manager_.get();
}

WolvicSigninClient* WolvicBrowserContext::GetSigninClient() {
  if (!signin_client_)
    CreateSigninClient();
  return signin_client_.get();
}

}  // namespace wolvic
