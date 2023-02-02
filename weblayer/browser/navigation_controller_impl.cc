// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/navigation_controller_impl.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/digital_asset_links/response_header_verifier.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/navigation/was_activated_option.mojom-shared.h"
#include "ui/base/page_transition_types.h"
#include "weblayer/browser/browser_impl.h"
#include "weblayer/browser/navigation_entry_data.h"
#include "weblayer/browser/navigation_ui_data_impl.h"
#include "weblayer/browser/page_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/navigation_observer.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_string.h"
#include "base/trace_event/trace_event.h"
#include "components/embedder_support/android/util/web_resource_response.h"
#include "weblayer/browser/java/jni/NavigationControllerImpl_jni.h"
#endif

#if BUILDFLAG(IS_ANDROID)
using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
#endif

namespace weblayer {

class NavigationControllerImpl::DelayDeletionHelper {
 public:
  explicit DelayDeletionHelper(NavigationControllerImpl* controller)
      : controller_(controller->weak_ptr_factory_.GetWeakPtr()) {
    // This should never be called reentrantly.
    DCHECK(!controller->should_delay_web_contents_deletion_);
    controller->should_delay_web_contents_deletion_ = true;
  }

  DelayDeletionHelper(const DelayDeletionHelper&) = delete;
  DelayDeletionHelper& operator=(const DelayDeletionHelper&) = delete;

  ~DelayDeletionHelper() {
    if (controller_)
      controller_->should_delay_web_contents_deletion_ = false;
  }

  bool WasControllerDeleted() { return controller_.get() == nullptr; }

