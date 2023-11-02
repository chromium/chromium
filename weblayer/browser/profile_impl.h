// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PROFILE_IMPL_H_
#define WEBLAYER_BROWSER_PROFILE_IMPL_H_

#include <set>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "build/build_config.h"
#include "weblayer/browser/browser_list_observer.h"
#include "weblayer/browser/i18n_util.h"
#include "weblayer/browser/profile_disk_operations.h"
#include "weblayer/public/profile.h"

#if BUILDFLAG(IS_ANDROID)
#include <jni.h>
#include "base/android/scoped_java_ref.h"
#endif

namespace content {
class BrowserContext;
class WebContents;
struct OpenURLParams;
}  // namespace content

namespace weblayer {
class BrowserContextImpl;
class CookieManagerImpl;
class PrerenderControllerImpl;

class ProfileImpl : public Profile {
 public:
  // Return the cache directory path for this BrowserContext. On some
  // platforms, file in cache directory may be deleted by the operating
  // system. So it is suitable for storing data that can be recreated such
  // as caches.
  // |context| must not be null.
  static base::FilePath GetCachePath(content::BrowserContext* context);

  static std::unique_ptr<ProfileImpl> DestroyAndDeleteDataFromDisk(
      std::unique_ptr<ProfileImpl> profile,
      base::OnceClosure done_callback);

  ProfileImpl(const std::string& name, bool is_incognito);

  ProfileImpl(const ProfileImpl&) = delete;
  ProfileImpl& operator=(const ProfileImpl&) = delete;

  ~ProfileImpl() override;

  // Returns the ProfileImpl from the specified BrowserContext.
  static ProfileImpl* FromBrowserContext(
      content::BrowserContext* browser_context);

  static std::set<ProfileImpl*> GetAllProfiles();

  // Allows getting notified when profiles are created or destroyed.
  class ProfileObserver {
   public:
    virtual void ProfileCreated(ProfileImpl* profile) {}
    virtual void ProfileDestroyed(ProfileImpl* profile) {}

   protected:
    virtual ~ProfileObserver() = default;
  };

  static void AddProfileObserver(ProfileObserver* observer);
  static void RemoveProfileObserver(ProfileObserver* observer);

  // Deletes |web_contents| after a delay. This is used if the owning Tab is
  // deleted and it's not safe to delete the WebContents.
  void DeleteWebContentsSoon(
      std::unique_ptr<content::WebContents> web_contents);

  BrowserContextImpl* GetBrowserContext();

  // Called when the download subsystem has finished initializing. By this point
  // information about downloads that were interrupted by a previous crash would
  // be available.
  void DownloadsInitialized();

  // Path data is stored at, empty if off-the-record.
  const base::FilePath& data_path() const { return info_.data_path; }
  const std::string& name() const { return info_.name; }
  DownloadDelegate* download_delegate() { return download_delegate_; }
  GoogleAccountAccessTokenFetchDelegate* access_token_fetch_delegate() {
    return access_token_fetch_delegate_;
  }

  void MarkAsDeleted();

  // Profile implementation:
  void ClearBrowsingData(const std::vector<BrowsingDataType>& data_types,
                         base::Time from_time,
                         base::Time to_time,
                         base::OnceClosure callback) override;
  void SetDownloadDirectory(const base::FilePath& directory) override;
  void SetDownloadDelegate(DownloadDelegate* delegate) override;
  void SetGoogleAccountAccessTokenFetchDelegate(
      GoogleAccountAccessTokenFetchDelegate* delegate) override;
  CookieManager* GetCookieManager() override;
  PrerenderController* GetPrerenderController() override;
  void GetBrowserPersistenceIds(
      base::OnceCallback<void(base::flat_set<std::string>)> callback) override;
  void RemoveBrowserPersistenceStorage(
      base::OnceCallback<void(bool)> done_callback,
      base::flat_set<std::string> ids) override;
  void SetBooleanSetting(SettingType type, bool value) override;
  bool GetBooleanSetting(SettingType type) override;
  void GetCachedFaviconForPageUrl(
      const GURL& page_url,
      base::OnceCallback<void(gfx::Image)> callback) override;
  void PrepareForPossibleCrossOriginNavigation() override;

#if BUILDFLAG(IS_ANDROID)
  ProfileImpl(JNIEnv* env,
              const base::android::JavaParamRef<jstring>& path,
              const base::android::JavaParamRef<jobject>& java_profile,
              bool is_incognito);

