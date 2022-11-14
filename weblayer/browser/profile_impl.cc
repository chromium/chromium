// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/profile_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_impl.h"
#include "weblayer/browser/browser_list.h"
#include "weblayer/browser/browsing_data_remover_delegate.h"
#include "weblayer/browser/cookie_manager_impl.h"
#include "weblayer/browser/favicon/favicon_service_impl.h"
#include "weblayer/browser/favicon/favicon_service_impl_factory.h"
#include "weblayer/browser/no_state_prefetch/prerender_controller_impl.h"
#include "weblayer/browser/persistence/browser_persister_file_utils.h"
#include "weblayer/browser/tab_impl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/unified_consent/pref_names.h"
#include "ui/gfx/android/java_bitmap.h"
#include "weblayer/browser/android/metrics/weblayer_metrics_service_client.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/java/jni/ProfileImpl_jni.h"
#include "weblayer/browser/safe_browsing/safe_browsing_service.h"
#endif

#if BUILDFLAG(IS_POSIX)
#include "base/base_paths_posix.h"
#endif

#if BUILDFLAG(IS_ANDROID)
using base::android::AttachCurrentThread;
#endif

namespace weblayer {

namespace {

bool g_first_profile_created = false;

// TaskRunner used by MarkProfileAsDeleted and NukeProfilesMarkedForDeletion to
// esnure that Nuke happens before any Mark in this process.
base::SequencedTaskRunner* GetBackgroundDiskOperationTaskRunner() {
  static const base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>>
      task_runner(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
  return task_runner.get()->get();
}

std::set<ProfileImpl*>& GetProfiles() {
  static base::NoDestructor<std::set<ProfileImpl*>> s_all_profiles;
  return *s_all_profiles;
}

base::ObserverList<ProfileImpl::ProfileObserver>::Unchecked& GetObservers() {
  static base::NoDestructor<
      base::ObserverList<ProfileImpl::ProfileObserver>::Unchecked>
      s_observers;
  return *s_observers;
}

#if BUILDFLAG(IS_ANDROID)
void PassFilePathsToJavaCallback(
    const base::android::ScopedJavaGlobalRef<jobject>& callback,
    const std::vector<std::string>& file_paths) {
  base::android::RunObjectCallbackAndroid(
      callback, base::android::ToJavaArrayOfStrings(
                    base::android::AttachCurrentThread(), file_paths));
}

void OnGotBrowserPersistenceIds(
    const base::android::ScopedJavaGlobalRef<jobject>& callback,
    base::flat_set<std::string> ids) {
  std::vector<std::string> as_vector;
  for (const std::string& id : ids)
    as_vector.push_back(id);
  base::android::RunObjectCallbackAndroid(
      callback,
      base::android::ToJavaArrayOfStrings(AttachCurrentThread(), as_vector));
}

void OnDidRemoveBrowserPersistenceStorage(
    const base::android::ScopedJavaGlobalRef<jobject>& callback,
    bool result) {
  base::android::RunBooleanCallbackAndroid(callback, result);
}

void OnDidGetCachedFaviconForPageUrl(
    const base::android::ScopedJavaGlobalRef<jobject>& callback,
    gfx::Image image) {
  SkBitmap favicon = image.AsImageSkia().GetRepresentation(1.0f).GetBitmap();
  base::android::RunObjectCallbackAndroid(
      callback, favicon.empty() ? nullptr : gfx::ConvertToJavaBitmap(favicon));
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

class ProfileImpl::DataClearer : public content::BrowsingDataRemover::Observer {
 public:
  DataClearer(content::BrowserContext* browser_context,
              base::OnceCallback<void()> callback)
      : remover_(browser_context->GetBrowsingDataRemover()),
        callback_(std::move(callback)) {
    remover_->AddObserver(this);
  }

  void ClearData(ProfileImpl* profile,
                 uint64_t mask,
                 base::Time from_time,
                 base::Time to_time) {
    uint64_t origin_types =
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
        content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;
    remover_->RemoveAndReply(from_time, to_time, mask, origin_types, this);
  }

  // content::BrowsingDataRemover::Observer:
  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override {
    // Remove the observer now as after this returns the BrowserContext may
    // be destroyed, which owns |remover_|.
    remover_->RemoveObserver(this);
    std::move(callback_).Run();
    delete this;
  }

 private:
  // DataClearer deletes itself when removal is done.
  ~DataClearer() override = default;

  raw_ptr<content::BrowsingDataRemover> remover_;
  base::OnceCallback<void()> callback_;
};

// static
base::FilePath ProfileImpl::GetCachePath(content::BrowserContext* context) {
  DCHECK(context);
  ProfileImpl* profile = FromBrowserContext(context);
  return profile->info_.cache_path;
}

ProfileImpl::ProfileImpl(const std::string& name, bool is_incognito)
    : download_directory_(BrowserContextImpl::GetDefaultDownloadDirectory()) {
  {
    base::ScopedAllowBlocking allow_blocking;
    info_ = CreateProfileInfo(name, is_incognito);
  }

  GetProfiles().insert(this);
  profile_metrics::SetBrowserProfileType(
      GetBrowserContext(), is_incognito
                               ? profile_metrics::BrowserProfileType::kIncognito
                               : profile_metrics::BrowserProfileType::kRegular);

  for (auto& observer : GetObservers())
    observer.ProfileCreated(this);

  if (!g_first_profile_created) {
    g_first_profile_created = true;
    GetBackgroundDiskOperationTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&NukeProfilesMarkedForDeletion));
  }

  // Ensure WebCacheManager is created so that it starts observing
  // OnRenderProcessHostCreated events.
  web_cache::WebCacheManager::GetInstance();

#if BUILDFLAG(IS_ANDROID)
  WebLayerMetricsServiceClient::GetInstance()->UpdateUkm(false);
#endif
}

ProfileImpl::~ProfileImpl() {
  // Destroy any scheduled WebContents. These implicitly refer to the
  // BrowserContext and must be destroyed before the BrowserContext.
  web_contents_to_delete_.clear();

  if (browser_context_) {
    // Needs to be called before ShutdownStoragePartitions().
    browser_context_->NotifyWillBeDestroyed();

    BrowserContextDependencyManager::GetInstance()
        ->DestroyBrowserContextServices(browser_context_.get());
    browser_context_->ShutdownStoragePartitions();
  }

  GetProfiles().erase(this);
  for (auto& observer : GetObservers())
    observer.ProfileDestroyed(this);
}

ProfileImpl* ProfileImpl::FromBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<BrowserContextImpl*>(browser_context)->profile_impl();
}

std::set<ProfileImpl*> ProfileImpl::GetAllProfiles() {
  return GetProfiles();
}

void ProfileImpl::AddProfileObserver(ProfileObserver* observer) {
  GetObservers().AddObserver(observer);
}

void ProfileImpl::RemoveProfileObserver(ProfileObserver* observer) {
  GetObservers().RemoveObserver(observer);
}

void ProfileImpl::DeleteWebContentsSoon(
    std::unique_ptr<content::WebContents> web_contents) {
  if (web_contents_to_delete_.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ProfileImpl::DeleteScheduleWebContents,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
  web_contents_to_delete_.push_back(std::move(web_contents));
}

BrowserContextImpl* ProfileImpl::GetBrowserContext() {
  if (browser_context_)
    return browser_context_.get();

  browser_context_ =
      std::make_unique<BrowserContextImpl>(this, info_.data_path);
  locale_change_subscription_ =
      i18n::RegisterLocaleChangeCallback(base::BindRepeating(
          &ProfileImpl::OnLocaleChanged, base::Unretained(this)));
  return browser_context_.get();
}

void ProfileImpl::DownloadsInitialized() {
#if BUILDFLAG(IS_ANDROID)
  return Java_ProfileImpl_downloadsInitialized(
      base::android::AttachCurrentThread(), java_profile_);
#endif
}

void ProfileImpl::MarkAsDeleted() {
  GetBackgroundDiskOperationTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&MarkProfileAsDeleted, info_));
}

