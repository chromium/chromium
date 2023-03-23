// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/tab_impl.h"

#include <cmath>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/task/thread_pool.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "cc/layers/layer.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/blocked_content/popup_blocker.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/blocked_content/popup_opener_tab_helper.h"
#include "components/blocked_content/popup_tracker.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/find_in_page/find_types.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_result.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_web_contents_helper.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webrtc/media_stream_devices_controller.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/renderer_preferences_util.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "weblayer/browser/autofill_client_impl.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_impl.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/content_browser_client_impl.h"
#include "weblayer/browser/favicon/favicon_fetcher_impl.h"
#include "weblayer/browser/favicon/favicon_tab_helper.h"
#include "weblayer/browser/file_select_helper.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/i18n_util.h"
#include "weblayer/browser/navigation_controller_impl.h"
#include "weblayer/browser/navigation_entry_data.h"
#include "weblayer/browser/no_state_prefetch/prerender_tab_helper.h"
#include "weblayer/browser/page_load_metrics_initialize.h"
#include "weblayer/browser/page_specific_content_settings_delegate.h"
#include "weblayer/browser/password_manager_driver_factory.h"
#include "weblayer/browser/persistence/browser_persister.h"
#include "weblayer/browser/popup_navigation_delegate_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/safe_browsing/safe_browsing_service.h"
#include "weblayer/browser/subresource_filter_profile_context_factory.h"
#include "weblayer/browser/translate_client_impl.h"
#include "weblayer/browser/weblayer_features.h"
#include "weblayer/common/isolated_world_ids.h"
#include "weblayer/public/fullscreen_delegate.h"
#include "weblayer/public/new_tab_delegate.h"
#include "weblayer/public/tab_observer.h"

#if !BUILDFLAG(IS_ANDROID)
#include "ui/views/controls/webview/webview.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/json/json_writer.h"
#include "base/trace_event/trace_event.h"
#include "components/android_autofill/browser/android_autofill_manager.h"
#include "components/android_autofill/browser/autofill_provider.h"
#include "components/android_autofill/browser/autofill_provider_android.h"
#include "components/browser_ui/sms/android/sms_infobar.h"
#include "components/download/content/public/context_menu_download.h"
#include "components/embedder_support/android/contextmenu/context_menu_builder.h"
#include "components/embedder_support/android/delegate/color_chooser_android.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"  // nogncheck
#include "components/safe_browsing/android/remote_database_manager.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer.h"
#include "components/safe_browsing/content/browser/safe_browsing_tab_observer.h"
#include "components/site_engagement/content/site_engagement_helper.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/translate/core/browser/translate_manager.h"
#include "ui/android/view_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "weblayer/browser/java/jni/TabImpl_jni.h"
#include "weblayer/browser/javascript_tab_modal_dialog_manager_delegate_android.h"
#include "weblayer/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "weblayer/browser/safe_browsing/weblayer_safe_browsing_tab_observer_delegate.h"
#include "weblayer/browser/translate_client_impl.h"
#include "weblayer/browser/webapps/weblayer_app_banner_manager_android.h"
#include "weblayer/browser/weblayer_factory_impl_android.h"
#include "weblayer/browser/webrtc/media_stream_manager.h"
#include "weblayer/common/features.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#include "weblayer/browser/captive_portal_service_factory.h"
#endif

#if BUILDFLAG(IS_ANDROID)
using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
#endif

namespace weblayer {

namespace {

// Maximum size of data when calling SetData().
constexpr int kMaxDataSize = 4096;

#if BUILDFLAG(IS_ANDROID)
bool g_system_autofill_disabled_for_testing = false;

#endif

NewTabType NewTabTypeFromWindowDisposition(WindowOpenDisposition disposition) {
  // WindowOpenDisposition has a *ton* of types, but the following are really
  // the only ones that should be hit for this code path.
  switch (disposition) {
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
      return NewTabType::kForeground;
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      return NewTabType::kBackground;
    case WindowOpenDisposition::NEW_POPUP:
      return NewTabType::kNewPopup;
    case WindowOpenDisposition::NEW_WINDOW:
      return NewTabType::kNewWindow;
    default:
      // The set of allowed types are in
      // ContentTabClientImpl::CanCreateWindow().
      NOTREACHED();
      return NewTabType::kForeground;
  }
}

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
// Opens a captive portal login page in |web_contents|.
void OpenCaptivePortalLoginTabInWebContents(
    content::WebContents* web_contents) {
  content::OpenURLParams params(
      CaptivePortalServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext())
          ->test_url(),
      content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false);
  web_contents->OpenURL(params);
}
#endif

// Pointer value of this is used as a key in base::SupportsUserData for
// WebContents. Value of the key is an instance of |UserData|.
constexpr int kWebContentsUserDataKey = 0;

struct UserData : public base::SupportsUserData::Data {
  raw_ptr<TabImpl> tab = nullptr;
};

#if BUILDFLAG(IS_ANDROID)
void HandleJavaScriptResult(const ScopedJavaGlobalRef<jobject>& callback,
                            base::Value result) {
  std::string json;
  base::JSONWriter::Write(result, &json);
  base::android::RunStringCallbackAndroid(callback, json);
}

void OnConvertedToJavaBitmap(const ScopedJavaGlobalRef<jobject>& value_callback,
                             const ScopedJavaGlobalRef<jobject>& java_bitmap) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  TabImpl::ScreenShotErrors error =
      java_bitmap ? TabImpl::ScreenShotErrors::kNone
                  : TabImpl::ScreenShotErrors::kBitmapAllocationFailed;
  Java_TabImpl_runCaptureScreenShotCallback(AttachCurrentThread(),
                                            value_callback, java_bitmap,
                                            static_cast<int>(error));
}