 private:
  base::WeakPtr<NavigationControllerImpl> controller_;
};

// NavigationThrottle implementation responsible for delaying certain
// operations and performing them when safe. This is necessary as content
// does allow certain operations to be called at certain times. For example,
// content does not allow calling WebContents::Stop() from
// WebContentsObserver::DidStartNavigation() (to do so crashes). To work around
// this NavigationControllerImpl detects these scenarios and delays processing
// until safe.
//
// Most of the support for these scenarios is handled by a custom
// NavigationThrottle.  To make things interesting, the NavigationThrottle is
// created after some of the scenarios this code wants to handle. As such,
// NavigationImpl does some amount of caching until the NavigationThrottle is
// created.
class NavigationControllerImpl::NavigationThrottleImpl
    : public content::NavigationThrottle {
 public:
  NavigationThrottleImpl(NavigationControllerImpl* controller,
                         content::NavigationHandle* handle)
      : NavigationThrottle(handle), controller_(controller) {}
  NavigationThrottleImpl(const NavigationThrottleImpl&) = delete;
  NavigationThrottleImpl& operator=(const NavigationThrottleImpl&) = delete;
  ~NavigationThrottleImpl() override = default;

  void ScheduleCancel() { should_cancel_ = true; }

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override {
    return should_cancel_ ? CANCEL : PROCEED;
  }

  ThrottleCheckResult WillRedirectRequest() override {
    controller_->WillRedirectRequest(this, navigation_handle());
    return should_cancel_ ? CANCEL : PROCEED;
  }

  const char* GetNameForLogging() override {
    return "WebLayerNavigationControllerThrottle";
  }

 private:
  raw_ptr<NavigationControllerImpl> controller_;
  bool should_cancel_ = false;
};

NavigationControllerImpl::NavigationControllerImpl(TabImpl* tab)
    : WebContentsObserver(tab->web_contents()), tab_(tab) {}

NavigationControllerImpl::~NavigationControllerImpl() = default;

std::unique_ptr<content::NavigationThrottle>
NavigationControllerImpl::CreateNavigationThrottle(
    content::NavigationHandle* handle) {
  if (!handle->IsInMainFrame())
    return nullptr;

  auto throttle = std::make_unique<NavigationThrottleImpl>(this, handle);
  DCHECK(navigation_map_.find(handle) != navigation_map_.end());
  auto* navigation = navigation_map_[handle].get();
  if (navigation->should_stop_when_throttle_created())
    throttle->ScheduleCancel();
  return throttle;
}

NavigationImpl* NavigationControllerImpl::GetNavigationImplFromHandle(
    content::NavigationHandle* handle) {
  auto iter = navigation_map_.find(handle);
  return iter == navigation_map_.end() ? nullptr : iter->second.get();
}

NavigationImpl* NavigationControllerImpl::GetNavigationImplFromId(
    int64_t navigation_id) {
  for (const auto& iter : navigation_map_) {
    if (iter.first->GetNavigationId() == navigation_id)
      return iter.second.get();
  }

  return nullptr;
}

void NavigationControllerImpl::OnFirstContentfulPaint(
    const base::TimeTicks& navigation_start,
    const base::TimeDelta& first_contentful_paint) {
#if BUILDFLAG(IS_ANDROID)
  TRACE_EVENT0("weblayer",
               "Java_NavigationControllerImpl_onFirstContentfulPaint2");
  int64_t first_contentful_paint_ms = first_contentful_paint.InMilliseconds();
  Java_NavigationControllerImpl_onFirstContentfulPaint2(
      AttachCurrentThread(), java_controller_,
      navigation_start.ToUptimeMillis(), first_contentful_paint_ms);
#endif

  for (auto& observer : observers_)
    observer.OnFirstContentfulPaint(navigation_start, first_contentful_paint);
}

void NavigationControllerImpl::OnLargestContentfulPaint(
    const base::TimeTicks& navigation_start,
    const base::TimeDelta& largest_contentful_paint) {
#if BUILDFLAG(IS_ANDROID)
  TRACE_EVENT0("weblayer",
               "Java_NavigationControllerImpl_onLargestContentfulPaint2");
  int64_t largest_contentful_paint_ms =
      largest_contentful_paint.InMilliseconds();
  Java_NavigationControllerImpl_onLargestContentfulPaint(
      AttachCurrentThread(), java_controller_,
      navigation_start.ToUptimeMillis(), largest_contentful_paint_ms);
#endif

  for (auto& observer : observers_)
    observer.OnLargestContentfulPaint(navigation_start,
                                      largest_contentful_paint);
}

void NavigationControllerImpl::OnPageDestroyed(Page* page) {
  for (auto& observer : observers_)
    observer.OnPageDestroyed(page);
}

void NavigationControllerImpl::OnPageLanguageDetermined(
    Page* page,
    const std::string& language) {
#if BUILDFLAG(IS_ANDROID)
  JNIEnv* env = AttachCurrentThread();
  Java_NavigationControllerImpl_onPageLanguageDetermined(
      env, java_controller_, static_cast<PageImpl*>(page)->java_page(),
      base::android::ConvertUTF8ToJavaString(env, language));
#endif

  for (auto& observer : observers_)
    observer.OnPageLanguageDetermined(page, language);
}

#if BUILDFLAG(IS_ANDROID)
void NavigationControllerImpl::SetNavigationControllerImpl(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_controller) {
  java_controller_ = java_controller;
}

void NavigationControllerImpl::Navigate(
    JNIEnv* env,
    const JavaParamRef<jstring>& url,
    jboolean should_replace_current_entry,
    jboolean disable_intent_processing,
    jboolean allow_intent_launches_in_background,
    jboolean disable_network_error_auto_reload,
    jboolean enable_auto_play,
    const base::android::JavaParamRef<jobject>& response) {
  auto params = std::make_unique<content::NavigationController::LoadURLParams>(
      GURL(base::android::ConvertJavaStringToUTF8(env, url)));
  params->should_replace_current_entry = should_replace_current_entry;
  // On android, the transition type largely dictates whether intent processing
  // happens. PAGE_TRANSITION_TYPED does not process intents, where as
  // PAGE_TRANSITION_LINK will (with the caveat that even links may not trigger
  // intent processing under some circumstances).
  params->transition_type = disable_intent_processing
                                ? ui::PAGE_TRANSITION_TYPED
                                : ui::PAGE_TRANSITION_LINK;
  auto data = std::make_unique<NavigationUIDataImpl>();

  if (disable_network_error_auto_reload)
    data->set_disable_network_error_auto_reload(true);

  data->set_allow_intent_launches_in_background(
      allow_intent_launches_in_background);

  if (!response.is_null()) {
    data->SetResponse(
        std::make_unique<embedder_support::WebResourceResponse>(response));
  }

  params->navigation_ui_data = std::move(data);

  if (enable_auto_play)
    params->was_activated = blink::mojom::WasActivatedOption::kYes;

  DoNavigate(std::move(params));
}

ScopedJavaLocalRef<jstring>
NavigationControllerImpl::GetNavigationEntryDisplayUri(JNIEnv* env, int index) {
  return ScopedJavaLocalRef<jstring>(base::android::ConvertUTF8ToJavaString(
      env, GetNavigationEntryDisplayURL(index).spec()));
}

ScopedJavaLocalRef<jstring> NavigationControllerImpl::GetNavigationEntryTitle(
    JNIEnv* env,
    int index) {
  return ScopedJavaLocalRef<jstring>(base::android::ConvertUTF8ToJavaString(
      env, GetNavigationEntryTitle(index)));
}

bool NavigationControllerImpl::IsNavigationEntrySkippable(JNIEnv* env,
                                                          int index) {
  return IsNavigationEntrySkippable(index);
}

base::android::ScopedJavaGlobalRef<jobject>
NavigationControllerImpl::GetNavigationImplFromId(JNIEnv* env, int64_t id) {
  auto* navigation_impl = GetNavigationImplFromId(id);
  return navigation_impl ? navigation_impl->java_navigation() : nullptr;
}

#endif

void NavigationControllerImpl::WillRedirectRequest(
    NavigationThrottleImpl* throttle,
    content::NavigationHandle* navigation_handle) {
  DCHECK(navigation_handle->IsInMainFrame());
  DCHECK(navigation_map_.find(navigation_handle) != navigation_map_.end());
  auto* navigation = navigation_map_[navigation_handle].get();
  navigation->set_safe_to_set_request_headers(true);
  DCHECK(!active_throttle_);
  base::AutoReset<NavigationThrottleImpl*> auto_reset(&active_throttle_,
                                                      throttle);
#if BUILDFLAG(IS_ANDROID)
  if (java_controller_) {
    TRACE_EVENT0("weblayer",
                 "Java_NavigationControllerImpl_navigationRedirected");
    Java_NavigationControllerImpl_navigationRedirected(
        AttachCurrentThread(), java_controller_, navigation->java_navigation());
  }
#endif
  for (auto& observer : observers_)
    observer.NavigationRedirected(navigation);
  navigation->set_safe_to_set_request_headers(false);
}

void NavigationControllerImpl::AddObserver(NavigationObserver* observer) {
  observers_.AddObserver(observer);
}

void NavigationControllerImpl::RemoveObserver(NavigationObserver* observer) {
  observers_.RemoveObserver(observer);
}

void NavigationControllerImpl::Navigate(const GURL& url) {
  DoNavigate(
      std::make_unique<content::NavigationController::LoadURLParams>(url));
}

void NavigationControllerImpl::Navigate(
    const GURL& url,
    const NavigationController::NavigateParams& params) {
  auto load_params =
      std::make_unique<content::NavigationController::LoadURLParams>(url);
  load_params->should_replace_current_entry =
      params.should_replace_current_entry;
  if (params.enable_auto_play)
    load_params->was_activated = blink::mojom::WasActivatedOption::kYes;

  DoNavigate(std::move(load_params));
}

void NavigationControllerImpl::GoBack() {
  web_contents()->GetController().GoBack();
}

void NavigationControllerImpl::GoForward() {
  web_contents()->GetController().GoForward();
}

bool NavigationControllerImpl::CanGoBack() {
  return web_contents()->GetController().CanGoBack();
}

bool NavigationControllerImpl::CanGoForward() {
  return web_contents()->GetController().CanGoForward();
}

void NavigationControllerImpl::GoToIndex(int index) {
  web_contents()->GetController().GoToIndex(index);
}

void NavigationControllerImpl::Reload() {
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, true);
}

