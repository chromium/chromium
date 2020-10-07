// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_impl.h"

#include <algorithm>

#include "base/callback_forward.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "components/base32/base32.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_list.h"
#include "weblayer/browser/feature_list_creator.h"
#include "weblayer/browser/persistence/browser_persister.h"
#include "weblayer/browser/persistence/browser_persister_file_utils.h"
#include "weblayer/browser/persistence/minimal_browser_persister.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/common/weblayer_paths.h"
#include "weblayer/public/browser_observer.h"
#include "weblayer/public/browser_restore_observer.h"

#if defined(OS_ANDROID)
#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/json/json_writer.h"
#include "components/metrics/metrics_service.h"
#include "components/ukm/ukm_service.h"
#include "weblayer/browser/android/metrics/weblayer_metrics_service_client.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/java/jni/BrowserImpl_jni.h"
#endif

#if defined(OS_ANDROID)
using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
#endif

namespace weblayer {

namespace {

#if defined(OS_ANDROID)
void UpdateMetricsService() {
  static bool s_foreground = false;
  // TODO(sky): convert this to observer.
  bool foreground = BrowserList::GetInstance()->HasAtLeastOneResumedBrowser();

  if (foreground == s_foreground)
    return;

  s_foreground = foreground;

  auto* metrics_service =
      WebLayerMetricsServiceClient::GetInstance()->GetMetricsService();
  if (metrics_service) {
    if (foreground)
      metrics_service->OnAppEnterForeground();
    else
      metrics_service->OnAppEnterBackground();
  }

  auto* ukm_service =
      WebLayerMetricsServiceClient::GetInstance()->GetUkmService();
  if (ukm_service) {
    if (foreground)
      ukm_service->OnAppEnterForeground();
    else
      ukm_service->OnAppEnterBackground();
  }
}
#endif  // defined(OS_ANDROID)

}  // namespace

// static
constexpr char BrowserImpl::kPersistenceFilePrefix[];

std::unique_ptr<Browser> Browser::Create(
    Profile* profile,
    const PersistenceInfo* persistence_info) {
  // BrowserImpl's constructor is private.
  auto browser =
      base::WrapUnique(new BrowserImpl(static_cast<ProfileImpl*>(profile)));
  if (persistence_info)
    browser->RestoreStateIfNecessary(*persistence_info);
  return browser;
}

BrowserImpl::~BrowserImpl() {
#if defined(OS_ANDROID)
  // Android side should always remove tabs first (because the Java Tab class
  // owns the C++ Tab). See BrowserImpl.destroy() (in the Java BrowserImpl
  // class).
  DCHECK(tabs_.empty());
#else
  while (!tabs_.empty())
    DestroyTab(tabs_.back().get());
#endif
  BrowserList::GetInstance()->RemoveBrowser(this);

#if defined(OS_ANDROID)
  if (BrowserList::GetInstance()->browsers().empty())
    BrowserProcess::GetInstance()->StopSafeBrowsingService();
#endif
}

TabImpl* BrowserImpl::CreateTabForSessionRestore(
    std::unique_ptr<content::WebContents> web_contents,
    const std::string& guid) {
  if (!web_contents) {
    content::WebContents::CreateParams create_params(
        profile_->GetBrowserContext());
    web_contents = content::WebContents::Create(create_params);
  }
  std::unique_ptr<TabImpl> tab =
      std::make_unique<TabImpl>(profile_, std::move(web_contents), guid);
#if defined(OS_ANDROID)
  Java_BrowserImpl_createJavaTabForNativeTab(
      AttachCurrentThread(), java_impl_, reinterpret_cast<jlong>(tab.get()));
#endif
  return AddTab(std::move(tab));
}

TabImpl* BrowserImpl::CreateTab(
    std::unique_ptr<content::WebContents> web_contents) {
  return CreateTabForSessionRestore(std::move(web_contents), std::string());
}

#if defined(OS_ANDROID)
bool BrowserImpl::CompositorHasSurface() {
  return Java_BrowserImpl_compositorHasSurface(AttachCurrentThread(),
                                               java_impl_);
}

void BrowserImpl::AddTab(JNIEnv* env,
                         long native_tab) {
  AddTab(reinterpret_cast<TabImpl*>(native_tab));
}

ScopedJavaLocalRef<jobjectArray> BrowserImpl::GetTabs(JNIEnv* env) {
  ScopedJavaLocalRef<jclass> clazz =
      base::android::GetClass(env, "org/chromium/weblayer_private/TabImpl");
  jobjectArray tabs = env->NewObjectArray(tabs_.size(), clazz.obj(),
                                          nullptr /* initialElement */);
  base::android::CheckException(env);

  for (size_t i = 0; i < tabs_.size(); ++i) {
    TabImpl* tab = static_cast<TabImpl*>(tabs_[i].get());
    env->SetObjectArrayElement(tabs, i, tab->GetJavaTab().obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, tabs);
}

void BrowserImpl::SetActiveTab(JNIEnv* env,
                               long native_tab) {
  SetActiveTab(reinterpret_cast<TabImpl*>(native_tab));
}

ScopedJavaLocalRef<jobject> BrowserImpl::GetActiveTab(JNIEnv* env) {
  if (!active_tab_)
    return nullptr;
  return ScopedJavaLocalRef<jobject>(active_tab_->GetJavaTab());
}

void BrowserImpl::PrepareForShutdown(JNIEnv* env) {
  PrepareForShutdown();
}

ScopedJavaLocalRef<jstring> BrowserImpl::GetPersistenceId(JNIEnv* env) {
  return ScopedJavaLocalRef<jstring>(
      base::android::ConvertUTF8ToJavaString(env, GetPersistenceId()));
}

void BrowserImpl::SaveBrowserPersisterIfNecessary(JNIEnv* env) {
  browser_persister_->SaveIfNecessary();
}

ScopedJavaLocalRef<jbyteArray> BrowserImpl::GetBrowserPersisterCryptoKey(
    JNIEnv* env) {
  std::vector<uint8_t> key;
  if (browser_persister_)
    key = browser_persister_->GetCryptoKey();
  return base::android::ToJavaByteArray(env, key);
}

ScopedJavaLocalRef<jbyteArray> BrowserImpl::GetMinimalPersistenceState(
    JNIEnv* env) {
  return base::android::ToJavaByteArray(env, GetMinimalPersistenceState());
}

void BrowserImpl::RestoreStateIfNecessary(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_persistence_id,
    const JavaParamRef<jbyteArray>& j_persistence_crypto_key,
    const JavaParamRef<jbyteArray>& j_minimal_persistence_state) {
  Browser::PersistenceInfo persistence_info;
  Browser::PersistenceInfo* persistence_info_ptr = nullptr;

  if (j_persistence_id.obj()) {
    const std::string persistence_id =
        base::android::ConvertJavaStringToUTF8(j_persistence_id);
    if (!persistence_id.empty()) {
      persistence_info.id = persistence_id;
      if (j_persistence_crypto_key.obj()) {
        base::android::JavaByteArrayToByteVector(
            env, j_persistence_crypto_key, &(persistence_info.last_crypto_key));
      }
      persistence_info_ptr = &persistence_info;
    }
  } else if (j_minimal_persistence_state.obj()) {
    base::android::JavaByteArrayToByteVector(env, j_minimal_persistence_state,
                                             &(persistence_info.minimal_state));
    persistence_info_ptr = &persistence_info;
  }
  if (persistence_info_ptr)
    RestoreStateIfNecessary(*persistence_info_ptr);
}

void BrowserImpl::WebPreferencesChanged(JNIEnv* env) {
  for (const auto& tab : tabs_) {
    TabImpl* tab_impl = static_cast<TabImpl*>(tab.get());
    tab_impl->WebPreferencesChanged();
  }
}

void BrowserImpl::OnFragmentStart(JNIEnv* env) {
  // FeatureListCreator is created before any Browsers.
  DCHECK(FeatureListCreator::GetInstance());
  FeatureListCreator::GetInstance()->OnBrowserFragmentStarted();
}

void BrowserImpl::OnFragmentResume(JNIEnv* env) {
  UpdateFragmentResumedState(true);
}

void BrowserImpl::OnFragmentPause(JNIEnv* env) {
  UpdateFragmentResumedState(false);
}

#endif

std::vector<uint8_t> BrowserImpl::GetMinimalPersistenceState(
    int max_size_in_bytes) {
  return PersistMinimalState(this, max_size_in_bytes);
}

void BrowserImpl::SetWebPreferences(blink::web_pref::WebPreferences* prefs) {
#if defined(OS_ANDROID)
  prefs->password_echo_enabled = Java_BrowserImpl_getPasswordEchoEnabled(
      AttachCurrentThread(), java_impl_);
  prefs->preferred_color_scheme =
      Java_BrowserImpl_getDarkThemeEnabled(AttachCurrentThread(), java_impl_)
          ? blink::PreferredColorScheme::kDark
          : blink::PreferredColorScheme::kLight;
  prefs->font_scale_factor =
      Java_BrowserImpl_getFontScale(AttachCurrentThread(), java_impl_);
#endif
}

#if defined(OS_ANDROID)
void BrowserImpl::RemoveTabBeforeDestroyingFromJava(Tab* tab) {
  // The java side owns the Tab, and is going to delete it shortly. See
  // JNI_TabImpl_DeleteTab.
  RemoveTab(tab).release();
}
#endif

void BrowserImpl::AddTab(Tab* tab) {
  DCHECK(tab);
  TabImpl* tab_impl = static_cast<TabImpl*>(tab);
  std::unique_ptr<Tab> owned_tab;
  if (tab_impl->browser())
    owned_tab = tab_impl->browser()->RemoveTab(tab_impl);
  else
    owned_tab.reset(tab_impl);
  AddTab(std::move(owned_tab));
}

void BrowserImpl::DestroyTab(Tab* tab) {
#if defined(OS_ANDROID)
  // Route destruction through the java side.
  Java_BrowserImpl_destroyTabImpl(AttachCurrentThread(), java_impl_,
                                  static_cast<TabImpl*>(tab)->GetJavaTab());
#else
  RemoveTab(tab);
#endif
}

void BrowserImpl::SetActiveTab(Tab* tab) {
  if (GetActiveTab() == tab)
    return;
  if (active_tab_)
    active_tab_->OnLosingActive();
  // TODO: currently the java side sets visibility, this code likely should
  // too and it should be removed from the java side.
  active_tab_ = static_cast<TabImpl*>(tab);
#if defined(OS_ANDROID)
  Java_BrowserImpl_onActiveTabChanged(
      AttachCurrentThread(), java_impl_,
      active_tab_ ? active_tab_->GetJavaTab() : nullptr);
#endif
  VisibleSecurityStateOfActiveTabChanged();
  for (BrowserObserver& obs : browser_observers_)
    obs.OnActiveTabChanged(active_tab_);
  if (active_tab_)
    active_tab_->web_contents()->GetController().LoadIfNecessary();
}

Tab* BrowserImpl::GetActiveTab() {
  return active_tab_;
}

std::vector<Tab*> BrowserImpl::GetTabs() {
  std::vector<Tab*> tabs(tabs_.size());
  for (size_t i = 0; i < tabs_.size(); ++i)
    tabs[i] = tabs_[i].get();
  return tabs;
}

Tab* BrowserImpl::CreateTab() {
  return CreateTab(nullptr);
}

void BrowserImpl::OnRestoreCompleted() {
  for (BrowserRestoreObserver& obs : browser_restore_observers_)
    obs.OnRestoreCompleted();
#if defined(OS_ANDROID)
  Java_BrowserImpl_onRestoreCompleted(AttachCurrentThread(), java_impl_);
#endif
}

void BrowserImpl::PrepareForShutdown() {
  browser_persister_.reset();
}

std::string BrowserImpl::GetPersistenceId() {
  return persistence_id_;
}

std::vector<uint8_t> BrowserImpl::GetMinimalPersistenceState() {
  // 0 means use the default max.
  return GetMinimalPersistenceState(0);
}

bool BrowserImpl::IsRestoringPreviousState() {
  return browser_persister_ && browser_persister_->is_restore_in_progress();
}

void BrowserImpl::AddObserver(BrowserObserver* observer) {
  browser_observers_.AddObserver(observer);
}

void BrowserImpl::RemoveObserver(BrowserObserver* observer) {
  browser_observers_.RemoveObserver(observer);
}

void BrowserImpl::AddBrowserRestoreObserver(BrowserRestoreObserver* observer) {
  browser_restore_observers_.AddObserver(observer);
}

void BrowserImpl::RemoveBrowserRestoreObserver(
    BrowserRestoreObserver* observer) {
  browser_restore_observers_.RemoveObserver(observer);
}

void BrowserImpl::VisibleSecurityStateOfActiveTabChanged() {
  if (visible_security_state_changed_callback_for_tests_)
    std::move(visible_security_state_changed_callback_for_tests_).Run();

#if defined(OS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BrowserImpl_onVisibleSecurityStateOfActiveTabChanged(env, java_impl_);
#endif
}

BrowserImpl::BrowserImpl(ProfileImpl* profile) : profile_(profile) {
  BrowserList::GetInstance()->AddBrowser(this);
}

void BrowserImpl::RestoreStateIfNecessary(
    const PersistenceInfo& persistence_info) {
  persistence_id_ = persistence_info.id;
  if (!persistence_id_.empty()) {
    browser_persister_ = std::make_unique<BrowserPersister>(
        GetBrowserPersisterDataPath(), this, persistence_info.last_crypto_key);
  } else if (!persistence_info.minimal_state.empty()) {
    RestoreMinimalState(this, persistence_info.minimal_state);
  }
}

TabImpl* BrowserImpl::AddTab(std::unique_ptr<Tab> tab) {
  TabImpl* tab_impl = static_cast<TabImpl*>(tab.get());
  DCHECK(!tab_impl->browser());
  tabs_.push_back(std::move(tab));
  tab_impl->set_browser(this);
#if defined(OS_ANDROID)
  Java_BrowserImpl_onTabAdded(AttachCurrentThread(), java_impl_,
                              tab_impl->GetJavaTab());
#endif
  for (BrowserObserver& obs : browser_observers_)
    obs.OnTabAdded(tab_impl);
  return tab_impl;
}

std::unique_ptr<Tab> BrowserImpl::RemoveTab(Tab* tab) {
  TabImpl* tab_impl = static_cast<TabImpl*>(tab);
  DCHECK_EQ(this, tab_impl->browser());
  static_cast<TabImpl*>(tab)->set_browser(nullptr);
  auto iter =
      std::find_if(tabs_.begin(), tabs_.end(), base::MatchesUniquePtr(tab));
  DCHECK(iter != tabs_.end());
  std::unique_ptr<Tab> owned_tab = std::move(*iter);
  tabs_.erase(iter);
  const bool active_tab_changed = active_tab_ == tab;
  if (active_tab_changed)
    SetActiveTab(nullptr);

#if defined(OS_ANDROID)
  Java_BrowserImpl_onTabRemoved(AttachCurrentThread(), java_impl_,
                                tab ? tab_impl->GetJavaTab() : nullptr);
#endif
  for (BrowserObserver& obs : browser_observers_)
    obs.OnTabRemoved(tab, active_tab_changed);
  return owned_tab;
}

base::FilePath BrowserImpl::GetBrowserPersisterDataPath() {
  return BuildPathForBrowserPersister(
      profile_->GetBrowserPersisterDataBaseDir(), GetPersistenceId());
}

#if defined(OS_ANDROID)
void BrowserImpl::UpdateFragmentResumedState(bool state) {
  const bool old_has_at_least_one_active_browser =
      BrowserList::GetInstance()->HasAtLeastOneResumedBrowser();
  fragment_resumed_ = state;
  UpdateMetricsService();
  if (old_has_at_least_one_active_browser !=
      BrowserList::GetInstance()->HasAtLeastOneResumedBrowser()) {
    BrowserList::GetInstance()->NotifyHasAtLeastOneResumedBrowserChanged();
  }
}

// This function is friended. JNI_BrowserImpl_CreateBrowser can not be
// friended, as it requires browser_impl.h to include BrowserImpl_jni.h, which
// is problematic (meaning not really supported and generates compile errors).
BrowserImpl* CreateBrowserForAndroid(ProfileImpl* profile,
                                     const JavaParamRef<jobject>& java_impl) {
  BrowserImpl* browser = new BrowserImpl(profile);
  browser->java_impl_ = java_impl;
  return browser;
}

static jlong JNI_BrowserImpl_CreateBrowser(
    JNIEnv* env,
    jlong profile,
    const JavaParamRef<jobject>& java_impl) {
  // The android side does not trigger restore from the constructor as at the
  // time this is called not enough of WebLayer has been wired up. Specifically,
  // when this is called BrowserImpl.java hasn't obtained the return value so
  // that it can't call any functions and further the client side hasn't been
  // fully created, leading to all sort of assertions if Tabs are created
  // and/or navigations start (which restore may trigger).
  return reinterpret_cast<intptr_t>(CreateBrowserForAndroid(
      reinterpret_cast<ProfileImpl*>(profile), java_impl));
}

static void JNI_BrowserImpl_DeleteBrowser(JNIEnv* env, jlong browser) {
  delete reinterpret_cast<BrowserImpl*>(browser);
}
#endif

}  // namespace weblayer