void ProfileImpl::ClearBrowsingData(
    const std::vector<BrowsingDataType>& data_types,
    base::Time from_time,
    base::Time to_time,
    base::OnceClosure callback) {
  auto* clearer = new DataClearer(GetBrowserContext(), std::move(callback));
  // DataClearer will delete itself in OnBrowsingDataRemoverDone().
  // If Profile is destroyed during clearing, it would lead to destroying
  // browser_context_ and then BrowsingDataRemover, which in turn would call
  // OnBrowsingDataRemoverDone(), even though the clearing hasn't been finished.

  uint64_t remove_mask = 0;
  // This follows what Chrome does: see browsing_data_bridge.cc.
  for (auto data_type : data_types) {
    switch (data_type) {
      case BrowsingDataType::COOKIES_AND_SITE_DATA:
        remove_mask |= content::BrowsingDataRemover::DATA_TYPE_COOKIES;
        remove_mask |= content::BrowsingDataRemover::DATA_TYPE_DOM_STORAGE;
        remove_mask |= content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES;
        remove_mask |= BrowsingDataRemoverDelegate::DATA_TYPE_ISOLATED_ORIGINS;
        remove_mask |= BrowsingDataRemoverDelegate::DATA_TYPE_FAVICONS;
        remove_mask |= BrowsingDataRemoverDelegate::DATA_TYPE_AD_INTERVENTIONS;
        remove_mask |= content::BrowsingDataRemover::DATA_TYPE_PRIVACY_SANDBOX;
        break;
      case BrowsingDataType::CACHE:
        remove_mask |= content::BrowsingDataRemover::DATA_TYPE_CACHE;
        ClearRendererCache();
        break;
      case BrowsingDataType::SITE_SETTINGS:
        remove_mask |= BrowsingDataRemoverDelegate::DATA_TYPE_SITE_SETTINGS;
        break;
      default:
        NOTREACHED();
    }
  }
  clearer->ClearData(this, remove_mask, from_time, to_time);
}

