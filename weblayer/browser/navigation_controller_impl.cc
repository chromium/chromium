// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/navigation_controller_impl.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "weblayer/browser/navigation_ui_data_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/navigation_observer.h"

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "base/trace_event/trace_event.h"
#include "components/embedder_support/android/util/web_resource_response.h"
#include "weblayer/browser/java/jni/NavigationControllerImpl_jni.h"
#endif

#if defined(OS_ANDROID)
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
  void ScheduleNavigate(
      std::unique_ptr<content::NavigationController::LoadURLParams> params) {
    load_params_ = std::move(params);
  }

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override {
    const bool should_cancel = should_cancel_;
    if (load_params_)
      controller_->DoNavigate(std::move(load_params_));
    // WARNING: this may have been deleted.
    return should_cancel ? CANCEL : PROCEED;
  }

  ThrottleCheckResult WillRedirectRequest() override {
    controller_->WillRedirectRequest(this, navigation_handle());

    const bool should_cancel = should_cancel_;
    if (load_params_)
      controller_->DoNavigate(std::move(load_params_));
    // WARNING: this may have been deleted.
    return should_cancel ? CANCEL : PROCEED;
  }

  const char* GetNameForLogging() override {
    return "WebLayerNavigationControllerThrottle";
  }

 private:
  NavigationControllerImpl* controller_;
  bool should_cancel_ = false;
  std::unique_ptr<content::NavigationController::LoadURLParams> load_params_;
};

NavigationControllerImpl::NavigationControllerImpl(TabImpl* tab)
    : WebContentsObserver(tab->web_contents()) {}

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
  auto load_params = navigation->TakeParamsToLoadWhenSafe();
  if (load_params)
    throttle->ScheduleNavigate(std::move(load_params));
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

#if defined(OS_ANDROID)
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

  if (!response.is_null()) {
    data->SetResponse(
        std::make_unique<embedder_support::WebResourceResponse>(response));
  }

  params->navigation_ui_data = std::move(data);

  if (enable_auto_play)
    params->was_activated = content::mojom::WasActivatedOption::kYes;

  DoNavigate(std::move(params));
}

ScopedJavaLocalRef<jstring>
NavigationControllerImpl::GetNavigationEntryDisplayUri(
    JNIEnv* env,
    int index) {
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
#if defined(OS_ANDROID)
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
  if (params.disable_network_error_auto_reload) {
    auto data = std::make_unique<NavigationUIDataImpl>();
    data->set_disable_network_error_auto_reload(true);
    load_params->navigation_ui_data = std::move(data);
  }
  if (params.enable_auto_play)
    load_params->was_activated = content::mojom::WasActivatedOption::kYes;

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
  return web_contents()->GetController().GetEntryCount();
}

int NavigationControllerImpl::GetNavigationListCurrentIndex() {
  return web_contents()->GetController().GetCurrentEntryIndex();
}

GURL NavigationControllerImpl::GetNavigationEntryDisplayURL(int index) {
  auto* entry = web_contents()->GetController().GetEntryAtIndex(index);
  if (!entry)
    return GURL();
  return entry->GetVirtualURL();
}

std::string NavigationControllerImpl::GetNavigationEntryTitle(int index) {
  auto* entry = web_contents()->GetController().GetEntryAtIndex(index);
  if (!entry)
    return std::string();
  return base::UTF16ToUTF8(entry->GetTitle());
}

bool NavigationControllerImpl::IsNavigationEntrySkippable(int index) {
  return web_contents()->GetController().IsEntryMarkedToBeSkipped(index);
}

void NavigationControllerImpl::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
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
  navigation->set_safe_to_set_user_agent(true);
#if defined(OS_ANDROID)
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
      Java_NavigationControllerImpl_createNavigation(
          env, java_controller_, reinterpret_cast<jlong>(navigation));
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
}

void NavigationControllerImpl::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  // NOTE: this implementation should remain empty. Real implementation is in
  // WillRedirectNavigation(). See description of NavigationThrottleImpl for
  // more information.
}

void NavigationControllerImpl::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  DCHECK(navigation_map_.find(navigation_handle) != navigation_map_.end());
  auto* navigation = navigation_map_[navigation_handle].get();
#if defined(OS_ANDROID)
  if (java_controller_) {
    TRACE_EVENT0("weblayer",
                 "Java_NavigationControllerImpl_readyToCommitNavigation");
    Java_NavigationControllerImpl_readyToCommitNavigation(
        AttachCurrentThread(), java_controller_, navigation->java_navigation());
  }
#endif
  for (auto& observer : observers_)
    observer.ReadyToCommitNavigation(navigation);
}

void NavigationControllerImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  DelayDeletionHelper deletion_helper(this);
  DCHECK(navigation_map_.find(navigation_handle) != navigation_map_.end());
  auto* navigation = navigation_map_[navigation_handle].get();
  if (navigation_handle->GetNetErrorCode() == net::OK &&
      !navigation_handle->IsErrorPage()) {
#if defined(OS_ANDROID)
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
#if defined(OS_ANDROID)
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
  web_contents()->GetMainFrame()->InsertVisualStateCallback(base::BindOnce(
      &NavigationControllerImpl::OldPageNoLongerRendered,
      weak_ptr_factory_.GetWeakPtr(), navigation_handle->GetURL()));

  navigation_map_.erase(navigation_map_.find(navigation_handle));
}

void NavigationControllerImpl::DidStartLoading() {
  NotifyLoadStateChanged();
}

void NavigationControllerImpl::DidStopLoading() {
  NotifyLoadStateChanged();
}

void NavigationControllerImpl::LoadProgressChanged(double progress) {
#if defined(OS_ANDROID)
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
#if defined(OS_ANDROID)
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
#if defined(OS_ANDROID)
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
#if defined(OS_ANDROID)
  if (java_controller_) {
    TRACE_EVENT0("weblayer", "Java_NavigationControllerImpl_loadStateChanged");
    Java_NavigationControllerImpl_loadStateChanged(
        AttachCurrentThread(), java_controller_, web_contents()->IsLoading(),
        web_contents()->IsLoadingToDifferentDocument());
  }
#endif
  for (auto& observer : observers_) {
    observer.LoadStateChanged(web_contents()->IsLoading(),
                              web_contents()->IsLoadingToDifferentDocument());
  }
}

void NavigationControllerImpl::DoNavigate(
    std::unique_ptr<content::NavigationController::LoadURLParams> params) {
  // Navigations should use the default user-agent. If the embedder wants a
  // custom user-agent, the embedder will call Navigation::SetUserAgentString().
  params->override_user_agent =
      content::NavigationController::UA_OVERRIDE_FALSE;
  if (navigation_starting_) {
    // DoNavigate() is being called reentrantly. Delay processing until it's
    // safe.
    Stop();
    navigation_starting_->SetParamsToLoadWhenSafe(std::move(params));
    return;
  }

  if (active_throttle_) {
    // DoNavigate() is being called reentrantly. Delay processing until it's
    // safe.
    Stop();
    active_throttle_->ScheduleNavigate(std::move(params));
    return;
  }

  params->has_user_gesture = true;
  web_contents()->GetController().LoadURLWithParams(*params);
  // So that if the user had entered the UI in a bar it stops flashing the
  // caret.
  web_contents()->Focus();
}

#if defined(OS_ANDROID)
static jlong JNI_NavigationControllerImpl_GetNavigationController(JNIEnv* env,
                                                                  jlong tab) {
  return reinterpret_cast<jlong>(
      reinterpret_cast<Tab*>(tab)->GetNavigationController());
}
#endif

}  // namespace weblayer
