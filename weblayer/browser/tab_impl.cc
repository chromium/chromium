// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/tab_impl.h"

#include <cmath>

#include "base/auto_reset.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/thread_pool.h"
#include "base/time/default_tick_clock.h"
#include "cc/layers/layer.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_provider.h"
#include "components/blocked_content/popup_blocker.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/blocked_content/popup_opener_tab_helper.h"
#include "components/blocked_content/popup_tracker.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/find_in_page/find_types.h"
#include "components/js_injection/browser/js_communication_host.h"
#include "components/js_injection/browser/web_message_host.h"
#include "components/js_injection/browser/web_message_host_factory.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_result.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/webrtc/media_stream_devices_controller.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/renderer_preferences_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
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
#include "weblayer/browser/infobar_service.h"
#include "weblayer/browser/js_communication/web_message_host_factory_wrapper.h"
#include "weblayer/browser/navigation_controller_impl.h"
#include "weblayer/browser/no_state_prefetch/prerender_tab_helper.h"
#include "weblayer/browser/page_load_metrics_initialize.h"
#include "weblayer/browser/page_specific_content_settings_delegate.h"
#include "weblayer/browser/password_manager_driver_factory.h"
#include "weblayer/browser/permissions/permission_manager_factory.h"
#include "weblayer/browser/persistence/browser_persister.h"
#include "weblayer/browser/popup_navigation_delegate_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/translate_client_impl.h"
#include "weblayer/browser/weblayer_features.h"
#include "weblayer/common/isolated_world_ids.h"
#include "weblayer/public/fullscreen_delegate.h"
#include "weblayer/public/js_communication/web_message.h"
#include "weblayer/public/js_communication/web_message_host_factory.h"
#include "weblayer/public/new_tab_delegate.h"
#include "weblayer/public/tab_observer.h"

#if !defined(OS_ANDROID)
#include "ui/views/controls/webview/webview.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/json/json_writer.h"
#include "base/trace_event/trace_event.h"
#include "components/autofill/android/provider/autofill_provider_android.h"
#include "components/browser_ui/sms/android/sms_infobar.h"
#include "components/embedder_support/android/contextmenu/context_menu_builder.h"
#include "components/embedder_support/android/delegate/color_chooser_android.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"  // nogncheck
#include "components/translate/core/browser/translate_manager.h"
#include "ui/android/view_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "weblayer/browser/browser_controls_container_view.h"
#include "weblayer/browser/browser_controls_navigation_state_handler.h"
#include "weblayer/browser/controls_visibility_reason.h"
#include "weblayer/browser/java/jni/TabImpl_jni.h"
#include "weblayer/browser/javascript_tab_modal_dialog_manager_delegate_android.h"
#include "weblayer/browser/js_communication/web_message_host_factory_proxy.h"
#include "weblayer/browser/translate_client_impl.h"
#include "weblayer/browser/weblayer_factory_impl_android.h"
#include "weblayer/browser/webrtc/media_stream_manager.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#include "weblayer/browser/captive_portal_service_factory.h"
#endif

#if defined(OS_ANDROID)
using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
#endif

namespace weblayer {

namespace {

// Maximum size of data when calling SetData().
constexpr int kMaxDataSize = 4096;

#if defined(OS_ANDROID)
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
  TabImpl* tab = nullptr;
};

#if defined(OS_ANDROID)
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
      gfx::ConvertToJavaBitmap(&bitmap, gfx::OomBehavior::kReturnNullOnOom));
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

#endif  // OS_ANDROID

std::set<TabImpl*>& GetTabs() {
  static base::NoDestructor<std::set<TabImpl*>> s_all_tab_impl;
  return *s_all_tab_impl;
}

// Simulates a WeakPtr for WebContents. Specifically if the WebContents
// supplied to the constructor is destroyed then web_contents() returns
// null.
class WebContentsTracker : public content::WebContentsObserver {
 public:
  explicit WebContentsTracker(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
};

}  // namespace

#if defined(OS_ANDROID)

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

  // By default renderer initiated navigations inherit the user-agent override
  // of the current NavigationEntry. For WebLayer, the user-agent override is
  // set on a per NavigationEntry entry basis.
  web_contents_->SetRendererInitiatedUserAgentOverrideOption(
      content::NavigationController::UA_OVERRIDE_FALSE);

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

#if defined(OS_ANDROID)
  InfoBarService::CreateForWebContents(web_contents_.get());
#endif

  find_in_page::FindTabHelper::CreateForWebContents(web_contents_.get());
  GetFindTabHelper()->AddObserver(this);

  TranslateClientImpl::CreateForWebContents(web_contents_.get());

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