// Convert SkBitmap to java Bitmap on a background thread since it involves a
// memcpy.
void ConvertToJavaBitmapBackgroundThread(
    const SkBitmap& bitmap,
    base::OnceCallback<void(const ScopedJavaGlobalRef<jobject>&)> callback) {
  // Make sure to only pass ScopedJavaGlobalRef between threads.
  ScopedJavaGlobalRef<jobject> java_bitmap = ScopedJavaGlobalRef<jobject>(
      gfx::ConvertToJavaBitmap(bitmap, gfx::OomBehavior::kReturnNullOnOom));
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(java_bitmap)));
}

void OnScreenShotCaptured(const ScopedJavaGlobalRef<jobject>& value_callback,
                          const SkBitmap& bitmap) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (bitmap.isNull() || bitmap.drawsNothing()) {
    Java_TabImpl_runCaptureScreenShotCallback(
        AttachCurrentThread(), value_callback, nullptr,
        static_cast<int>(TabImpl::ScreenShotErrors::kCaptureFailed));
    return;
  }
  // Not using PostTaskAndReplyWithResult to ensure ScopedJavaLocalRef is not
  // passed between threads.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ConvertToJavaBitmapBackgroundThread, bitmap,
                     base::BindOnce(&OnConvertedToJavaBitmap, value_callback)));
}

#endif  // BUILDFLAG(IS_ANDROID)

std::set<TabImpl*>& GetTabs() {
  static base::NoDestructor<std::set<TabImpl*>> s_all_tab_impl;
  return *s_all_tab_impl;
}

// Returns a scoped refptr to the SafeBrowsingService's database manager, if
// available. Otherwise returns nullptr.
const scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
GetDatabaseManagerFromSafeBrowsingService() {
#if BUILDFLAG(IS_ANDROID)
  SafeBrowsingService* safe_browsing_service =
      BrowserProcess::GetInstance()->GetSafeBrowsingService();
  return scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>(
      safe_browsing_service->GetSafeBrowsingDBManager());
#else
  return nullptr;
#endif
}

// Creates a ContentSubresourceFilterWebContentsHelper for |web_contents|,
// passing it the needed embedder-level state.
void CreateContentSubresourceFilterWebContentsHelper(
    content::WebContents* web_contents) {
  subresource_filter::RulesetService* ruleset_service =
      BrowserProcess::GetInstance()->subresource_filter_ruleset_service();
  subresource_filter::VerifiedRulesetDealer::Handle* dealer =
      ruleset_service ? ruleset_service->GetRulesetDealer() : nullptr;
  subresource_filter::ContentSubresourceFilterWebContentsHelper::
      CreateForWebContents(
          web_contents,
          SubresourceFilterProfileContextFactory::GetForBrowserContext(
              web_contents->GetBrowserContext()),
          GetDatabaseManagerFromSafeBrowsingService(), dealer);
}

}  // namespace

#if BUILDFLAG(IS_ANDROID)

static ScopedJavaLocalRef<jobject> JNI_TabImpl_FromWebContents(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  TabImpl* tab = TabImpl::FromWebContents(web_contents);
  if (tab)
    return ScopedJavaLocalRef<jobject>(tab->GetJavaTab());
  return nullptr;
}

static void JNI_TabImpl_DestroyContextMenuParams(
    JNIEnv* env,
    jlong native_context_menu_params) {
  // Note: this runs on the finalizer thread which isn't the UI thread.
  auto* context_menu_params =
      reinterpret_cast<content::ContextMenuParams*>(native_context_menu_params);
  delete context_menu_params;
}

TabImpl::TabImpl(ProfileImpl* profile,
                 const JavaParamRef<jobject>& java_impl,
                 std::unique_ptr<content::WebContents> web_contents)
    : TabImpl(profile, std::move(web_contents)) {
  java_impl_ = java_impl;
}
#endif