void NavigationControllerImpl::Stop() {
  CancelDelayedLoad();

  NavigationImpl* navigation = nullptr;
  if (navigation_starting_) {
    navigation_starting_->set_should_stop_when_throttle_created();
    navigation = navigation_starting_;
  } else if (active_throttle_) {
    active_throttle_->ScheduleCancel();
    DCHECK(navigation_map_.find(active_throttle_->navigation_handle()) !=
           navigation_map_.end());
    navigation = navigation_map_[active_throttle_->navigation_handle()].get();
  } else {
    web_contents()->Stop();
  }

  if (navigation)
    navigation->set_was_stopped();
}

int NavigationControllerImpl::GetNavigationListSize() {
  if (web_contents()
          ->GetController()
          .GetLastCommittedEntry()
          ->IsInitialEntry()) {
    // If we're currently on the initial NavigationEntry, no navigation has
    // committed, so the initial NavigationEntry should not be part of the
    // "Navigation List", and we should return 0 as the navigation list size.
    // This also preserves the old behavior where we used to not have the
    // initial NavigationEntry.
    return 0;
  }

  return web_contents()->GetController().GetEntryCount();
}

int NavigationControllerImpl::GetNavigationListCurrentIndex() {
  if (web_contents()
          ->GetController()
          .GetLastCommittedEntry()
          ->IsInitialEntry()) {
    // If we're currently on the initial NavigationEntry, no navigation has
    // committed, so the initial NavigationEntry should not be part of the
    // "Navigation List", and we should return -1 as the current index. This
    // also preserves the old behavior where we used to not have the initial
    // NavigationEntry.
    return -1;
  }

  return web_contents()->GetController().GetCurrentEntryIndex();
}

