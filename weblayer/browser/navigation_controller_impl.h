// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NAVIGATION_CONTROLLER_IMPL_H_
#define WEBLAYER_BROWSER_NAVIGATION_CONTROLLER_IMPL_H_

#include <map>

#include "base/macros.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents_observer.h"
#include "weblayer/browser/navigation_impl.h"
#include "weblayer/public/navigation_controller.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace weblayer {
class TabImpl;

class NavigationControllerImpl : public NavigationController,
                                 public content::WebContentsObserver {
 public:
  explicit NavigationControllerImpl(TabImpl* tab);
  ~NavigationControllerImpl() override;

#if defined(OS_ANDROID)
  void SetNavigationControllerImpl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_controller);
  void Navigate(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                const base::android::JavaParamRef<jstring>& url);
  void GoBack(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj) {
    GoBack();
  }
  void GoForward(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj) {
    GoForward();
  }
  bool CanGoBack(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj) {
    return CanGoBack();
  }
  bool CanGoForward(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj) {
    return CanGoForward();
  }
  void Reload(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj) {
    Reload();
  }
  void Stop(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj) {
    Stop();
  }
  int GetNavigationListSize(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj) {
    return GetNavigationListSize();
  }
  int GetNavigationListCurrentIndex(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj) {
    return GetNavigationListCurrentIndex();
  }
  base::android::ScopedJavaLocalRef<jstring> GetNavigationEntryDisplayUri(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      int index);
#endif

 private:
  // NavigationController implementation:
  void AddObserver(NavigationObserver* observer) override;
  void RemoveObserver(NavigationObserver* observer) override;
  void Navigate(const GURL& url) override;
  void GoBack() override;
  void GoForward() override;
  bool CanGoBack() override;
  bool CanGoForward() override;
  void Reload() override;
  void Stop() override;
  int GetNavigationListSize() override;
  int GetNavigationListCurrentIndex() override;
  GURL GetNavigationEntryDisplayURL(int index) override;

  // content::WebContentsObserver implementation:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidStartLoading() override;
  void DidStopLoading() override;
  void LoadProgressChanged(double progress) override;
  void DidFirstVisuallyNonEmptyPaint() override;

  void NotifyLoadStateChanged();

  base::ObserverList<NavigationObserver>::Unchecked observers_;
  std::map<content::NavigationHandle*, std::unique_ptr<NavigationImpl>>
      navigation_map_;

#if defined(OS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_controller_;
#endif

  DISALLOW_COPY_AND_ASSIGN(NavigationControllerImpl);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NAVIGATION_CONTROLLER_IMPL_H_