TabImpl::TabImpl(ProfileImpl* profile,
                 std::unique_ptr<content::WebContents> web_contents,
                 const std::string& guid)
    : profile_(profile),
      web_contents_(std::move(web_contents)),
      guid_(guid.empty() ? base::GenerateGUID() : guid) {
  GetTabs().insert(this);
  DCHECK(web_contents_);
  // This code path is hit when the page requests a new tab, which should
  // only be possible from the same profile.
  DCHECK_EQ(profile_->GetBrowserContext(), web_contents_->GetBrowserContext());

  // FaviconTabHelper adds a WebContentsObserver. Create FaviconTabHelper
  // before |this| observes the WebContents to ensure favicons are reset before
  // notifying weblayer observers of changes.
  FaviconTabHelper::CreateForWebContents(web_contents_.get());

  UpdateRendererPrefs(false);
  locale_change_subscription_ =
      i18n::RegisterLocaleChangeCallback(base::BindRepeating(
          &TabImpl::UpdateRendererPrefs, base::Unretained(this), true));

  std::unique_ptr<UserData> user_data = std::make_unique<UserData>();
  user_data->tab = this;
  web_contents_->SetUserData(&kWebContentsUserDataKey, std::move(user_data));

  web_contents_->SetDelegate(this);
  Observe(web_contents_.get());

  navigation_controller_ = std::make_unique<NavigationControllerImpl>(this);

  find_in_page::FindTabHelper::CreateForWebContents(web_contents_.get());
  GetFindTabHelper()->AddObserver(this);

  TranslateClientImpl::CreateForWebContents(web_contents_.get());

#if BUILDFLAG(IS_ANDROID)
  // infobars::ContentInfoBarManager must be created before
  // SubresourceFilterClientImpl as the latter depends on it.
  infobars::ContentInfoBarManager::CreateForWebContents(web_contents_.get());
#endif

  CreateContentSubresourceFilterWebContentsHelper(web_contents_.get());

  sessions::SessionTabHelper::CreateForWebContents(
      web_contents_.get(),
      base::BindRepeating(&TabImpl::GetSessionServiceTabHelperDelegate));

  permissions::PermissionRequestManager::CreateForWebContents(
      web_contents_.get());
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents_.get(),
      std::make_unique<PageSpecificContentSettingsDelegate>(
          web_contents_.get()));
  blocked_content::PopupBlockerTabHelper::CreateForWebContents(
      web_contents_.get());
  blocked_content::PopupOpenerTabHelper::CreateForWebContents(
      web_contents_.get(), base::DefaultTickClock::GetInstance(),
      HostContentSettingsMapFactory::GetForBrowserContext(
          web_contents_->GetBrowserContext()));
  PasswordManagerDriverFactory::CreateForWebContents(web_contents_.get());

  InitializePageLoadMetricsForWebContents(web_contents_.get());
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents_.get());

#if BUILDFLAG(IS_ANDROID)
  javascript_dialogs::TabModalDialogManager::CreateForWebContents(
      web_contents_.get(),
      std::make_unique<JavaScriptTabModalDialogManagerDelegateAndroid>(
          web_contents_.get()));

  if (base::FeatureList::IsEnabled(
          features::kWebLayerClientSidePhishingDetection)) {
    safe_browsing::SafeBrowsingTabObserver::CreateForWebContents(
        web_contents_.get(),
        std::make_unique<WebLayerSafeBrowsingTabObserverDelegate>());
  }

  if (site_engagement::SiteEngagementService::IsEnabled()) {
    site_engagement::SiteEngagementService::Helper::CreateForWebContents(
        web_contents_.get());
  }

  auto* browser_context =
      static_cast<BrowserContextImpl*>(web_contents_->GetBrowserContext());
  safe_browsing::SafeBrowsingNavigationObserver::MaybeCreateForWebContents(
      web_contents_.get(),
      HostContentSettingsMapFactory::GetForBrowserContext(browser_context),
      SafeBrowsingNavigationObserverManagerFactory::GetForBrowserContext(
          browser_context),
      browser_context->pref_service(),
      BrowserProcess::GetInstance()->GetSafeBrowsingService());
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  captive_portal::CaptivePortalTabHelper::CreateForWebContents(
      web_contents_.get(),
      CaptivePortalServiceFactory::GetForBrowserContext(
          web_contents_->GetBrowserContext()),
      base::BindRepeating(&OpenCaptivePortalLoginTabInWebContents,
                          web_contents_.get()));
#endif

  // PrerenderTabHelper adds a WebContentsObserver.
  PrerenderTabHelper::CreateForWebContents(web_contents_.get());

  webapps::InstallableManager::CreateForWebContents(web_contents_.get());

#if BUILDFLAG(IS_ANDROID)
  // Must be created after InstallableManager.
  WebLayerAppBannerManagerAndroid::CreateForWebContents(web_contents_.get());
#endif
}

TabImpl::~TabImpl() {
  DCHECK(!browser_);

  GetFindTabHelper()->RemoveObserver(this);

  Observe(nullptr);
  web_contents_->SetDelegate(nullptr);
  if (navigation_controller_->should_delay_web_contents_deletion()) {
    // Some user-data on WebContents directly or indirectly references this.
    // Remove that linkage to avoid use-after-free.
    web_contents_->RemoveUserData(&kWebContentsUserDataKey);
    // Have Profile handle the task posting to ensure the WebContents is
    // deleted before Profile. To do otherwise means it would be possible for
    // the Profile to outlive the WebContents, which is problematic (crash).
    profile_->DeleteWebContentsSoon(std::move(web_contents_));
  }
  web_contents_.reset();
  GetTabs().erase(this);
}

// static
TabImpl* TabImpl::FromWebContents(content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  UserData* user_data = reinterpret_cast<UserData*>(
      web_contents->GetUserData(&kWebContentsUserDataKey));
  return user_data ? user_data->tab.get() : nullptr;
}

// static
std::set<TabImpl*> TabImpl::GetAllTabImpl() {
  return GetTabs();
}

void TabImpl::AddDataObserver(DataObserver* observer) {
  data_observers_.AddObserver(observer);
}

void TabImpl::RemoveDataObserver(DataObserver* observer) {
  data_observers_.RemoveObserver(observer);
}

Browser* TabImpl::GetBrowser() {
  return browser_;
}

