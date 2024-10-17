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
#include "components/password_manager/core/browser/password_store/password_store.h"
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

namespace wolvic {

class WolvicBrowserContext : public content::BrowserContext,
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
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  bool IsOffTheRecord() override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PlatformNotificationService* GetPlatformNotificationService()
      override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::StorageNotificationService* GetStorageNotificationService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override;
  content::BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
  content::BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate()
      override;
  content::ContentIndexProvider* GetContentIndexProvider() override;
  content::ClientHintsControllerDelegate* GetClientHintsControllerDelegate()
      override;
  content::FederatedIdentityApiPermissionContextDelegate*
  GetFederatedIdentityApiPermissionContext() override;
  content::FederatedIdentityPermissionContextDelegate*
  GetFederatedIdentityPermissionContext() override;
  content::ReduceAcceptLanguageControllerDelegate*
  GetReduceAcceptLanguageControllerDelegate() override;
  content::OriginTrialsControllerDelegate* GetOriginTrialsControllerDelegate()
      override;

  static WolvicBrowserContext* FromWebContents(content::WebContents& web_contents);
  void AddVisitedURLs(const std::vector<GURL>&);

  // visitedlink::VisitedLinkDelegate implementation.
  void RebuildTable(const scoped_refptr<URLEnumerator>& enumerator) override;

  autofill::AutocompleteHistoryManager* GetAutocompleteHistoryManager();
  password_manager::PasswordStore* GetPasswordStore();
  password_manager::FieldInfoManager* GetFieldInfoManager();
  signin::IdentityManager* GetIdentityManager();
  WolvicSigninClient* GetSigninClient();

 protected:
  std::unique_ptr<content::BackgroundSyncController>
      background_sync_controller_;
  std::unique_ptr<content::ContentIndexProvider> content_index_provider_;
  std::unique_ptr<content::ReduceAcceptLanguageControllerDelegate>
      reduce_accept_lang_controller_delegate_;
  std::unique_ptr<WolvicDownloadManagerDelegate> download_manager_delegate_;

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
  void CreateSigninClient();
  void CreateAutocompleteHistoryManager();
  void CreatePasswordStore();
  void CreateIdentityManger();
  void CreateFieldInfoManager();

  const bool off_the_record_;
  std::unique_ptr<PrefService> user_pref_service_;
  base::FilePath path_;
  std::unique_ptr<SimpleFactoryKey> key_;
  std::unique_ptr<visitedlink::VisitedLinkWriter> visitedlink_writer_;
  std::unique_ptr<autofill::AutocompleteHistoryManager> autocomplete_history_manager_;
  scoped_refptr<password_manager::PasswordStore> password_store_;
  std::unique_ptr<password_manager::FieldInfoManager> field_info_manager_;
  std::unique_ptr<signin::IdentityManager> identity_manager_;
  std::unique_ptr<WolvicSigninClient> signin_client_;
};

}  // namespace wolvic

#endif  // WOLVIC_WOLVIC_BROWSER_CONTEXT_H_