void ProfileImpl::SetDownloadDirectory(const base::FilePath& directory) {
  download_directory_ = directory;
}

void ProfileImpl::SetDownloadDelegate(DownloadDelegate* delegate) {
  download_delegate_ = delegate;
}

void ProfileImpl::SetGoogleAccountAccessTokenFetchDelegate(
    GoogleAccountAccessTokenFetchDelegate* delegate) {
  access_token_fetch_delegate_ = delegate;
}

CookieManager* ProfileImpl::GetCookieManager() {
  if (!cookie_manager_)
    cookie_manager_ = std::make_unique<CookieManagerImpl>(GetBrowserContext());
  return cookie_manager_.get();
}

PrerenderController* ProfileImpl::GetPrerenderController() {
  if (!prerender_controller_)
    prerender_controller_ =
        std::make_unique<PrerenderControllerImpl>(GetBrowserContext());
  return prerender_controller_.get();
}

void ProfileImpl::GetBrowserPersistenceIds(
    base::OnceCallback<void(base::flat_set<std::string>)> callback) {
  DCHECK(!browser_context_->IsOffTheRecord());
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&GetBrowserPersistenceIdsOnBackgroundThread,
                     GetBrowserPersisterDataBaseDir()),
      std::move(callback));
}

void ProfileImpl::RemoveBrowserPersistenceStorage(
    base::OnceCallback<void(bool)> done_callback,
    base::flat_set<std::string> ids) {
  DCHECK(!browser_context_->IsOffTheRecord());
  RemoveBrowserPersistenceStorageImpl(this, std::move(done_callback),
                                      std::move(ids));
}

// static
void ProfileImpl::NukeDataAfterRemovingData(
    std::unique_ptr<ProfileImpl> profile,
    base::OnceClosure done_callback) {
  // Need PostTask to avoid reentrancy for deleting |browser_context_|.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ProfileImpl::DoNukeData, std::move(profile),
                                std::move(done_callback)));
}

// static
void ProfileImpl::DoNukeData(std::unique_ptr<ProfileImpl> profile,
                             base::OnceClosure done_callback) {
  ProfileInfo info = profile->info_;
  profile.reset();
  GetBackgroundDiskOperationTaskRunner()->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&TryNukeProfileFromDisk, info),
      std::move(done_callback));
}

void ProfileImpl::ClearRendererCache() {
  for (content::RenderProcessHost::iterator iter =
           content::RenderProcessHost::AllHostsIterator();
       !iter.IsAtEnd(); iter.Advance()) {
    content::RenderProcessHost* render_process_host = iter.GetCurrentValue();
    if (render_process_host->GetBrowserContext() == GetBrowserContext() &&
        render_process_host->IsInitializedAndNotDead()) {
      web_cache::WebCacheManager::GetInstance()->ClearCacheForProcess(
          render_process_host->GetID());
    }
  }
}