void TabImpl::SetErrorPageDelegate(ErrorPageDelegate* delegate) {
  error_page_delegate_ = delegate;
}

void TabImpl::SetFullscreenDelegate(FullscreenDelegate* delegate) {
  if (delegate == fullscreen_delegate_)
    return;

  const bool had_delegate = (fullscreen_delegate_ != nullptr);
  const bool has_delegate = (delegate != nullptr);

  // If currently fullscreen, and the delegate is being set to null, force an
  // exit now (otherwise the delegate can't take us out of fullscreen).
  if (is_fullscreen_ && fullscreen_delegate_ && had_delegate != has_delegate)
    OnExitFullscreen();

  fullscreen_delegate_ = delegate;
  // Whether fullscreen is enabled depends upon whether there is a delegate. If
  // having a delegate changed, then update the renderer (which is where
  // fullscreen enabled is tracked).
  if (had_delegate != has_delegate)
    web_contents_->OnWebPreferencesChanged();
}

void TabImpl::SetNewTabDelegate(NewTabDelegate* delegate) {
  new_tab_delegate_ = delegate;
}

void TabImpl::SetGoogleAccountsDelegate(GoogleAccountsDelegate* delegate) {
  google_accounts_delegate_ = delegate;
}

void TabImpl::AddObserver(TabObserver* observer) {
  observers_.AddObserver(observer);
}

void TabImpl::RemoveObserver(TabObserver* observer) {
  observers_.RemoveObserver(observer);
}

NavigationController* TabImpl::GetNavigationController() {
  return navigation_controller_.get();
}

void TabImpl::ExecuteScript(const std::u16string& script,
                            bool use_separate_isolate,
                            JavaScriptResultCallback callback) {
  if (use_separate_isolate) {
    web_contents_->GetPrimaryMainFrame()->ExecuteJavaScriptInIsolatedWorld(
        script, std::move(callback), ISOLATED_WORLD_ID_WEBLAYER);
  } else {
    content::RenderFrameHost::AllowInjectingJavaScript();
    web_contents_->GetPrimaryMainFrame()->ExecuteJavaScript(
        script, std::move(callback));
  }
}

const std::string& TabImpl::GetGuid() {
  return guid_;
}

void TabImpl::SetData(const std::map<std::string, std::string>& data) {
  bool result = SetDataInternal(data);
  DCHECK(result) << "Data given to SetData() was too large.";
}

const std::map<std::string, std::string>& TabImpl::GetData() {
  return data_;
}

void TabImpl::ExecuteScriptWithUserGestureForTests(
    const std::u16string& script) {
  web_contents_->GetPrimaryMainFrame()
      ->ExecuteJavaScriptWithUserGestureForTests(script, base::NullCallback());
}

std::unique_ptr<FaviconFetcher> TabImpl::CreateFaviconFetcher(
    FaviconFetcherDelegate* delegate) {
  return std::make_unique<FaviconFetcherImpl>(web_contents_.get(), delegate);
}

void TabImpl::SetTranslateTargetLanguage(
    const std::string& translate_target_lang) {
  translate::TranslateManager* translate_manager =
      TranslateClientImpl::FromWebContents(web_contents())
          ->GetTranslateManager();
  translate_manager->SetPredefinedTargetLanguage(
      translate_target_lang,
      /*should_auto_translate=*/true);
}

#if !BUILDFLAG(IS_ANDROID)
void TabImpl::AttachToView(views::WebView* web_view) {
  web_view->SetWebContents(web_contents_.get());
  web_contents_->Focus();
}
#endif

void TabImpl::WebPreferencesChanged() {
  web_contents_->OnWebPreferencesChanged();
}

void TabImpl::SetWebPreferences(blink::web_pref::WebPreferences* prefs) {
  prefs->fullscreen_supported = !!fullscreen_delegate_;

  if (!browser_)
    return;
  browser_->SetWebPreferences(prefs);
}

void TabImpl::OnGainedActive() {
  web_contents()->GetController().LoadIfNecessary();
  if (enter_fullscreen_on_gained_active_)
    EnterFullscreenImpl();
}

void TabImpl::OnLosingActive() {
  if (is_fullscreen_)
    web_contents_->ExitFullscreen(/* will_cause_resize */ false);
}

bool TabImpl::IsActive() {
  return browser_->GetActiveTab() == this;
}

void TabImpl::ShowContextMenu(const content::ContextMenuParams& params) {
#if BUILDFLAG(IS_ANDROID)
  Java_TabImpl_showContextMenu(
      base::android::AttachCurrentThread(), java_impl_,
      context_menu::BuildJavaContextMenuParams(params),
      reinterpret_cast<jlong>(new content::ContextMenuParams(params)));
#endif
}

#if BUILDFLAG(IS_ANDROID)
// static
void TabImpl::DisableAutofillSystemIntegrationForTesting() {
  g_system_autofill_disabled_for_testing = true;
}

static jlong JNI_TabImpl_CreateTab(JNIEnv* env,
                                   jlong profile,
                                   const JavaParamRef<jobject>& java_impl) {
  ProfileImpl* profile_impl = reinterpret_cast<ProfileImpl*>(profile);
  content::WebContents::CreateParams create_params(
      profile_impl->GetBrowserContext());
  create_params.initially_hidden = true;
  return reinterpret_cast<intptr_t>(new TabImpl(
      profile_impl, java_impl, content::WebContents::Create(create_params)));
}

