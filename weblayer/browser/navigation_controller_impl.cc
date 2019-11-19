// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/navigation_controller_impl.h"

#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/navigation_observer.h"

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "base/trace_event/trace_event.h"
#include "weblayer/browser/java/jni/NavigationControllerImpl_jni.h"
#endif

#if defined(OS_ANDROID)
using base::android::AttachCurrentThread;
#endif

namespace weblayer {

NavigationControllerImpl::NavigationControllerImpl(TabImpl* tab)
    : WebContentsObserver(tab->web_contents()) {}

NavigationControllerImpl::~NavigationControllerImpl() = default;

#if defined(OS_ANDROID)
void NavigationControllerImpl::SetNavigationControllerImpl(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_controller) {
  java_controller_.Reset(env, java_controller);
}

void NavigationControllerImpl::Navigate(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& url) {
  Navigate(GURL(base::android::ConvertJavaStringToUTF8(env, url)));
}

base::android::ScopedJavaLocalRef<jstring>
NavigationControllerImpl::GetNavigationEntryDisplayUri(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    int index) {
  return base::android::ScopedJavaLocalRef<jstring>(
      base::android::ConvertUTF8ToJavaString(
          env, GetNavigationEntryDisplayURL(index).spec()));
}
#endif

void NavigationControllerImpl::AddObserver(NavigationObserver* observer) {
  observers_.AddObserver(observer);
}

void NavigationControllerImpl::RemoveObserver(NavigationObserver* observer) {
  observers_.RemoveObserver(observer);
}

void NavigationControllerImpl::Navigate(const GURL& url) {
  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents()->GetController().LoadURLWithParams(params);
  // So that if the user had entered the UI in a bar it stops flashing the
  // caret.
  web_contents()->Focus();
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

void NavigationControllerImpl::Reload() {
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, false);
}

void NavigationControllerImpl::Stop() {
  web_contents()->Stop();
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

void NavigationControllerImpl::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  navigation_map_[navigation_handle] =
      std::make_unique<NavigationImpl>(navigation_handle);
  auto* navigation = navigation_map_[navigation_handle].get();
#if defined(OS_ANDROID)
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
}

void NavigationControllerImpl::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  DCHECK(navigation_map_.find(navigation_handle) != navigation_map_.end());
  auto* navigation = navigation_map_[navigation_handle].get();
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
    }
#endif
    for (auto& observer : observers_)
      observer.NavigationCompleted(navigation);
  } else {
#if defined(OS_ANDROID)
    if (java_controller_) {
      TRACE_EVENT0("weblayer",
                   "Java_NavigationControllerImpl_navigationFailed");
      Java_NavigationControllerImpl_navigationFailed(
          AttachCurrentThread(), java_controller_,
          navigation->java_navigation());
    }
#endif
    for (auto& observer : observers_)
      observer.NavigationFailed(navigation);
  }

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

#if defined(OS_ANDROID)
static jlong JNI_NavigationControllerImpl_GetNavigationController(JNIEnv* env,
                                                                  jlong tab) {
  return reinterpret_cast<jlong>(
      reinterpret_cast<Tab*>(tab)->GetNavigationController());
}
#endif

}  // namespace weblayer