void ProfileImpl::OnLocaleChanged() {
  GetBrowserContext()->ForEachStoragePartition(base::BindRepeating(
      [](const std::string& accept_language,
         content::StoragePartition* storage_partition) {
        storage_partition->GetNetworkContext()->SetAcceptLanguage(
            accept_language);
      },
      i18n::GetAcceptLangs()));
}

// static
std::unique_ptr<Profile> Profile::Create(const std::string& name,
                                         bool is_incognito) {
  return std::make_unique<ProfileImpl>(name, is_incognito);
}

// static
std::unique_ptr<Profile> Profile::DestroyAndDeleteDataFromDisk(
    std::unique_ptr<Profile> profile,
    base::OnceClosure done_callback) {
  std::unique_ptr<ProfileImpl> impl(
      static_cast<ProfileImpl*>(profile.release()));
  return ProfileImpl::DestroyAndDeleteDataFromDisk(std::move(impl),
                                                   std::move(done_callback));
}

// static
std::unique_ptr<ProfileImpl> ProfileImpl::DestroyAndDeleteDataFromDisk(
    std::unique_ptr<ProfileImpl> profile,
    base::OnceClosure done_callback) {
  if (profile->GetNumberOfBrowsers() > 0)
    return profile;

  ProfileInfo profile_info = profile->info_;
  GetBackgroundDiskOperationTaskRunner()->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&MarkProfileAsDeleted, profile_info),
      base::BindOnce(&ProfileImpl::OnProfileMarked, std::move(profile),
                     std::move(done_callback)));
  return nullptr;
}

// static
void ProfileImpl::OnProfileMarked(std::unique_ptr<ProfileImpl> profile,
                                  base::OnceClosure done_callback) {
  // Try to finish all writes and remove all data before nuking the profile.
  profile->GetBrowserContext()->pref_service()->CommitPendingWrite();

  ProfileImpl* raw_profile = profile.get();
  auto* clearer = new DataClearer(
      raw_profile->GetBrowserContext(),
      base::BindOnce(&ProfileImpl::NukeDataAfterRemovingData,
                     std::move(profile), std::move(done_callback)));
  uint64_t remove_all_mask = 0xffffffffffffffffull;
  clearer->ClearData(raw_profile, remove_all_mask, base::Time::Min(),
                     base::Time::Max());
}

#if BUILDFLAG(IS_ANDROID)
ProfileImpl::ProfileImpl(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& name,
    const base::android::JavaParamRef<jobject>& java_profile,
    bool is_incognito)
    : ProfileImpl(ConvertJavaStringToUTF8(env, name), is_incognito) {
  java_profile_ = java_profile;
}

static jlong JNI_ProfileImpl_CreateProfile(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& name,
    const base::android::JavaParamRef<jobject>& java_profile,
    jboolean is_incognito) {
  return reinterpret_cast<jlong>(
      new ProfileImpl(env, name, java_profile, is_incognito));
}

static void JNI_ProfileImpl_DeleteProfile(JNIEnv* env, jlong profile) {
  delete reinterpret_cast<ProfileImpl*>(profile);
}

static void JNI_ProfileImpl_EnumerateAllProfileNames(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ListProfileNames),
      base::BindOnce(&PassFilePathsToJavaCallback,
                     base::android::ScopedJavaGlobalRef<jobject>(callback)));
}

jint ProfileImpl::GetNumBrowserImpl(JNIEnv* env) {
  return GetNumberOfBrowsers();
}

jlong ProfileImpl::GetBrowserContext(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(GetBrowserContext());
}

void ProfileImpl::DestroyAndDeleteDataFromDisk(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_completion_callback) {
  std::unique_ptr<ProfileImpl> ptr(this);
  std::unique_ptr<ProfileImpl> result =
      ProfileImpl::DestroyAndDeleteDataFromDisk(
          std::move(ptr),
          base::BindOnce(&base::android::RunRunnableAndroid,
                         base::android::ScopedJavaGlobalRef<jobject>(
                             j_completion_callback)));
  CHECK(!result);
}