static void JNI_TabImpl_DeleteTab(JNIEnv* env, jlong tab) {
  TabImpl* tab_impl = reinterpret_cast<TabImpl*>(tab);
  DCHECK(tab_impl);
  // RemoveTabBeforeDestroyingFromJava() should have been called before this,
  // which sets browser to null.
  DCHECK(!tab_impl->browser());
  delete tab_impl;
}

ScopedJavaLocalRef<jobject> TabImpl::GetWebContents(JNIEnv* env) {
  return web_contents_->GetJavaWebContents();
}

void TabImpl::SetJavaImpl(JNIEnv* env, const JavaParamRef<jobject>& impl) {
  // This should only be called early on and only once.
  DCHECK(!java_impl_);
  java_impl_ = impl;
}

void TabImpl::ExecuteScript(JNIEnv* env,
                            const JavaParamRef<jstring>& script,
                            bool use_separate_isolate,
                            const JavaParamRef<jobject>& callback) {
  ScopedJavaGlobalRef<jobject> jcallback(env, callback);
  ExecuteScript(base::android::ConvertJavaStringToUTF16(script),
                use_separate_isolate,
                base::BindOnce(&HandleJavaScriptResult, jcallback));
}

void TabImpl::InitializeAutofillIfNecessary(JNIEnv* env) {
  if (g_system_autofill_disabled_for_testing)
    return;
  if (!autofill::ContentAutofillDriverFactory::FromWebContents(
          web_contents_.get())) {
    InitializeAutofillDriver();
  }
}

ScopedJavaLocalRef<jstring> TabImpl::GetGuid(JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(AttachCurrentThread(),
                                                GetGuid());
}

TabImpl::ScreenShotErrors TabImpl::PrepareForCaptureScreenShot(
    float scale,
    content::RenderWidgetHostView** rwhv,
    gfx::Rect* src_rect,
    gfx::Size* output_size) {
  if (scale <= 0.f || scale > 1.f)
    return ScreenShotErrors::kScaleOutOfRange;

  if (!IsActive())
    return ScreenShotErrors::kTabNotActive;

  if (web_contents_->GetVisibility() != content::Visibility::VISIBLE)
    return ScreenShotErrors::kWebContentsNotVisible;

  if (!browser_ || !browser_->CompositorHasSurface())
    return ScreenShotErrors::kNoSurface;

  *rwhv = web_contents_->GetTopLevelRenderWidgetHostView();
  if (!*rwhv)
    return ScreenShotErrors::kNoRenderWidgetHostView;

  if (!(*rwhv)->GetNativeView()->GetWindowAndroid())
    return ScreenShotErrors::kNoWindowAndroid;

  *src_rect =
      gfx::Rect(web_contents_->GetNativeView()->GetPhysicalBackingSize());
  if (src_rect->IsEmpty())
    return ScreenShotErrors::kEmptyViewport;

  *output_size = gfx::ScaleToCeiledSize(src_rect->size(), scale, scale);
  if (output_size->IsEmpty())
    return ScreenShotErrors::kScaledToEmpty;
  return ScreenShotErrors::kNone;
}

void TabImpl::CaptureScreenShot(
    JNIEnv* env,
    jfloat scale,
    const base::android::JavaParamRef<jobject>& value_callback) {
  content::RenderWidgetHostView* rwhv = nullptr;
  gfx::Rect src_rect;
  gfx::Size output_size;
  ScreenShotErrors error =
      PrepareForCaptureScreenShot(scale, &rwhv, &src_rect, &output_size);
  if (error != ScreenShotErrors::kNone) {
    Java_TabImpl_runCaptureScreenShotCallback(
        env, ScopedJavaLocalRef<jobject>(value_callback),
        ScopedJavaLocalRef<jobject>(), static_cast<int>(error));
    return;
  }

  rwhv->CopyFromSurface(
      src_rect, output_size,
      base::BindOnce(&OnScreenShotCaptured,
                     ScopedJavaGlobalRef<jobject>(value_callback)));
}

jboolean TabImpl::SetData(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& data) {
  std::vector<std::string> flattened_map;
  base::android::AppendJavaStringArrayToStringVector(env, data, &flattened_map);
  std::map<std::string, std::string> data_map;
  for (size_t i = 0; i < flattened_map.size(); i += 2) {
    data_map.insert({flattened_map[i], flattened_map[i + 1]});
  }
  return SetDataInternal(data_map);
}

base::android::ScopedJavaLocalRef<jobjectArray> TabImpl::GetData(JNIEnv* env) {
  std::vector<std::string> flattened_map;
  for (const auto& kv : data_) {
    flattened_map.push_back(kv.first);
    flattened_map.push_back(kv.second);
  }
  return base::android::ToJavaArrayOfStrings(env, flattened_map);
}

jboolean TabImpl::CanTranslate(JNIEnv* env) {
  return TranslateClientImpl::FromWebContents(web_contents())
      ->GetTranslateManager()
      ->CanManuallyTranslate();
}

void TabImpl::ShowTranslateUi(JNIEnv* env) {
  TranslateClientImpl::FromWebContents(web_contents())
      ->ShowTranslateUiWhenReady();
}

void TabImpl::RemoveTabFromBrowserBeforeDestroying(JNIEnv* env) {
  DCHECK(browser_);
  browser_->RemoveTabBeforeDestroyingFromJava(this);
}

