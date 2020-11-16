// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_TAB_IMPL_H_
#define WEBLAYER_BROWSER_TAB_IMPL_H_

#include <memory>
#include <set>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/find_in_page/find_result_observer.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/browser_controls_state.h"
#include "weblayer/browser/i18n_util.h"
#include "weblayer/public/tab.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#include "weblayer/browser/browser_controls_navigation_state_handler_delegate.h"
#endif

namespace js_injection {
class JsCommunicationHost;
}

namespace autofill {
class AutofillProvider;
}  // namespace autofill

namespace blink {
namespace web_pref {
struct WebPreferences;
}
}  // namespace blink

namespace content {
class RenderWidgetHostView;
class WebContents;
struct ContextMenuParams;
}

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace sessions {
class SessionTabHelperDelegate;
}

namespace weblayer {
class BrowserControlsNavigationStateHandler;
class BrowserImpl;
class FullscreenDelegate;
class NavigationControllerImpl;
class NewTabDelegate;
class ProfileImpl;

#if defined(OS_ANDROID)
class BrowserControlsContainerView;
enum class ControlsVisibilityReason;
class WebMessageHostFactoryProxy;
#endif

class TabImpl : public Tab,
                public content::WebContentsDelegate,
                public content::WebContentsObserver,
#if defined(OS_ANDROID)
                public BrowserControlsNavigationStateHandlerDelegate,
#endif
                public find_in_page::FindResultObserver {
 public:
  enum class ScreenShotErrors {
    kNone = 0,
    kScaleOutOfRange,
    kTabNotActive,
    kWebContentsNotVisible,
    kNoSurface,
    kNoRenderWidgetHostView,
    kNoWindowAndroid,
    kEmptyViewport,
    kHiddenByControls,
    kScaledToEmpty,
    kCaptureFailed,
    kBitmapAllocationFailed,
  };

  class DataObserver {
   public:
    // Called when SetData() is called on |tab|.
    virtual void OnDataChanged(
        TabImpl* tab,
        const std::map<std::string, std::string>& data) = 0;
  };

  // TODO(sky): investigate a better way to not have so many ifdefs.
#if defined(OS_ANDROID)
  TabImpl(ProfileImpl* profile,
          const base::android::JavaParamRef<jobject>& java_impl,
          std::unique_ptr<content::WebContents> web_contents);
#endif
  explicit TabImpl(ProfileImpl* profile,
                   std::unique_ptr<content::WebContents> web_contents,
                   const std::string& guid = std::string());
  ~TabImpl() override;

  // Returns the TabImpl from the specified WebContents (which may be null), or
  // null if |web_contents| was not created by a TabImpl.
  static TabImpl* FromWebContents(content::WebContents* web_contents);

  static std::set<TabImpl*> GetAllTabImpl();

  ProfileImpl* profile() { return profile_; }

  void set_browser(BrowserImpl* browser) { browser_ = browser; }
  BrowserImpl* browser() { return browser_; }

  content::WebContents* web_contents() const { return web_contents_.get(); }

  bool has_new_tab_delegate() const { return new_tab_delegate_ != nullptr; }
  NewTabDelegate* new_tab_delegate() const { return new_tab_delegate_; }

  // Called from Browser when this Tab is losing active status.
  void OnLosingActive();

  bool IsActive();

  void ShowContextMenu(const content::ContextMenuParams& params);

#if defined(OS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> GetJavaTab() {
    return java_impl_;
  }

  // Call this method to disable integration with the system-level Autofill
  // infrastructure. Useful in conjunction with InitializeAutofillForTests().
  // Should be called early in the lifetime of WebLayer, and in
  // particular, must be called before the TabImpl is attached to the browser
  // on the Java side to have the desired effect.
  static void DisableAutofillSystemIntegrationForTesting();

  base::android::ScopedJavaLocalRef<jobject> GetWebContents(JNIEnv* env);
  void SetBrowserControlsContainerViews(
      JNIEnv* env,
      jlong native_top_browser_controls_container_view,
      jlong native_bottom_browser_controls_container_view);
  void ExecuteScript(JNIEnv* env,
                     const base::android::JavaParamRef<jstring>& script,
                     bool use_separate_isolate,
                     const base::android::JavaParamRef<jobject>& callback);
  void SetJavaImpl(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& impl);

