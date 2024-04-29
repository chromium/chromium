// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_WOLVIC_BROWSER_CONTEXT_H_
#define WOLVIC_WOLVIC_BROWSER_CONTEXT_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/password_manager/core/browser/field_info_manager.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/prefs/pref_name_set.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/visitedlink/browser/visitedlink_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "wolvic/browser/autocomplete/wolvic_signin_client.h"
#include "wolvic/browser/downloads/wolvic_download_manager_delegate.h"

class SimpleFactoryKey;
class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace visitedlink {
class VisitedLinkWriter;
}

namespace content {

class WolvicBrowserContext : public BrowserContext,
			     public visitedlink::VisitedLinkDelegate {
 public:
  // If |delay_services_creation| is true, the owner is responsible for calling
  // CreateBrowserContextServices() for this BrowserContext.
  WolvicBrowserContext(bool off_the_record);

  WolvicBrowserContext(const WolvicBrowserContext&) = delete;
  WolvicBrowserContext& operator=(const WolvicBrowserContext&) = delete;

  ~WolvicBrowserContext() override;

  PrefService* GetPrefService() const { return user_pref_service_.get(); }

  // BrowserContext implementation.
  base::FilePath GetPath() override;
  std::unique_ptr<ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  bool IsOffTheRecord() override;
  DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  ResourceContext* GetResourceContext() override;
  BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  PlatformNotificationService* GetPlatformNotificationService() override;
  PushMessagingService* GetPushMessagingService() override;
  StorageNotificationService* GetStorageNotificationService() override;
  SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  PermissionControllerDelegate* GetPermissionControllerDelegate() override;
  BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  BackgroundSyncController* GetBackgroundSyncController() override;
  BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate() override;
  ContentIndexProvider* GetContentIndexProvider() override;
  ClientHintsControllerDelegate* GetClientHintsControllerDelegate() override;
  FederatedIdentityApiPermissionContextDelegate*
  GetFederatedIdentityApiPermissionContext() override;
  FederatedIdentityPermissionContextDelegate*
  GetFederatedIdentityPermissionContext() override;
  ReduceAcceptLanguageControllerDelegate*
  GetReduceAcceptLanguageControllerDelegate() override;
  OriginTrialsControllerDelegate* GetOriginTrialsControllerDelegate() override;

  static WolvicBrowserContext* FromWebContents(content::WebContents& web_contents);
  void AddVisitedURLs(const std::vector<GURL>&);

  // visitedlink::VisitedLinkDelegate implementation.
  void RebuildTable(const scoped_refptr<URLEnumerator>& enumerator) override;

  autofill::AutocompleteHistoryManager* GetAutocompleteHistoryManager() {
    return autocomplete_history_manager_.get();
  }

  password_manager::PasswordStore* GetPasswordStore() {
    return password_store_.get();
  }

  password_manager::FieldInfoManager* GetFieldInfoManager() {
    return field_info_manager_.get();
  }

  signin::IdentityManager* GetIdentityManager() {
    return identity_manager_.get();
  }

  wolvic::WolvicSigninClient* GetSigninClient() {
    return signin_client_.get();
  }

 protected:
  std::unique_ptr<BackgroundSyncController> background_sync_controller_;
  std::unique_ptr<ContentIndexProvider> content_index_provider_;
  std::unique_ptr<ReduceAcceptLanguageControllerDelegate>
      reduce_accept_lang_controller_delegate_;
  std::unique_ptr<wolvic::WolvicDownloadManagerDelegate>
      download_manager_delegate_;

 private:
  friend class password_manager::PasswordStore;

  // Performs initialization of the WolvicBrowserContext while IO is still
  // allowed on the current thread.
  void InitWhileIOAllowed();
  void FinishInitWhileIOAllowed();

  base::FilePath GetPrefStorePath();
  void CreateUserPrefService();
  void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry,
                     PrefNameSet* persistent_prefs);
  void MigrateLocalStatePrefs();
  void CreateAutocompleteHistoryManager();
  void CreatePasswordStore();
  void CreateIdentityManger();

  const bool off_the_record_;
  std::unique_ptr<ResourceContext> resource_context_;
  std::unique_ptr<PrefService> user_pref_service_;
  base::FilePath path_;
  std::unique_ptr<SimpleFactoryKey> key_;
  std::unique_ptr<visitedlink::VisitedLinkWriter> visitedlink_writer_;
  std::unique_ptr<autofill::AutocompleteHistoryManager> autocomplete_history_manager_;
  scoped_refptr<password_manager::PasswordStore> password_store_;
  std::unique_ptr<password_manager::FieldInfoManager> field_info_manager_;
  std::unique_ptr<signin::IdentityManager> identity_manager_;
  std::unique_ptr<wolvic::WolvicSigninClient> signin_client_;
};

}  // namespace content

#endif  // WOLVIC_WOLVIC_BROWSER_CONTEXT_H_