void TabImpl::SetTranslateTargetLanguage(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& translate_target_lang) {
  SetTranslateTargetLanguage(
      base::android::ConvertJavaStringToUTF8(env, translate_target_lang));
}

void TabImpl::SetDesktopUserAgentEnabled(JNIEnv* env, jboolean enable) {
  if (desktop_user_agent_enabled_ == enable)
    return;

  desktop_user_agent_enabled_ = enable;

  // Reset state that an earlier call to Navigation::SetUserAgentString()
  // could have modified.
  embedder_support::SetDesktopUserAgentOverride(
      web_contents_.get(), embedder_support::GetUserAgentMetadata(),
      /* override_in_new_tabs= */ false);
  web_contents_->SetRendererInitiatedUserAgentOverrideOption(
      content::NavigationController::UA_OVERRIDE_INHERIT);

  content::NavigationEntry* entry =
      web_contents_->GetController().GetLastCommittedEntry();
  if (!entry)
    return;

  entry->SetIsOverridingUserAgent(enable);
  web_contents_->NotifyPreferencesChanged();
  web_contents_->GetController().Reload(
      content::ReloadType::ORIGINAL_REQUEST_URL, true);
}

jboolean TabImpl::IsDesktopUserAgentEnabled(JNIEnv* env) {
  auto* entry = web_contents_->GetController().GetLastCommittedEntry();
  if (!entry)
    return false;

  // The same user agent override mechanism is used for per-navigation user
  // agent and desktop mode. Make sure not to return desktop mode for
  // navigation entries which used a per-navigation user agent.
  auto* entry_data = NavigationEntryData::Get(entry);
  if (entry_data && entry_data->per_navigation_user_agent_override())
    return false;

  return entry->GetIsOverridingUserAgent();
}

void TabImpl::Download(JNIEnv* env, jlong native_context_menu_params) {
  auto* context_menu_params =
      reinterpret_cast<content::ContextMenuParams*>(native_context_menu_params);

  bool is_link = context_menu_params->media_type !=
                     blink::mojom::ContextMenuDataMediaType::kImage &&
                 context_menu_params->media_type !=
                     blink::mojom::ContextMenuDataMediaType::kVideo;

  download::CreateContextMenuDownload(web_contents_.get(), *context_menu_params,
                                      std::string(), is_link);
}
#endif  // BUILDFLAG(IS_ANDROID)

content::WebContents* TabImpl::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  if (blocked_content::ConsiderForPopupBlocking(params.disposition)) {
    bool blocked = blocked_content::MaybeBlockPopup(
                       source, nullptr,
                       std::make_unique<PopupNavigationDelegateImpl>(
                           params, source, nullptr),
                       &params, blink::mojom::WindowFeatures(),
                       HostContentSettingsMapFactory::GetForBrowserContext(
                           source->GetBrowserContext())) == nullptr;
    if (blocked)
      return nullptr;
  }

  if (params.disposition == WindowOpenDisposition::CURRENT_TAB) {
    source->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(params));
    return source;
  }

  // All URLs not opening in the current tab will get a new tab.
  std::unique_ptr<content::WebContents> new_tab_contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          web_contents()->GetBrowserContext()));
  base::WeakPtr<content::WebContents> new_tab_contents_weak_ptr(
      new_tab_contents->GetWeakPtr());
  bool was_blocked = false;
  AddNewContents(web_contents(), std::move(new_tab_contents), params.url,
                 params.disposition, {}, params.user_gesture, &was_blocked);
  if (was_blocked || !new_tab_contents_weak_ptr)
    return nullptr;
  new_tab_contents_weak_ptr->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(params));
  return new_tab_contents_weak_ptr.get();
}

void TabImpl::ShowRepostFormWarningDialog(content::WebContents* source) {
#if BUILDFLAG(IS_ANDROID)
  Java_TabImpl_showRepostFormWarningDialog(base::android::AttachCurrentThread(),
                                           java_impl_);
#else
  source->GetController().CancelPendingReload();
#endif
}

void TabImpl::NavigationStateChanged(content::WebContents* source,
                                     content::InvalidateTypes changed_flags) {
  DCHECK_EQ(web_contents_.get(), source);
  if (changed_flags & content::INVALIDATE_TYPE_URL) {
    for (auto& observer : observers_)
      observer.DisplayedUrlChanged(source->GetVisibleURL());
    UpdateBrowserVisibleSecurityStateIfNecessary();
  }

  // TODO(crbug.com/1064582): INVALIDATE_TYPE_TITLE is called only when a title
  // is set on the active navigation entry, but not when the active entry
  // changes, so check INVALIDATE_TYPE_LOAD here as well. However this should
  // be fixed and INVALIDATE_TYPE_LOAD should be removed.
  if (changed_flags &
      (content::INVALIDATE_TYPE_TITLE | content::INVALIDATE_TYPE_LOAD)) {
    std::u16string title = web_contents_->GetTitle();
    if (title_ != title) {
      title_ = title;
      for (auto& observer : observers_)
        observer.OnTitleUpdated(title);
    }
  }
}