  jint GetNumBrowserImpl(JNIEnv* env);
  jlong GetBrowserContext(JNIEnv* env);
  void DestroyAndDeleteDataFromDisk(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& j_completion_callback);
  void ClearBrowsingData(
      JNIEnv* env,
      const base::android::JavaParamRef<jintArray>& j_data_types,
      const jlong j_from_time_millis,
      const jlong j_to_time_millis,
      const base::android::JavaRef<jobject>& j_callback);
  void SetDownloadDirectory(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& directory);
  jlong GetCookieManager(JNIEnv* env);
  jlong GetPrerenderController(JNIEnv* env);
  void EnsureBrowserContextInitialized(JNIEnv* env);
  void SetBooleanSetting(JNIEnv* env, jint j_type, jboolean j_value);
  jboolean GetBooleanSetting(JNIEnv* env, jint j_type);
  void GetBrowserPersistenceIds(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& j_callback);
  void RemoveBrowserPersistenceStorage(
      JNIEnv* env,
      const base::android::JavaRef<jobjectArray>& j_ids,
      const base::android::JavaRef<jobject>& j_callback);
  void PrepareForPossibleCrossOriginNavigation(JNIEnv* env);
  void GetCachedFaviconForPageUrl(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& j_page_url,
      const base::android::JavaRef<jobject>& j_callback);
  void MarkAsDeleted(JNIEnv* env) { MarkAsDeleted(); }
  base::android::ScopedJavaGlobalRef<jobject> GetJavaProfile() {
    return java_profile_;
  }
#endif

  const base::FilePath& download_directory() { return download_directory_; }

  // Get the directory where BrowserPersister stores tab state data. This will
  // be a real file path even for the off-the-record profile.
  base::FilePath GetBrowserPersisterDataBaseDir() const;

  // Creates a new web contents and navigates it according to `params`, but only
  // if an OpenUrlCallback has been set by the embedder. This is used for
  // navigations originating from service workers, which don't necessarily have
  // an associated tab. It may return null if the operation fails.
  content::WebContents* OpenUrl(const content::OpenURLParams& params);

 private:
  class DataClearer;

  static void OnProfileMarked(std::unique_ptr<ProfileImpl> profile,
                              base::OnceClosure done_callback);
  static void NukeDataAfterRemovingData(std::unique_ptr<ProfileImpl> profile,
                                        base::OnceClosure done_callback);
  static void DoNukeData(std::unique_ptr<ProfileImpl> profile,
                         base::OnceClosure done_callback);
  void ClearRendererCache();

  // Callback when the system locale has been updated.
  void OnLocaleChanged();

  // Returns the number of Browsers with this profile.
  int GetNumberOfBrowsers();

  void DeleteScheduleWebContents();

  ProfileInfo info_;

  std::unique_ptr<BrowserContextImpl> browser_context_;

  base::FilePath download_directory_;

  raw_ptr<DownloadDelegate> download_delegate_ = nullptr;
  raw_ptr<GoogleAccountAccessTokenFetchDelegate> access_token_fetch_delegate_ =
      nullptr;

  base::CallbackListSubscription locale_change_subscription_;

  std::unique_ptr<CookieManagerImpl> cookie_manager_;
  std::unique_ptr<PrerenderControllerImpl> prerender_controller_;

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_profile_;
#endif

  // The typical pattern for CancelableTaskTrackers is to have the caller
  // supply one. This code is predominantly called from the Java side, where
  // CancelableTaskTracker isn't applicable. Because of this, the
  // CancelableTaskTracker is owned by Profile.
  base::CancelableTaskTracker cancelable_task_tracker_;

  std::vector<std::unique_ptr<content::WebContents>> web_contents_to_delete_;

  base::WeakPtrFactory<ProfileImpl> weak_ptr_factory_{this};
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PROFILE_IMPL_H_
