// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/test_navigation_observer.h"

#include "base/test/bind_test_util.h"
#include "url/gurl.h"
#include "weblayer/public/navigation.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/public/tab.h"
#include "weblayer/shell/browser/shell.h"

namespace weblayer {

TestNavigationObserver::TestNavigationObserver(const GURL& url,
                                               NavigationEvent target_event,
                                               Shell* shell)
    : TestNavigationObserver(url, target_event, shell->tab()) {}

TestNavigationObserver::TestNavigationObserver(const GURL& url,
                                               NavigationEvent target_event,
                                               Tab* tab)
    : url_(url), target_event_(target_event), tab_(tab) {
  tab_->GetNavigationController()->AddObserver(this);
}

TestNavigationObserver::~TestNavigationObserver() {
  tab_->GetNavigationController()->RemoveObserver(this);
}

void TestNavigationObserver::NavigationStarted(Navigation* navigation) {
  // Note: We don't go through CheckNavigationCompleted() here as that waits
  // for the load to be complete, which isn't appropriate when just waiting for
  // the navigation to be started.
  if (navigation->GetURL() == url_ &&
      target_event_ == NavigationEvent::kStart) {
    run_loop_.Quit();
  }
}

void TestNavigationObserver::NavigationCompleted(Navigation* navigation) {
  if (navigation->GetURL() == url_)
    observed_event_ = NavigationEvent::kCompletion;
  CheckNavigationCompleted();
}

void TestNavigationObserver::NavigationFailed(Navigation* navigation) {
  if (navigation->GetURL() == url_)
    observed_event_ = NavigationEvent::kFailure;
  CheckNavigationCompleted();
}

void TestNavigationObserver::LoadStateChanged(bool is_loading,
                                              bool to_different_document) {
  done_loading_ = !is_loading;
  CheckNavigationCompleted();
}

void TestNavigationObserver::CheckNavigationCompleted() {
  if (done_loading_ && observed_event_ == target_event_)
    run_loop_.Quit();
}

void TestNavigationObserver::Wait() {
  run_loop_.Run();
}

}  // namespace weblayer