content::JavaScriptDialogManager* TabImpl::GetJavaScriptDialogManager(
    content::WebContents* web_contents) {
#if BUILDFLAG(IS_ANDROID)
  return javascript_dialogs::TabModalDialogManager::FromWebContents(
      web_contents);
#else
  return nullptr;
#endif
}

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<content::ColorChooser> TabImpl::OpenColorChooser(
    content::WebContents* web_contents,
    SkColor color,
    const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions) {
  return std::make_unique<web_contents_delegate_android::ColorChooserAndroid>(
      web_contents, color, suggestions);
}
#endif

void TabImpl::CreateSmsPrompt(content::RenderFrameHost* render_frame_host,
                              const std::vector<url::Origin>& origin_list,
                              const std::string& one_time_code,
                              base::OnceClosure on_confirm,
                              base::OnceClosure on_cancel) {
#if BUILDFLAG(IS_ANDROID)
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  sms::SmsInfoBar::Create(
      web_contents,
      infobars::ContentInfoBarManager::FromWebContents(web_contents),
      origin_list, one_time_code, std::move(on_confirm), std::move(on_cancel));
#else
  NOTREACHED();
#endif
}

void TabImpl::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
}

bool TabImpl::IsBackForwardCacheSupported() {
  return true;
}

void TabImpl::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  MediaStreamManager::FromWebContents(web_contents)
      ->RequestMediaAccessPermission(request, std::move(callback));
#else
  std::move(callback).Run(blink::mojom::StreamDevicesSet(),
                          blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED,
                          nullptr);
#endif
}

bool TabImpl::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  DCHECK(type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE ||
         type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  blink::PermissionType permission_type =
      type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE
          ? blink::PermissionType::AUDIO_CAPTURE
          : blink::PermissionType::VIDEO_CAPTURE;

  // TODO(crbug.com/1321100): Remove `security_origin`.
  if (render_frame_host->GetLastCommittedOrigin().GetURL() != security_origin) {
    return false;
  }
  // It is OK to ignore `security_origin` because it will be calculated from
  // `render_frame_host` and we always ignore `requesting_origin` for
  // `AUDIO_CAPTURE` and `VIDEO_CAPTURE`.
  // `render_frame_host->GetMainFrame()->GetLastCommittedOrigin()` will be used
  // instead.
  return render_frame_host->GetBrowserContext()
             ->GetPermissionController()
             ->GetPermissionStatusForCurrentDocument(permission_type,
                                                     render_frame_host) ==
         blink::mojom::PermissionStatus::GRANTED;
}

void TabImpl::EnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  // TODO(crbug.com/1232147): support |options|.
  if (is_fullscreen_) {
    // Typically EnterFullscreenModeForTab() should not be called consecutively,
    // but there may be corner cases with oopif that lead to multiple
    // consecutive calls. Avoid notifying the delegate in this case.
    return;
  }
  is_fullscreen_ = true;
  if (!IsActive()) {
    // Process the request the tab is made active.
    enter_fullscreen_on_gained_active_ = true;
    return;
  }
  EnterFullscreenImpl();
}

void TabImpl::ExitFullscreenModeForTab(content::WebContents* web_contents) {
  weak_ptr_factory_for_fullscreen_exit_.InvalidateWeakPtrs();
  is_fullscreen_ = false;
  if (enter_fullscreen_on_gained_active_)
    enter_fullscreen_on_gained_active_ = false;
  else
    fullscreen_delegate_->ExitFullscreen();
}

bool TabImpl::IsFullscreenForTabOrPending(
    const content::WebContents* web_contents) {
  return is_fullscreen_;
}

blink::mojom::DisplayMode TabImpl::GetDisplayMode(
    const content::WebContents* web_contents) {
  return is_fullscreen_ ? blink::mojom::DisplayMode::kFullscreen
                        : blink::mojom::DisplayMode::kBrowser;
}

void TabImpl::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  if (!new_tab_delegate_) {
    *was_blocked = true;
    return;
  }

  // At this point the |new_contents| is beyond the popup blocker, but we use
  // the same logic for determining if the popup tracker needs to be attached.
  if (source && blocked_content::ConsiderForPopupBlocking(disposition)) {
    blocked_content::PopupTracker::CreateForWebContents(new_contents.get(),
                                                        source, disposition);
  }

  new_tab_delegate_->OnNewTab(browser_->CreateTab(std::move(new_contents)),
                              NewTabTypeFromWindowDisposition(disposition));
}

void TabImpl::CloseContents(content::WebContents* source) {
  // The only time that |browser_| is null is during shutdown, and this callback
  // shouldn't come in at that time.
  DCHECK(browser_);

#if BUILDFLAG(IS_ANDROID)
  JNIEnv* env = AttachCurrentThread();
  Java_TabImpl_handleCloseFromWebContents(env, java_impl_);
  // The above call resulted in the destruction of this; nothing to do but
  // return.
#else
  browser_->DestroyTab(this);
#endif
}

void TabImpl::FindReply(content::WebContents* web_contents,
                        int request_id,
                        int number_of_matches,
                        const gfx::Rect& selection_rect,
                        int active_match_ordinal,
                        bool final_update) {
  GetFindTabHelper()->HandleFindReply(request_id, number_of_matches,
                                      selection_rect, active_match_ordinal,
                                      final_update);
}