  // Invoked every time that the Java-side AutofillProvider instance is
  // changed (set to null or to a new object). On first invocation with a non-
  // null object initializes the native Autofill infrastructure. On
  // subsequent invocations updates the association of that native
  // infrastructure with its Java counterpart.
  void OnAutofillProviderChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& autofill_provider);
  void UpdateBrowserControlsConstraint(JNIEnv* env,
                                       jint constraint,
                                       jboolean animate);

  base::android::ScopedJavaLocalRef<jstring> GetGuid(JNIEnv* env);
  void CaptureScreenShot(
      JNIEnv* env,
      jfloat scale,
      const base::android::JavaParamRef<jobject>& value_callback);

  jboolean SetData(JNIEnv* env,
                   const base::android::JavaParamRef<jobjectArray>& data);
  base::android::ScopedJavaLocalRef<jobjectArray> GetData(JNIEnv* env);
  jboolean IsRendererControllingBrowserControlsOffsets(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jstring> RegisterWebMessageCallback(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& js_object_name,
      const base::android::JavaParamRef<jobjectArray>& origins,
      const base::android::JavaParamRef<jobject>& client);
  void UnregisterWebMessageCallback(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& js_object_name);
  jboolean CanTranslate(JNIEnv* env);
  void ShowTranslateUi(JNIEnv* env);
  void RemoveTabFromBrowserBeforeDestroying(JNIEnv* env);
  void SetTranslateTargetLanguage(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& translate_target_lang);
#endif

  ErrorPageDelegate* error_page_delegate() { return error_page_delegate_; }

  void AddDataObserver(DataObserver* observer);
  void RemoveDataObserver(DataObserver* observer);

  GoogleAccountsDelegate* google_accounts_delegate() {
    return google_accounts_delegate_;
  }

  // Tab:
  void SetErrorPageDelegate(ErrorPageDelegate* delegate) override;
  void SetFullscreenDelegate(FullscreenDelegate* delegate) override;
  void SetNewTabDelegate(NewTabDelegate* delegate) override;
  void SetGoogleAccountsDelegate(GoogleAccountsDelegate* delegate) override;
  void AddObserver(TabObserver* observer) override;
  void RemoveObserver(TabObserver* observer) override;
  NavigationController* GetNavigationController() override;
  void ExecuteScript(const base::string16& script,
                     bool use_separate_isolate,
                     JavaScriptResultCallback callback) override;
  const std::string& GetGuid() override;
  void SetData(const std::map<std::string, std::string>& data) override;
  const std::map<std::string, std::string>& GetData() override;
  base::string16 AddWebMessageHostFactory(
      std::unique_ptr<WebMessageHostFactory> factory,
      const base::string16& js_object_name,
      const std::vector<std::string>& js_origins) override;
  void RemoveWebMessageHostFactory(
      const base::string16& js_object_name) override;
  std::unique_ptr<FaviconFetcher> CreateFaviconFetcher(
      FaviconFetcherDelegate* delegate) override;
#if !defined(OS_ANDROID)
  void AttachToView(views::WebView* web_view) override;
#endif

  void WebPreferencesChanged();
  void SetWebPreferences(blink::web_pref::WebPreferences* prefs);

  // Executes |script| with a user gesture.
  void ExecuteScriptWithUserGestureForTests(const base::string16& script);

  // Initializes the autofill system with |provider| for tests.
  void InitializeAutofillForTests(
      std::unique_ptr<autofill::AutofillProvider> provider);

 private:
  // content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void ShowRepostFormWarningDialog(content::WebContents* source) override;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* web_contents) override;
  content::ColorChooser* OpenColorChooser(
      content::WebContents* web_contents,
      SkColor color,
      const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions)
      override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  void CreateSmsPrompt(content::RenderFrameHost*,
                       const url::Origin&,
                       const std::string& one_time_code,
                       base::OnceClosure on_confirm,
                       base::OnceClosure on_cancel) override;
  int GetTopControlsHeight() override;
  int GetTopControlsMinHeight() override;
  int GetBottomControlsHeight() override;
  bool DoBrowserControlsShrinkRendererSize(
      content::WebContents* web_contents) override;
  bool OnlyExpandTopControlsAtPageTop() override;
  bool ShouldAnimateBrowserControlsHeightChanges() override;
  bool EmbedsFullscreenWidget() override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  void EnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenModeForTab(content::WebContents* web_contents) override;
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) override;
  blink::mojom::DisplayMode GetDisplayMode(
      const content::WebContents* web_contents) override;
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;
  void CloseContents(content::WebContents* source) override;
  void FindReply(content::WebContents* web_contents,
                 int request_id,
                 int number_of_matches,
                 const gfx::Rect& selection_rect,
                 int active_match_ordinal,
                 bool final_update) override;