void ProfileImpl::ClearBrowsingData(
    JNIEnv* env,
    const base::android::JavaParamRef<jintArray>& j_data_types,
    const jlong j_from_time_millis,
    const jlong j_to_time_millis,
    const base::android::JavaRef<jobject>& j_callback) {
  std::vector<int> data_type_ints;
  base::android::JavaIntArrayToIntVector(env, j_data_types, &data_type_ints);
  std::vector<BrowsingDataType> data_types;
  data_types.reserve(data_type_ints.size());
  for (int type : data_type_ints)
    data_types.push_back(static_cast<BrowsingDataType>(type));
  ClearBrowsingData(
      data_types,
      base::Time::FromJavaTime(static_cast<int64_t>(j_from_time_millis)),
      base::Time::FromJavaTime(static_cast<int64_t>(j_to_time_millis)),
      base::BindOnce(base::android::RunRunnableAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
}

void ProfileImpl::SetDownloadDirectory(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& directory) {
  base::FilePath directory_path(
      base::android::ConvertJavaStringToUTF8(directory));

  SetDownloadDirectory(directory_path);
}

jlong ProfileImpl::GetCookieManager(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetCookieManager());
}

jlong ProfileImpl::GetPrerenderController(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetPrerenderController());
}

void ProfileImpl::EnsureBrowserContextInitialized(JNIEnv* env) {
  GetBrowserContext()->GetDownloadManager();
}

void ProfileImpl::SetBooleanSetting(JNIEnv* env,
                                    jint j_type,
                                    jboolean j_value) {
  SetBooleanSetting(static_cast<SettingType>(j_type), j_value);
}

jboolean ProfileImpl::GetBooleanSetting(JNIEnv* env, jint j_type) {
  return GetBooleanSetting(static_cast<SettingType>(j_type));
}