#if BUILDFLAG(IS_ANDROID)
// FindMatchRectsReply and OnFindResultAvailable forward find-related results to
// the Java TabImpl. The find actions themselves are initiated directly from
// Java via FindInPageBridge.
void TabImpl::FindMatchRectsReply(content::WebContents* web_contents,
                                  int version,
                                  const std::vector<gfx::RectF>& rects,
                                  const gfx::RectF& active_rect) {
  JNIEnv* env = AttachCurrentThread();
  // Create the details object.
  ScopedJavaLocalRef<jobject> details_object =
      Java_TabImpl_createFindMatchRectsDetails(
          env, version, rects.size(),
          ScopedJavaLocalRef<jobject>(Java_TabImpl_createRectF(
              env, active_rect.x(), active_rect.y(), active_rect.right(),
              active_rect.bottom())));

  // Add the rects.
  for (size_t i = 0; i < rects.size(); ++i) {
    const gfx::RectF& rect = rects[i];
    Java_TabImpl_setMatchRectByIndex(
        env, details_object, i,
        ScopedJavaLocalRef<jobject>(Java_TabImpl_createRectF(
            env, rect.x(), rect.y(), rect.right(), rect.bottom())));
  }

  Java_TabImpl_onFindMatchRectsAvailable(env, java_impl_, details_object);
}
#endif

void TabImpl::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
#if BUILDFLAG(IS_ANDROID)
  // If a renderer process is lost when the tab is not visible, indicate to the
  // WebContents that it should automatically reload the next time it becomes
  // visible.
  JNIEnv* env = AttachCurrentThread();
  if (Java_TabImpl_willAutomaticallyReloadAfterCrashImpl(env, java_impl_))
    web_contents()->GetController().SetNeedsReload();
#endif

  for (auto& observer : observers_)
    observer.OnRenderProcessGone();
}

void TabImpl::OnFindResultAvailable(content::WebContents* web_contents) {
#if BUILDFLAG(IS_ANDROID)
  const find_in_page::FindNotificationDetails& find_result =
      GetFindTabHelper()->find_result();
  JNIEnv* env = AttachCurrentThread();
  Java_TabImpl_onFindResultAvailable(
      env, java_impl_, find_result.number_of_matches(),
      find_result.active_match_ordinal(), find_result.final_update());
#endif
}

void TabImpl::DidChangeVisibleSecurityState() {
  UpdateBrowserVisibleSecurityStateIfNecessary();
}

void TabImpl::UpdateBrowserVisibleSecurityStateIfNecessary() {
  if (browser_ && browser_->GetActiveTab() == this)
    browser_->VisibleSecurityStateOfActiveTabChanged();
}

void TabImpl::OnExitFullscreen() {
  // If |processing_enter_fullscreen_| is true, it means the callback is being
  // called while processing EnterFullscreenModeForTab(). WebContents doesn't
  // deal well with this. FATAL as Android generally doesn't run with DCHECKs.
  LOG_IF(FATAL, processing_enter_fullscreen_)
      << "exiting fullscreen while entering fullscreen is not supported";
  web_contents_->ExitFullscreen(/* will_cause_resize */ false);
}

void TabImpl::UpdateRendererPrefs(bool should_sync_prefs) {
  blink::RendererPreferences* prefs = web_contents_->GetMutableRendererPrefs();
  content::UpdateFontRendererPreferencesFromSystemSettings(prefs);
  prefs->accept_languages = i18n::GetAcceptLangs();
  if (should_sync_prefs)
    web_contents_->SyncRendererPrefs();
}

#if BUILDFLAG(IS_ANDROID)

void TabImpl::InitializeAutofillForTests() {
  InitializeAutofillDriver();
}

void TabImpl::InitializeAutofillDriver() {
  content::WebContents* web_contents = web_contents_.get();
  DCHECK(
      !autofill::ContentAutofillDriverFactory::FromWebContents(web_contents));
  DCHECK(autofill::AutofillProvider::FromWebContents(web_contents));

  AutofillClientImpl::CreateForWebContents(web_contents);
}

#endif  // BUILDFLAG(IS_ANDROID)

find_in_page::FindTabHelper* TabImpl::GetFindTabHelper() {
  return find_in_page::FindTabHelper::FromWebContents(web_contents_.get());
}

// static
sessions::SessionTabHelperDelegate* TabImpl::GetSessionServiceTabHelperDelegate(
    content::WebContents* web_contents) {
  TabImpl* tab = FromWebContents(web_contents);
  return (tab && tab->browser_) ? tab->browser_->browser_persister() : nullptr;
}

bool TabImpl::SetDataInternal(const std::map<std::string, std::string>& data) {
  int total_size = 0;
  for (const auto& kv : data)
    total_size += kv.first.size() + kv.second.size();
  if (total_size > kMaxDataSize)
    return false;
  data_ = data;
  for (auto& observer : data_observers_)
    observer.OnDataChanged(this, data_);
  return true;
}

void TabImpl::EnterFullscreenImpl() {
  // This ensures the existing callback is ignored.
  weak_ptr_factory_for_fullscreen_exit_.InvalidateWeakPtrs();

  auto exit_fullscreen_closure =
      base::BindOnce(&TabImpl::OnExitFullscreen,
                     weak_ptr_factory_for_fullscreen_exit_.GetWeakPtr());
  base::AutoReset<bool> reset(&processing_enter_fullscreen_, true);
  fullscreen_delegate_->EnterFullscreen(std::move(exit_fullscreen_closure));
}

}  // namespace weblayer