#if defined(OS_ANDROID)
  InfoBarService::CreateForWebContents(web_contents_.get());
  javascript_dialogs::TabModalDialogManager::CreateForWebContents(
      web_contents_.get(),
      std::make_unique<JavaScriptTabModalDialogManagerDelegateAndroid>(
          web_contents_.get()));

  browser_controls_navigation_state_handler_ =
      std::make_unique<BrowserControlsNavigationStateHandler>(
          web_contents_.get(), this);
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
}

TabImpl::~TabImpl() {
  DCHECK(!browser_);

  GetFindTabHelper()->RemoveObserver(this);

  // Delete the WebContents and related objects that may be observing
  // the WebContents now to avoid calling back when this object is partially
  // deleted. DidFinishNavigation() may be called while deleting WebContents,
  // so stop observing first. Similarly WebContents destructor can callback to
  // delegate such as NavigationStateChanged, so clear its Delegate as well.
#if defined(OS_ANDROID)
  browser_controls_navigation_state_handler_.reset();
#endif
  Observe(nullptr);
  web_contents_->SetDelegate(nullptr);
  if (navigation_controller_->should_delay_web_contents_deletion()) {
    // Some user-data on WebContents directly or indirectly references this.
    // Remove that linkage to avoid use-after-free.
    web_contents_->RemoveUserData(&kWebContentsUserDataKey);
    web_contents_->RemoveUserData(
        autofill::ContentAutofillDriverFactory::
            kContentAutofillDriverFactoryWebContentsUserDataKey);
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
  return user_data ? user_data->tab : nullptr;
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

void TabImpl::ExecuteScript(const base::string16& script,
                            bool use_separate_isolate,
                            JavaScriptResultCallback callback) {
  if (use_separate_isolate) {
    web_contents_->GetMainFrame()->ExecuteJavaScriptInIsolatedWorld(
        script, std::move(callback), ISOLATED_WORLD_ID_WEBLAYER);
  } else {
    content::RenderFrameHost::AllowInjectingJavaScript();
    web_contents_->GetMainFrame()->ExecuteJavaScript(script,
                                                     std::move(callback));
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

base::string16 TabImpl::AddWebMessageHostFactory(
    std::unique_ptr<WebMessageHostFactory> factory,
    const base::string16& js_object_name,
    const std::vector<std::string>& allowed_origin_rules) {
  if (!js_communication_host_) {
    js_communication_host_ =
        std::make_unique<js_injection::JsCommunicationHost>(
            web_contents_.get());
  }
  return js_communication_host_->AddWebMessageHostFactory(
      std::make_unique<WebMessageHostFactoryWrapper>(std::move(factory)),
      js_object_name, allowed_origin_rules);
}

void TabImpl::RemoveWebMessageHostFactory(
    const base::string16& js_object_name) {
  if (js_communication_host_)
    js_communication_host_->RemoveWebMessageHostFactory(js_object_name);
}

void TabImpl::ExecuteScriptWithUserGestureForTests(
    const base::string16& script) {
  web_contents_->GetMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      script);
}

std::unique_ptr<FaviconFetcher> TabImpl::CreateFaviconFetcher(
    FaviconFetcherDelegate* delegate) {
  return std::make_unique<FaviconFetcherImpl>(web_contents_.get(), delegate);
}

#if !defined(OS_ANDROID)
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

void TabImpl::OnLosingActive() {
  if (is_fullscreen_)
    web_contents_->ExitFullscreen(/* will_cause_resize */ false);
}

bool TabImpl::IsActive() {
  return browser_->GetActiveTab() == this;
}

void TabImpl::ShowContextMenu(const content::ContextMenuParams& params) {
#if defined(OS_ANDROID)
  Java_TabImpl_showContextMenu(
      base::android::AttachCurrentThread(), java_impl_,
      context_menu::BuildJavaContextMenuParams(params));
#endif
}

#if defined(OS_ANDROID)
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

void TabImpl::SetBrowserControlsContainerViews(
    JNIEnv* env,
    jlong native_top_controls_container_view,
    jlong native_bottom_controls_container_view) {
  top_controls_container_view_ =
      reinterpret_cast<BrowserControlsContainerView*>(
          native_top_controls_container_view);
  bottom_controls_container_view_ =
      reinterpret_cast<BrowserControlsContainerView*>(
          native_bottom_controls_container_view);
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

void TabImpl::SetJavaImpl(JNIEnv* env, const JavaParamRef<jobject>& impl) {
  // This should only be called early on and only once.
  DCHECK(!java_impl_);
  java_impl_ = impl;
}

void TabImpl::OnAutofillProviderChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& autofill_provider) {
  if (g_system_autofill_disabled_for_testing)
    return;

  if (!autofill_provider_) {
    // The first invocation should be when instantiating the autofill
    // infrastructure, at which point the Java-side object should not be null.
    DCHECK(autofill_provider);

    // Initialize the native side of the autofill infrastructure.
    autofill_provider_ = std::make_unique<autofill::AutofillProviderAndroid>(
        autofill_provider, web_contents_.get());
    InitializeAutofill();
    return;
  }

  // The AutofillProvider Java object has been changed; inform
  // |autofill_provider_|.
  auto* provider =
      static_cast<autofill::AutofillProviderAndroid*>(autofill_provider_.get());
  provider->OnJavaAutofillProviderChanged(env, autofill_provider);
}

void TabImpl::UpdateBrowserControlsConstraint(JNIEnv* env,
                                              jint constraint,
                                              jboolean animate) {
  current_browser_controls_visibility_constraint_ =
      static_cast<content::BrowserControlsState>(constraint);
  // Passing BOTH here means that it doesn't matter what state the controls are
  // currently in; don't change the current state unless it's incompatible with
  // the new constraint.
  UpdateBrowserControlsState(content::BROWSER_CONTROLS_STATE_BOTH, animate);
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

  const int reduced_height =
      src_rect->height() -
      top_controls_container_view_->GetContentHeightDelta() -
      bottom_controls_container_view_->GetContentHeightDelta();
  if (reduced_height <= 0)
    return ScreenShotErrors::kHiddenByControls;
  src_rect->set_height(reduced_height);

  *output_size = gfx::ScaleToCeiledSize(src_rect->size(), scale, scale);
  if (output_size->IsEmpty())
    return ScreenShotErrors::kScaledToEmpty;
  return ScreenShotErrors::kNone;
}

void TabImpl::UpdateBrowserControlsState(
    content::BrowserControlsState new_state,
    bool animate) {
  if (base::FeatureList::IsEnabled(kImmediatelyHideBrowserControlsForTest))
    animate = false;
  // The constraint is managed by Java code, so re-use the existing constraint
  // and only update the desired state.
  web_contents_->GetMainFrame()->UpdateBrowserControlsState(
      current_browser_controls_visibility_constraint_, new_state, animate);
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

jboolean TabImpl::IsRendererControllingBrowserControlsOffsets(JNIEnv* env) {
  return browser_controls_navigation_state_handler_
      ->IsRendererControllingOffsets();
}

base::android::ScopedJavaLocalRef<jstring> TabImpl::RegisterWebMessageCallback(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& js_object_name,
    const base::android::JavaParamRef<jobjectArray>& js_origins,
    const base::android::JavaParamRef<jobject>& client) {
  auto proxy = std::make_unique<WebMessageHostFactoryProxy>(client);
  std::vector<std::string> origins;
  base::android::AppendJavaStringArrayToStringVector(env, js_origins, &origins);
  base::string16 result = AddWebMessageHostFactory(
      std::move(proxy),
      base::android::ConvertJavaStringToUTF16(env, js_object_name), origins);
  return base::android::ConvertUTF16ToJavaString(env, result);
}

void TabImpl::UnregisterWebMessageCallback(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& js_object_name) {
  base::string16 name;
  base::android::ConvertJavaStringToUTF16(env, js_object_name, &name);
  RemoveWebMessageHostFactory(name);
}

jboolean TabImpl::CanTranslate(JNIEnv* env) {
  return TranslateClientImpl::FromWebContents(web_contents())
      ->GetTranslateManager()
      ->CanManuallyTranslate();
}

void TabImpl::ShowTranslateUi(JNIEnv* env) {
  TranslateClientImpl::FromWebContents(web_contents())
      ->ManualTranslateWhenReady();
}

void TabImpl::RemoveTabFromBrowserBeforeDestroying(JNIEnv* env) {
  DCHECK(browser_);
  browser_->RemoveTabBeforeDestroyingFromJava(this);
}

void TabImpl::SetTranslateTargetLanguage(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& translate_target_lang) {
  translate::TranslateManager* translate_manager =
      TranslateClientImpl::FromWebContents(web_contents())
          ->GetTranslateManager();
  translate_manager->SetPredefinedTargetLanguage(
      base::android::ConvertJavaStringToUTF8(env, translate_target_lang));
}
#endif  // OS_ANDROID

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
  WebContentsTracker tracker(new_tab_contents.get());
  bool was_blocked = false;
  AddNewContents(web_contents(), std::move(new_tab_contents), params.url,
                 params.disposition, {}, params.user_gesture, &was_blocked);
  if (was_blocked || !tracker.web_contents())
    return nullptr;
  tracker.web_contents()->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(params));
  return tracker.web_contents();
}

void TabImpl::ShowRepostFormWarningDialog(content::WebContents* source) {
#if defined(OS_ANDROID)
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
    base::string16 title = web_contents_->GetTitle();
    if (title_ != title) {
      title_ = title;
      for (auto& observer : observers_)
        observer.OnTitleUpdated(title);
    }
  }
}

content::JavaScriptDialogManager* TabImpl::GetJavaScriptDialogManager(
    content::WebContents* web_contents) {
#if defined(OS_ANDROID)
  return javascript_dialogs::TabModalDialogManager::FromWebContents(
      web_contents);
#else
  return nullptr;
#endif
}

content::ColorChooser* TabImpl::OpenColorChooser(
    content::WebContents* web_contents,
    SkColor color,
    const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions) {
#if defined(OS_ANDROID)
  return new web_contents_delegate_android::ColorChooserAndroid(
      web_contents, color, suggestions);
#else
  return nullptr;
#endif
}

void TabImpl::CreateSmsPrompt(content::RenderFrameHost* render_frame_host,
                              const url::Origin& origin,
                              const std::string& one_time_code,
                              base::OnceClosure on_confirm,
                              base::OnceClosure on_cancel) {
#if defined(OS_ANDROID)
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  sms::SmsInfoBar::Create(
      web_contents, InfoBarService::FromWebContents(web_contents),
      InfoBarService::GetResourceIdMapper(), origin, one_time_code,
      std::move(on_confirm), std::move(on_cancel));
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

int TabImpl::GetTopControlsHeight() {
#if defined(OS_ANDROID)
  return top_controls_container_view_
             ? top_controls_container_view_->GetControlsHeight()
             : 0;
#else
  return 0;
#endif
}

int TabImpl::GetTopControlsMinHeight() {
#if defined(OS_ANDROID)
  return top_controls_container_view_
             ? top_controls_container_view_->GetMinHeight()
             : 0;
#else
  return 0;
#endif
}

int TabImpl::GetBottomControlsHeight() {
#if defined(OS_ANDROID)
  return bottom_controls_container_view_
             ? bottom_controls_container_view_->GetControlsHeight()
             : 0;
#else
  return 0;
#endif
}

bool TabImpl::DoBrowserControlsShrinkRendererSize(
    content::WebContents* web_contents) {
#if defined(OS_ANDROID)
  TRACE_EVENT0("weblayer", "Java_TabImpl_doBrowserControlsShrinkRendererSize");
  return Java_TabImpl_doBrowserControlsShrinkRendererSize(AttachCurrentThread(),
                                                          java_impl_);
#else
  return false;
#endif
}

bool TabImpl::ShouldAnimateBrowserControlsHeightChanges() {
#if defined(OS_ANDROID)
  return top_controls_container_view_
             ? top_controls_container_view_
                   ->ShouldAnimateBrowserControlsHeightChanges()
             : false;
#else
  return false;
#endif
}

bool TabImpl::OnlyExpandTopControlsAtPageTop() {
#if defined(OS_ANDROID)
  return top_controls_container_view_
             ? top_controls_container_view_->OnlyExpandControlsAtPageTop()
             : false;
#else
  return false;
#endif
}

bool TabImpl::EmbedsFullscreenWidget() {
  return true;
}

void TabImpl::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
#if defined(OS_ANDROID)
  MediaStreamManager::FromWebContents(web_contents)
      ->RequestMediaAccessPermission(request, std::move(callback));
#else
  std::move(callback).Run(
      {}, blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED, nullptr);
#endif
}

bool TabImpl::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  DCHECK(type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE ||
         type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  ContentSettingsType content_settings_type =
      type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE
          ? ContentSettingsType::MEDIASTREAM_MIC
          : ContentSettingsType::MEDIASTREAM_CAMERA;
  return PermissionManagerFactory::GetForBrowserContext(
             content::WebContents::FromRenderFrameHost(render_frame_host)
                 ->GetBrowserContext())
             ->GetPermissionStatusForFrame(content_settings_type,
                                           render_frame_host, security_origin)
             .content_setting == CONTENT_SETTING_ALLOW;
}

void TabImpl::EnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  // TODO: support |options|.
  is_fullscreen_ = true;
  auto exit_fullscreen_closure = base::BindOnce(&TabImpl::OnExitFullscreen,
                                                weak_ptr_factory_.GetWeakPtr());
  base::AutoReset<bool> reset(&processing_enter_fullscreen_, true);
  fullscreen_delegate_->EnterFullscreen(std::move(exit_fullscreen_closure));
#if defined(OS_ANDROID)
  // Make sure browser controls cannot show when the tab is fullscreen.
  SetBrowserControlsConstraint(ControlsVisibilityReason::kFullscreen,
                               content::BROWSER_CONTROLS_STATE_HIDDEN);
#endif
}

