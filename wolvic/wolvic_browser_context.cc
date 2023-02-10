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
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/origin_trials/browser/leveldb_persistence_provider.h"
#include "components/origin_trials/browser/origin_trials.h"
#include "components/origin_trials/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
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

namespace content {

WolvicBrowserContext::WolvicBrowserContext()
    : resource_context_(std::make_unique<ResourceContext>()) {
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

  // Need to destruct the ResourceContext before posting tasks which may delete
  // the URLRequestContext because ResourceContext's destructor will remove any
  // outstanding request while URLRequestContext's destructor ensures that there
  // are no more outstanding requests.
  if (resource_context_) {
    GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                          resource_context_.release());
  }
  ShutdownStoragePartitions();
}

void WolvicBrowserContext::InitWhileIOAllowed() {
  CHECK(base::PathService::Get(SHELL_DIR_USER_DATA, &path_));
  LOG(ERROR) << "ShellBrowserContext data path=" << path_;

  FinishInitWhileIOAllowed();
}

void WolvicBrowserContext::FinishInitWhileIOAllowed() {
  key_ = std::make_unique<SimpleFactoryKey>(path_, /*off_the_record=*/false);
  SimpleKeyMap::GetInstance()->Associate(this, key_.get());
}

std::unique_ptr<ZoomLevelDelegate>
WolvicBrowserContext::CreateZoomLevelDelegate(const base::FilePath&) {
  return nullptr;
}

base::FilePath WolvicBrowserContext::GetPath() {
  return path_;
}

bool WolvicBrowserContext::IsOffTheRecord() {
  return false;
}

DownloadManagerDelegate* WolvicBrowserContext::GetDownloadManagerDelegate() {
  return nullptr;
}

ResourceContext* WolvicBrowserContext::GetResourceContext() {
  return resource_context_.get();
}

BrowserPluginGuestManager* WolvicBrowserContext::GetGuestManager() {
  return nullptr;
}

storage::SpecialStoragePolicy* WolvicBrowserContext::GetSpecialStoragePolicy() {
  return nullptr;
}

PlatformNotificationService*
WolvicBrowserContext::GetPlatformNotificationService() {
  return nullptr;
}

PushMessagingService* WolvicBrowserContext::GetPushMessagingService() {
  return nullptr;
}

StorageNotificationService*
WolvicBrowserContext::GetStorageNotificationService() {
  return nullptr;
}

SSLHostStateDelegate* WolvicBrowserContext::GetSSLHostStateDelegate() {
  return nullptr;
}

PermissionControllerDelegate*
WolvicBrowserContext::GetPermissionControllerDelegate() {
  return nullptr;
}

ClientHintsControllerDelegate*
WolvicBrowserContext::GetClientHintsControllerDelegate() {
  return nullptr;
}

BackgroundFetchDelegate* WolvicBrowserContext::GetBackgroundFetchDelegate() {
  return nullptr;
}

BackgroundSyncController* WolvicBrowserContext::GetBackgroundSyncController() {
  // if (!background_sync_controller_) {
  // background_sync_controller_ =
  // std::make_unique<MockBackgroundSyncController>();
  // }
  // return background_sync_controller_.get();
  return nullptr;
}

BrowsingDataRemoverDelegate*
WolvicBrowserContext::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

ContentIndexProvider* WolvicBrowserContext::GetContentIndexProvider() {
  return nullptr;
}

FederatedIdentityApiPermissionContextDelegate*
WolvicBrowserContext::GetFederatedIdentityApiPermissionContext() {
  return nullptr;
}

FederatedIdentityPermissionContextDelegate*
WolvicBrowserContext::GetFederatedIdentityPermissionContext() {
  return nullptr;
}

ReduceAcceptLanguageControllerDelegate*
WolvicBrowserContext::GetReduceAcceptLanguageControllerDelegate() {
  // if (!reduce_accept_lang_controller_delegate_) {
  // reduce_accept_lang_controller_delegate_ =
  // std::make_unique<MockReduceAcceptLanguageControllerDelegate>(
  // GetShellLanguage());
  // }
  // return reduce_accept_lang_controller_delegate_.get();
  return nullptr;
}

OriginTrialsControllerDelegate*
WolvicBrowserContext::GetOriginTrialsControllerDelegate() {
  return nullptr;
}

}  // namespace content