#if defined(OS_ANDROID)
  void FindMatchRectsReply(content::WebContents* web_contents,
                           int version,
                           const std::vector<gfx::RectF>& rects,
                           const gfx::RectF& active_rect) override;

  // Pointer arguments are outputs. Check the preconditions for capturing a
  // screenshot and either set all outputs, or return an error code, in which
  // case the state of output arguments is undefined.
  ScreenShotErrors PrepareForCaptureScreenShot(
      float scale,
      content::RenderWidgetHostView** rwhv,
      gfx::Rect* src_rect,
      gfx::Size* output_size);

  void UpdateBrowserControlsState(content::BrowserControlsState new_state,
                                  bool animate);
#endif

  // content::WebContentsObserver:
  void RenderProcessGone(base::TerminationStatus status) override;
  void DidChangeVisibleSecurityState() override;

  // find_in_page::FindResultObserver:
  void OnFindResultAvailable(content::WebContents* web_contents) override;

#if defined(OS_ANDROID)
  // BrowserControlsNavigationStateHandlerDelegate:
  void OnBrowserControlsStateStateChanged(
      ControlsVisibilityReason reason,
      content::BrowserControlsState state) override;
  void OnUpdateBrowserControlsStateBecauseOfProcessSwitch(
      bool did_commit) override;
#endif

  // Called from closure supplied to delegate to exit fullscreen.
  void OnExitFullscreen();

  void UpdateRendererPrefs(bool should_sync_prefs);

  void InitializeAutofill();

  // Returns the FindTabHelper for the page, or null if none exists.
  find_in_page::FindTabHelper* GetFindTabHelper();

  static sessions::SessionTabHelperDelegate* GetSessionServiceTabHelperDelegate(
      content::WebContents* web_contents);

#if defined(OS_ANDROID)
  void SetBrowserControlsConstraint(ControlsVisibilityReason reason,
                                    content::BrowserControlsState constraint);
#endif

  void UpdateBrowserVisibleSecurityStateIfNecessary();

  bool SetDataInternal(const std::map<std::string, std::string>& data);

  BrowserImpl* browser_ = nullptr;
  ErrorPageDelegate* error_page_delegate_ = nullptr;
  FullscreenDelegate* fullscreen_delegate_ = nullptr;
  NewTabDelegate* new_tab_delegate_ = nullptr;
  GoogleAccountsDelegate* google_accounts_delegate_ = nullptr;
  ProfileImpl* profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<NavigationControllerImpl> navigation_controller_;
  base::ObserverList<TabObserver>::Unchecked observers_;
  std::unique_ptr<i18n::LocaleChangeSubscription> locale_change_subscription_;

#if defined(OS_ANDROID)
  BrowserControlsContainerView* top_controls_container_view_ = nullptr;
  BrowserControlsContainerView* bottom_controls_container_view_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> java_impl_;
  std::unique_ptr<BrowserControlsNavigationStateHandler>
      browser_controls_navigation_state_handler_;

  // Last value supplied to UpdateBrowserControlsConstraint(). This *constraint*
  // can be SHOWN, if for example a modal dialog is forcing the controls to be
  // visible, HIDDEN, if for example fullscreen is forcing the controls to be
  // hidden, or BOTH, if either state is viable (e.g. during normal browsing).
  // When BOTH, the actual current state could be showing or hidden.
  content::BrowserControlsState
      current_browser_controls_visibility_constraint_ =
          content::BROWSER_CONTROLS_STATE_SHOWN;

  std::map<std::string, std::unique_ptr<WebMessageHostFactoryProxy>>
      js_name_to_proxy_;
#endif

  bool is_fullscreen_ = false;
  // Set to true doing EnterFullscreenModeForTab().
  bool processing_enter_fullscreen_ = false;

  std::unique_ptr<autofill::AutofillProvider> autofill_provider_;

  const std::string guid_;

  std::map<std::string, std::string> data_;
  base::ObserverList<DataObserver>::Unchecked data_observers_;

  base::string16 title_;

  std::unique_ptr<js_injection::JsCommunicationHost> js_communication_host_;

  base::WeakPtrFactory<TabImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TabImpl);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_TAB_IMPL_H_
