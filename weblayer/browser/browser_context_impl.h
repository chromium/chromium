// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_CONTEXT_IMPL_H_
#define WEBLAYER_BROWSER_BROWSER_CONTEXT_IMPL_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "weblayer/browser/download_manager_delegate_impl.h"
#include "weblayer/public/profile.h"

namespace user_prefs {
class PrefRegistrySyncable;
}
class PrefService;

namespace weblayer {
class ProfileImpl;
class ResourceContextImpl;

namespace prefs {
// WebLayer specific pref names.
extern const char kNoStatePrefetchEnabled[];
extern const char kUkmEnabled[];
}  // namespace prefs

class BrowserContextImpl : public content::BrowserContext {
 public:
  BrowserContextImpl(ProfileImpl* profile_impl, const base::FilePath& path);
  ~BrowserContextImpl() override;
  BrowserContextImpl(const BrowserContextImpl&) = delete;
  BrowserContextImpl& operator=(const BrowserContextImpl&) = delete;

  static base::FilePath GetDefaultDownloadDirectory();

  // BrowserContext implementation:
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath&) override;
  base::FilePath GetPath() override;
  bool IsOffTheRecord() override;
  variations::VariationsClient* GetVariationsClient() override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;

  content::ResourceContext* GetResourceContext() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PlatformNotificationService* GetPlatformNotificationService()
      override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::StorageNotificationService* GetStorageNotificationService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override;
  content::ClientHintsControllerDelegate* GetClientHintsControllerDelegate()
      override;
  content::BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
  content::BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate()
      override;
  std::unique_ptr<download::InProgressDownloadManager>
  RetrieveInProgressDownloadManager() override;
  content::ContentIndexProvider* GetContentIndexProvider() override;
  content::ReduceAcceptLanguageControllerDelegate*
  GetReduceAcceptLanguageControllerDelegate() override;
  content::OriginTrialsControllerDelegate* GetOriginTrialsControllerDelegate()
      override;

  ProfileImpl* profile_impl() const { return profile_impl_; }

  PrefService* pref_service() const { return user_pref_service_.get(); }

  SimpleFactoryKey* simple_factory_key() { return &simple_factory_key_; }

 private:
  class WebLayerVariationsClient;

  // Creates a simple in-memory pref service.
  // TODO(timvolodine): Investigate whether WebLayer needs persistent pref
  // service.
  void CreateUserPrefService();

  // Registers the preferences that WebLayer accesses.
  void RegisterPrefs(user_prefs::PrefRegistrySyncable* pref_registry);

  const raw_ptr<ProfileImpl> profile_impl_;
  base::FilePath path_;
  // In Chrome, a SimpleFactoryKey is used as a minimal representation of a
  // BrowserContext used before full browser mode has started. WebLayer doesn't
  // have an incomplete mode, so this is just a member variable for
  // compatibility with components that expect a SimpleFactoryKey.
  SimpleFactoryKey simple_factory_key_;
  // ResourceContext needs to be deleted on the IO thread in general (and in
  // particular due to the destruction of the safebrowsing mojo interface
  // that has been added in ContentBrowserClient::ExposeInterfacesToRenderer
  // on IO thread, see crbug.com/1029317). Also this is similar to how Chrome
  // handles ProfileIOData.
  // TODO(timvolodine): consider a more general Profile shutdown/destruction
  // sequence for the IO/UI bits (crbug.com/1029879).
  std::unique_ptr<ResourceContextImpl, content::BrowserThread::DeleteOnIOThread>
      resource_context_;
  DownloadManagerDelegateImpl download_delegate_;
  std::unique_ptr<PrefService> user_pref_service_;
  std::unique_ptr<WebLayerVariationsClient> weblayer_variations_client_;
};
}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_CONTEXT_IMPL_H_