GURL NavigationControllerImpl::GetNavigationEntryDisplayURL(int index) {
  auto* entry = web_contents()->GetController().GetEntryAtIndex(index);
  // This function should never be called when GetNavigationListSize() is 0
  // because `index` should be between 0 and GetNavigationListSize() - 1, which
  // also means `entry` must not be the initial NavigationEntry.
  DCHECK_NE(0, GetNavigationListSize());
  DCHECK(!entry->IsInitialEntry());
  return entry->GetVirtualURL();
}

std::string NavigationControllerImpl::GetNavigationEntryTitle(int index) {
  auto* entry = web_contents()->GetController().GetEntryAtIndex(index);
  // This function should never be called when GetNavigationListSize() is 0
  // because `index` should be between 0 and GetNavigationListSize() - 1, which
  // also means `entry` must not be the initial NavigationEntry.
  DCHECK_NE(0, GetNavigationListSize());
  DCHECK(!entry->IsInitialEntry());
  return base::UTF16ToUTF8(entry->GetTitle());
}

bool NavigationControllerImpl::IsNavigationEntrySkippable(int index) {
  return web_contents()->GetController().IsEntryMarkedToBeSkipped(index);
}

void NavigationControllerImpl::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
  if (!navigation_handle->IsInPrimaryMainFrame())
    return;

  // This function should not be called reentrantly.
  DCHECK(!navigation_starting_);

  DCHECK(!base::Contains(navigation_map_, navigation_handle));
  navigation_map_[navigation_handle] =
      std::make_unique<NavigationImpl>(navigation_handle);
  auto* navigation = navigation_map_[navigation_handle].get();
  base::AutoReset<NavigationImpl*> auto_reset(&navigation_starting_,
                                              navigation);
  navigation->set_safe_to_set_request_headers(true);
  navigation->set_safe_to_disable_network_error_auto_reload(true);
  navigation->set_safe_to_disable_intent_processing(true);