void TabImpl::ExitFullscreenModeForTab(content::WebContents* web_contents) {
  is_fullscreen_ = false;
  fullscreen_delegate_->ExitFullscreen();
#if defined(OS_ANDROID)
  // Attempt to show browser controls when exiting fullscreen.
  SetBrowserControlsConstraint(ControlsVisibilityReason::kFullscreen,
                               content::BROWSER_CONTROLS_STATE_BOTH);
#endif
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

void TabImpl::AddNewContents(content::WebContents* source,
                             std::unique_ptr<content::WebContents> new_contents,
                             const GURL& target_url,
                             WindowOpenDisposition disposition,
                             const gfx::Rect& initial_rect,
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

#if defined(OS_ANDROID)
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

#if defined(OS_ANDROID)
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

void TabImpl::RenderProcessGone(base::TerminationStatus status) {
#if defined(OS_ANDROID)
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
#if defined(OS_ANDROID)
  const find_in_page::FindNotificationDetails& find_result =
      GetFindTabHelper()->find_result();
  JNIEnv* env = AttachCurrentThread();
  Java_TabImpl_onFindResultAvailable(
      env, java_impl_, find_result.number_of_matches(),
      find_result.active_match_ordinal(), find_result.final_update());
#endif
}

#if defined(OS_ANDROID)
void TabImpl::OnBrowserControlsStateStateChanged(
    ControlsVisibilityReason reason,
    content::BrowserControlsState state) {
  SetBrowserControlsConstraint(reason, state);
}

void TabImpl::OnUpdateBrowserControlsStateBecauseOfProcessSwitch(
    bool did_commit) {
  // This matches the logic of updateAfterRendererProcessSwitch() and
  // updateEnabledState() in Chrome's TabBrowserControlsConstraintsHelper.
  if (did_commit &&
      current_browser_controls_visibility_constraint_ ==
          content::BROWSER_CONTROLS_STATE_SHOWN &&
      top_controls_container_view_ &&
      top_controls_container_view_->IsFullyVisible()) {
    // The top-control is fully visible, don't animate this else the controls
    // bounce around.
    UpdateBrowserControlsState(content::BROWSER_CONTROLS_STATE_SHOWN, false);
  } else {
    UpdateBrowserControlsState(
        content::BROWSER_CONTROLS_STATE_BOTH,
        current_browser_controls_visibility_constraint_ !=
            content::BROWSER_CONTROLS_STATE_HIDDEN);
  }
}

#endif

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
  blink::mojom::RendererPreferences* prefs =
      web_contents_->GetMutableRendererPrefs();
  content::UpdateFontRendererPreferencesFromSystemSettings(prefs);
  prefs->accept_languages = i18n::GetAcceptLangs();
  if (should_sync_prefs)
    web_contents_->SyncRendererPrefs();
}

#if defined(OS_ANDROID)
void TabImpl::SetBrowserControlsConstraint(
    ControlsVisibilityReason reason,
    content::BrowserControlsState constraint) {
  Java_TabImpl_setBrowserControlsVisibilityConstraint(
      base::android::AttachCurrentThread(), java_impl_,
      static_cast<int>(reason), constraint);
}
#endif

void TabImpl::InitializeAutofillForTests(
    std::unique_ptr<autofill::AutofillProvider> provider) {
  DCHECK(!autofill_provider_);

  autofill_provider_ = std::move(provider);
  InitializeAutofill();
}

void TabImpl::InitializeAutofill() {
  DCHECK(autofill_provider_);

  content::WebContents* web_contents = web_contents_.get();
  DCHECK(
      !autofill::ContentAutofillDriverFactory::FromWebContents(web_contents));

  AutofillClientImpl::CreateForWebContents(web_contents);
  autofill::ContentAutofillDriverFactory::CreateForWebContentsAndDelegate(
      web_contents, AutofillClientImpl::FromWebContents(web_contents),
      i18n::GetApplicationLocale(),
      autofill::AutofillManager::DISABLE_AUTOFILL_DOWNLOAD_MANAGER,
      autofill_provider_.get());
}

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

}  // namespace weblayer
