// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "url/gurl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/public/tab.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/test_navigation_observer.h"

namespace weblayer {

namespace {

// Navigates to |url| in |shell| and waits for |event| to occur.
void NavigateAndWaitForEvent(const GURL& url,
                             Shell* shell,
                             TestNavigationObserver::NavigationEvent event) {
  TestNavigationObserver test_observer(url, event, shell);
  shell->tab()->GetNavigationController()->Navigate(url);
  test_observer.Wait();
}

}  // namespace

void NavigateAndWaitForCompletion(const GURL& url, Shell* shell) {
  NavigateAndWaitForEvent(url, shell,
                          TestNavigationObserver::NavigationEvent::Completion);
}

void NavigateAndWaitForFailure(const GURL& url, Shell* shell) {
  NavigateAndWaitForEvent(url, shell,
                          TestNavigationObserver::NavigationEvent::Failure);
}

base::Value ExecuteScript(Shell* shell,
                          const std::string& script,
                          bool use_separate_isolate) {
  base::Value final_result;
  base::RunLoop run_loop;
  shell->tab()->ExecuteScript(
      base::ASCIIToUTF16(script), use_separate_isolate,
      base::BindLambdaForTesting(
          [&run_loop, &final_result](base::Value result) {
            final_result = std::move(result);
            run_loop.Quit();
          }));
  run_loop.Run();
  return final_result;
}

const base::string16& GetTitle(Shell* shell) {
  TabImpl* tab_impl = static_cast<TabImpl*>(shell->tab());

  return tab_impl->web_contents()->GetTitle();
}

}  // namespace weblayer
