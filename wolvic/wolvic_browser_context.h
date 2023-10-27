// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_WOLVIC_BROWSER_CONTEXT_H_
#define WOLVIC_WOLVIC_BROWSER_CONTEXT_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_name_set.h"
#include "components/visitedlink/browser/visitedlink_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "wolvic/browser/downloads/wolvic_download_manager_delegate.h"

class SimpleFactoryKey;
class PrefService;
class PrefRegistrySimple;

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

 protected:
  std::unique_ptr<PermissionControllerDelegate> permission_manager_;
  std::unique_ptr<BackgroundSyncController> background_sync_controller_;
  std::unique_ptr<ContentIndexProvider> content_index_provider_;
  std::unique_ptr<ReduceAcceptLanguageControllerDelegate>
      reduce_accept_lang_controller_delegate_;
  std::unique_ptr<wolvic::WolvicDownloadManagerDelegate>
      download_manager_delegate_;

 private:
  // Performs initialization of the WolvicBrowserContext while IO is still
  // allowed on the current thread.
  void InitWhileIOAllowed();
  void FinishInitWhileIOAllowed();

  base::FilePath GetPrefStorePath();
  void CreateUserPrefService();
  void RegisterPrefs(PrefRegistrySimple* registry, PrefNameSet* persistent_prefs);
  void MigrateLocalStatePrefs();

  const bool off_the_record_;
  std::unique_ptr<ResourceContext> resource_context_;
  std::unique_ptr<PrefService> user_pref_service_;
  base::FilePath path_;
  std::unique_ptr<SimpleFactoryKey> key_;
  std::unique_ptr<visitedlink::VisitedLinkWriter> visitedlink_writer_;
};

}  // namespace content

#endif  // WOLVIC_WOLVIC_BROWSER_CONTEXT_H_