#if BUILDFLAG(IS_ANDROID)
  // Desktop mode and per-navigation UA use the same mechanism and so don't
  // interact well. It's not possible to support both at the same time since
  // if there's a per-navigation UA active and desktop mode is turned on, or
  // was on previously, the WebContent's state would have to change before
  // navigation even though that would be wrong for the previous navigation if
  // the new navigation didn't commit.
  if (!TabImpl::FromWebContents(web_contents())->desktop_user_agent_enabled())
#endif
    navigation->set_safe_to_set_user_agent(true);

#if BUILDFLAG(IS_ANDROID)
  NavigationUIDataImpl* navigation_ui_data = static_cast<NavigationUIDataImpl*>(
      navigation_handle->GetNavigationUIData());
  if (navigation_ui_data) {
    auto response = navigation_ui_data->TakeResponse();
    if (response)
      navigation->SetResponse(std::move(response));
  }

  if (java_controller_) {
    JNIEnv* env = AttachCurrentThread();
    {
      TRACE_EVENT0("weblayer",
                   "Java_NavigationControllerImpl_createNavigation");
      ScopedJavaLocalRef<jobject> java_navigation =
          Java_NavigationControllerImpl_createNavigation(
              env, java_controller_, reinterpret_cast<jlong>(navigation));
      navigation->SetJavaNavigation(
          base::android::ScopedJavaGlobalRef<jobject>(java_navigation));
    }
    TRACE_EVENT0("weblayer", "Java_NavigationControllerImpl_navigationStarted");
    Java_NavigationControllerImpl_navigationStarted(
        env, java_controller_, navigation->java_navigation());
  }
#endif
  for (auto& observer : observers_)
    observer.NavigationStarted(navigation);
  navigation->set_safe_to_set_user_agent(false);
  navigation->set_safe_to_set_request_headers(false);
  navigation->set_safe_to_disable_network_error_auto_reload(false);
  navigation->set_safe_to_disable_intent_processing(false);
}

void NavigationControllerImpl::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  // NOTE: this implementation should remain empty. Real implementation is in
  // WillRedirectNavigation(). See description of NavigationThrottleImpl for
  // more information.
}

void NavigationControllerImpl::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
#if BUILDFLAG(IS_ANDROID)
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
  if (!navigation_handle->IsInPrimaryMainFrame())
    return;

  DCHECK(navigation_map_.find(navigation_handle) != navigation_map_.end());
  auto* navigation = navigation_map_[navigation_handle].get();
  if (java_controller_) {
    TRACE_EVENT0("weblayer",
                 "Java_NavigationControllerImpl_readyToCommitNavigation");
    Java_NavigationControllerImpl_readyToCommitNavigation(
        AttachCurrentThread(), java_controller_, navigation->java_navigation());
  }
#endif
}

void NavigationControllerImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
  if (!navigation_handle->IsInPrimaryMainFrame())
    return;

  DelayDeletionHelper deletion_helper(this);
  DCHECK(navigation_map_.find(navigation_handle) != navigation_map_.end());
  auto* navigation = navigation_map_[navigation_handle].get();

  navigation->set_finished();

  if (navigation_handle->HasCommitted()) {
    // Set state on NavigationEntry user data if a per-navigation user agent was
    // specified. This can't be done earlier because a NavigationEntry might not
    // have existed at the time that SetUserAgentString was called.
    if (navigation->set_user_agent_string_called()) {
      auto* entry = web_contents()->GetController().GetLastCommittedEntry();
      if (entry) {
        auto* entry_data = NavigationEntryData::Get(entry);
        if (entry_data)
          entry_data->set_per_navigation_user_agent_override(true);
      }
    }

    auto* rfh = navigation_handle->GetRenderFrameHost();
    PageImpl::GetOrCreateForPage(rfh->GetPage());
    navigation->set_safe_to_get_page();

#if BUILDFLAG(IS_ANDROID)
    // Ensure that the Java-side Page object for this navigation is
    // populated from and linked to the native Page object. Without this
    // call, the Java-side navigation object won't be created and linked to
    // the native object until/unless the client calls Navigation#getPage(),
    // which is problematic when implementation-side callers need to bridge
    // the C++ Page object into Java (e.g., to fire
    // NavigationCallback#onPageLanguageDetermined()).
    Java_NavigationControllerImpl_getOrCreatePageForNavigation(
        AttachCurrentThread(), java_controller_, navigation->java_navigation());
#endif
  }

  // In some corner cases (e.g., a tab closing with an ongoing navigation)
  // navigations finish without committing but without any other error state.
  // Such navigations are regarded as failed by WebLayer.
  if (navigation_handle->HasCommitted() &&
      navigation_handle->GetNetErrorCode() == net::OK &&
      !navigation_handle->IsErrorPage()) {
    if (!navigation_handle->IsSameDocument()) {
      navigation->set_consenting_content(
          digital_asset_links::ResponseHeaderVerifier::Verify(
              tab_->browser()->GetPackageName(),
              navigation->GetNormalizedHeader(
                  digital_asset_links::kEmbedderAncestorHeader)));
    }
#if BUILDFLAG(IS_ANDROID)
    if (java_controller_) {
      TRACE_EVENT0("weblayer",
                   "Java_NavigationControllerImpl_navigationCompleted");
      Java_NavigationControllerImpl_navigationCompleted(
          AttachCurrentThread(), java_controller_,
          navigation->java_navigation());
      if (deletion_helper.WasControllerDeleted())
        return;
    }
#endif
    for (auto& observer : observers_) {
      observer.NavigationCompleted(navigation);
      if (deletion_helper.WasControllerDeleted())
        return;
    }
  } else {
#if BUILDFLAG(IS_ANDROID)
    navigation->set_consenting_content(false);
    if (java_controller_) {
      TRACE_EVENT0("weblayer",
                   "Java_NavigationControllerImpl_navigationFailed");
      Java_NavigationControllerImpl_navigationFailed(
          AttachCurrentThread(), java_controller_,
          navigation->java_navigation());
      if (deletion_helper.WasControllerDeleted())
        return;
    }
#endif
    for (auto& observer : observers_) {
      observer.NavigationFailed(navigation);
      if (deletion_helper.WasControllerDeleted())
        return;
    }
  }

  // Note InsertVisualStateCallback currently does not take into account
  // any delays from surface sync, ie a frame submitted by renderer may not
  // be displayed immediately. Such situations should be rare however, so
  // this should be good enough for the purposes needed.
  web_contents()->GetPrimaryMainFrame()->InsertVisualStateCallback(
      base::BindOnce(&NavigationControllerImpl::OldPageNoLongerRendered,
                     weak_ptr_factory_.GetWeakPtr(),
                     navigation_handle->GetURL()));

  navigation_map_.erase(navigation_map_.find(navigation_handle));
}

void NavigationControllerImpl::DidStartLoading() {
  NotifyLoadStateChanged();
}

void NavigationControllerImpl::DidStopLoading() {
  NotifyLoadStateChanged();
}

void NavigationControllerImpl::LoadProgressChanged(double progress) {
#if BUILDFLAG(IS_ANDROID)
  if (java_controller_) {
    TRACE_EVENT0("weblayer",
                 "Java_NavigationControllerImpl_loadProgressChanged");
    Java_NavigationControllerImpl_loadProgressChanged(
        AttachCurrentThread(), java_controller_, progress);
  }
#endif
  for (auto& observer : observers_)
    observer.LoadProgressChanged(progress);
}