void ProfileImpl::GetBrowserPersistenceIds(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_callback) {
  GetBrowserPersistenceIds(
      base::BindOnce(&OnGotBrowserPersistenceIds,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
}

void ProfileImpl::RemoveBrowserPersistenceStorage(
    JNIEnv* env,
    const base::android::JavaRef<jobjectArray>& j_ids,
    const base::android::JavaRef<jobject>& j_callback) {
  std::vector<std::string> ids;
  base::android::AppendJavaStringArrayToStringVector(env, j_ids, &ids);
  RemoveBrowserPersistenceStorage(
      base::BindOnce(&OnDidRemoveBrowserPersistenceStorage,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback)),
      base::flat_set<std::string>(ids.begin(), ids.end()));
}

void ProfileImpl::PrepareForPossibleCrossOriginNavigation(JNIEnv* env) {
  PrepareForPossibleCrossOriginNavigation();
}

void ProfileImpl::GetCachedFaviconForPageUrl(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& j_page_url,
    const base::android::JavaRef<jobject>& j_callback) {
  GetCachedFaviconForPageUrl(
      GURL(base::android::ConvertJavaStringToUTF8(j_page_url)),
      base::BindOnce(&OnDidGetCachedFaviconForPageUrl,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
}

#endif  // BUILDFLAG(IS_ANDROID)

base::FilePath ProfileImpl::GetBrowserPersisterDataBaseDir() const {
  return ComputeBrowserPersisterDataBaseDir(info_);
}

content::WebContents* ProfileImpl::OpenUrl(
    const content::OpenURLParams& params) {
#if !BUILDFLAG(IS_ANDROID)
  return nullptr;
#else
  // We expect only NEW_FOREGROUND_TAB. The NEW_POPUP disposition is only used
  // for payment handler windows, but WebLayer (and Android Chrome) do not
  // support that. See ContentBrowserClient::ShowPaymentHandlerWindow().
  DCHECK_EQ(params.disposition, WindowOpenDisposition::NEW_FOREGROUND_TAB);

  JNIEnv* env = base::android::AttachCurrentThread();
  BrowserImpl* browser = reinterpret_cast<BrowserImpl*>(
      Java_ProfileImpl_getBrowserForNewTab(env, java_profile_));
  if (!browser)
    return nullptr;

  std::unique_ptr<content::WebContents> new_tab_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetBrowserContext()));
  base::WeakPtr<content::WebContents> new_tab_contents_weak_ptr(
      new_tab_contents->GetWeakPtr());
  Tab* tab = browser->CreateTab(std::move(new_tab_contents));

  if (!new_tab_contents_weak_ptr)
    return nullptr;

  Java_ProfileImpl_onTabAdded(env, java_profile_,
                              static_cast<TabImpl*>(tab)->GetJavaTab());
  new_tab_contents_weak_ptr->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(params));
  return new_tab_contents_weak_ptr.get();
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ProfileImpl::SetBooleanSetting(SettingType type, bool value) {
  auto* pref_service = GetBrowserContext()->pref_service();
  switch (type) {
    case SettingType::BASIC_SAFE_BROWSING_ENABLED:
#if BUILDFLAG(IS_ANDROID)
      safe_browsing::SetSafeBrowsingState(
          pref_service,
          value ? safe_browsing::SafeBrowsingState::STANDARD_PROTECTION
                : safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING,
          /*is_esb_enabled_in_sync=*/false);
#endif
      break;
    case SettingType::UKM_ENABLED: {
#if BUILDFLAG(IS_ANDROID)
      bool old_value = pref_service->GetBoolean(prefs::kUkmEnabled);
#endif
      pref_service->SetBoolean(prefs::kUkmEnabled, value);
#if BUILDFLAG(IS_ANDROID)
      // Trigger a purge if the current state no longer allows UKM.
      bool must_purge = old_value && !value;
      WebLayerMetricsServiceClient::GetInstance()->UpdateUkm(must_purge);
#endif
      break;
    }
    case SettingType::EXTENDED_REPORTING_SAFE_BROWSING_ENABLED:
#if BUILDFLAG(IS_ANDROID)
      pref_service->SetBoolean(::prefs::kSafeBrowsingScoutReportingEnabled,
                               value);
#endif
      break;
    case SettingType::REAL_TIME_SAFE_BROWSING_ENABLED:
#if BUILDFLAG(IS_ANDROID)
      pref_service->SetBoolean(
          unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
          value);
#endif
      break;
    case SettingType::NETWORK_PREDICTION_ENABLED:
      pref_service->SetBoolean(prefs::kNoStatePrefetchEnabled, value);
  }
}

bool ProfileImpl::GetBooleanSetting(SettingType type) {
  auto* pref_service = GetBrowserContext()->pref_service();
  switch (type) {
    case SettingType::BASIC_SAFE_BROWSING_ENABLED:
#if BUILDFLAG(IS_ANDROID)
      return safe_browsing::IsSafeBrowsingEnabled(*pref_service);
#else
      return false;
#endif
    case SettingType::UKM_ENABLED:
      return pref_service->GetBoolean(prefs::kUkmEnabled);
    case SettingType::EXTENDED_REPORTING_SAFE_BROWSING_ENABLED:
#if BUILDFLAG(IS_ANDROID)
      return pref_service->GetBoolean(
          ::prefs::kSafeBrowsingScoutReportingEnabled);
#else
      return false;
#endif
    case SettingType::REAL_TIME_SAFE_BROWSING_ENABLED:
#if BUILDFLAG(IS_ANDROID)
      return pref_service->GetBoolean(
          unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
#else
      return false;
#endif
    case SettingType::NETWORK_PREDICTION_ENABLED:
      return pref_service->GetBoolean(prefs::kNoStatePrefetchEnabled);
  }
  NOTREACHED();
}

void ProfileImpl::GetCachedFaviconForPageUrl(
    const GURL& page_url,
    base::OnceCallback<void(gfx::Image)> callback) {
  auto* service =
      FaviconServiceImplFactory::GetForBrowserContext(GetBrowserContext());
  if (!service) {
    std::move(callback).Run({});
    return;
  }

  service->GetFaviconForPageUrl(page_url, std::move(callback),
                                &cancelable_task_tracker_);
}

void ProfileImpl::PrepareForPossibleCrossOriginNavigation() {
  content::RenderProcessHost::WarmupSpareRenderProcessHost(GetBrowserContext());
}

int ProfileImpl::GetNumberOfBrowsers() {
  const auto& browsers = BrowserList::GetInstance()->browsers();
  return base::ranges::count(browsers, this, &BrowserImpl::profile);
}

void ProfileImpl::DeleteScheduleWebContents() {
  web_contents_to_delete_.clear();
}

}  // namespace weblayer