void NavigationControllerImpl::DidFirstVisuallyNonEmptyPaint() {
#if BUILDFLAG(IS_ANDROID)
  TRACE_EVENT0("weblayer",
               "Java_NavigationControllerImpl_onFirstContentfulPaint");
  Java_NavigationControllerImpl_onFirstContentfulPaint(AttachCurrentThread(),
                                                       java_controller_);
#endif

  for (auto& observer : observers_)
    observer.OnFirstContentfulPaint();
}

void NavigationControllerImpl::OldPageNoLongerRendered(const GURL& url,
                                                       bool success) {
#if BUILDFLAG(IS_ANDROID)
  TRACE_EVENT0("weblayer",
               "Java_NavigationControllerImpl_onOldPageNoLongerRendered");
  JNIEnv* env = AttachCurrentThread();
  Java_NavigationControllerImpl_onOldPageNoLongerRendered(
      env, java_controller_,
      base::android::ConvertUTF8ToJavaString(env, url.spec()));
#endif
  for (auto& observer : observers_)
    observer.OnOldPageNoLongerRendered(url);
}

void NavigationControllerImpl::NotifyLoadStateChanged() {
#if BUILDFLAG(IS_ANDROID)
  if (java_controller_) {
    TRACE_EVENT0("weblayer", "Java_NavigationControllerImpl_loadStateChanged");
    Java_NavigationControllerImpl_loadStateChanged(
        AttachCurrentThread(), java_controller_, web_contents()->IsLoading(),
        web_contents()->ShouldShowLoadingUI());
  }
#endif
  for (auto& observer : observers_) {
    observer.LoadStateChanged(web_contents()->IsLoading(),
                              web_contents()->ShouldShowLoadingUI());
  }
}

void NavigationControllerImpl::DoNavigate(
    std::unique_ptr<content::NavigationController::LoadURLParams> params) {
  CancelDelayedLoad();

  // Navigations should use the default user-agent (which may be overridden if
  // desktop mode is turned on). If the embedder wants a custom user-agent, the
  // embedder will call Navigation::SetUserAgentString() in DidStartNavigation.
#if BUILDFLAG(IS_ANDROID)
  // We need to set UA_OVERRIDE_FALSE if per navigation UA is set. However at
  // this point we don't know if the embedder will call that later. Since we
  // ensure that the two can't be set at the same time, it's sufficient to
  // not enable it if desktop mode is turned on.
  if (!TabImpl::FromWebContents(web_contents())->desktop_user_agent_enabled())
#endif
    params->override_user_agent =
        content::NavigationController::UA_OVERRIDE_FALSE;
  if (navigation_starting_ || active_throttle_) {
    // DoNavigate() is being called reentrantly. Delay processing until it's
    // safe.
    Stop();
    ScheduleDelayedLoad(std::move(params));
    return;
  }

  params->has_user_gesture = true;
  web_contents()->GetController().LoadURLWithParams(*params);
  // So that if the user had entered the UI in a bar it stops flashing the
  // caret.
  web_contents()->Focus();
}

void NavigationControllerImpl::ScheduleDelayedLoad(
    std::unique_ptr<content::NavigationController::LoadURLParams> params) {
  delayed_load_params_ = std::move(params);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&NavigationControllerImpl::ProcessDelayedLoad,
                                weak_ptr_factory_.GetWeakPtr()));
}

void NavigationControllerImpl::CancelDelayedLoad() {
  delayed_load_params_.reset();
}

void NavigationControllerImpl::ProcessDelayedLoad() {
  if (delayed_load_params_)
    DoNavigate(std::move(delayed_load_params_));
}

#if BUILDFLAG(IS_ANDROID)
static jlong JNI_NavigationControllerImpl_GetNavigationController(JNIEnv* env,
                                                                  jlong tab) {
  return reinterpret_cast<jlong>(
      reinterpret_cast<Tab*>(tab)->GetNavigationController());
}
#endif

}  // namespace weblayer
