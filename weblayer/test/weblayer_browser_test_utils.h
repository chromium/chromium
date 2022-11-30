// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_TEST_WEBLAYER_BROWSER_TEST_UTILS_H_
#define WEBLAYER_TEST_WEBLAYER_BROWSER_TEST_UTILS_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "build/build_config.h"
#include "weblayer/public/navigation.h"
#include "weblayer/public/navigation_observer.h"

class GURL;

namespace autofill {
struct FormData;
}

namespace content {
class WebContents;
}

namespace weblayer {
class Shell;
class Tab;

// Navigates |shell| to |url| and wait for completed navigation.
void NavigateAndWaitForCompletion(const GURL& url, Shell* shell);

void NavigateAndWaitForCompletion(const GURL& url, Tab* tab);

// Navigates |shell| to |url| and wait for failed navigation.
void NavigateAndWaitForFailure(const GURL& url, Shell* shell);

// Initiates navigation to |url| in |tab| and waits for it to start.
void NavigateAndWaitForStart(const GURL& url, Tab* tab);

// Executes |script| in |shell| and returns the result.
base::Value ExecuteScript(Shell* shell,
                          const std::string& script,
                          bool use_separate_isolate);
base::Value ExecuteScript(Tab* tab,
                          const std::string& script,
                          bool use_separate_isolate);

// Executes |script| in |shell| with a user gesture. Useful for tests of
// functionality that gates action on a user gesture having occurred.
// Differs from ExecuteScript() as follows:
// - Does not notify the caller of the result as the underlying implementation
//   does not. Thus, unlike the above, the caller of this function will need to
//   explicitly listen *after* making this call for any expected event to
//   occur.
// - Does not allow running in a separate isolate as the  machinery for
//   setting a user gesture works only in the main isolate.
void ExecuteScriptWithUserGesture(Shell* shell, const std::string& script);
void ExecuteScriptWithUserGesture(Tab* tab, const std::string& script);

/// Gets the title of the current webpage in |shell|.
const std::u16string& GetTitle(Shell* shell);

#if BUILDFLAG(IS_ANDROID)
// Sets up the autofill system to be one that simply forwards detected forms to
// the passed-in callback.
void InitializeAutofillWithEventForwarding(
    Shell* shell,
    const base::RepeatingCallback<void(const autofill::FormData&)>&
        on_received_form_data);
#endif  // BUILDFLAG(IS_ANDROID)

// Configures the subresource filter to activate on |url| in |web_contents|.
void ActivateSubresourceFilterInWebContentsForURL(
    content::WebContents* web_contents,
    const GURL& url);

class OneShotNavigationObserver : public NavigationObserver {
 public:
  explicit OneShotNavigationObserver(Shell* shell);

  ~OneShotNavigationObserver() override;

  void WaitForNavigation();

  bool completed() { return completed_; }
  bool is_error_page() { return is_error_page_; }
  bool is_download() { return is_download_; }
  bool is_reload() { return is_reload_; }
  bool was_stop_called() { return was_stop_called_; }
  Navigation::LoadError load_error() { return load_error_; }
  int http_status_code() { return http_status_code_; }
  NavigationState navigation_state() { return navigation_state_; }
  bool is_page_initiated() const { return is_page_initiated_; }

 private:
  // NavigationObserver implementation:
  void NavigationStarted(Navigation* navigation) override;
  void NavigationCompleted(Navigation* navigation) override;
  void NavigationFailed(Navigation* navigation) override;

  void Finish(Navigation* navigation);

  base::RunLoop run_loop_;
  raw_ptr<Tab> tab_;
  bool completed_ = false;
  bool is_error_page_ = false;
  bool is_download_ = false;
  bool is_reload_ = false;
  bool was_stop_called_ = false;
  bool is_page_initiated_ = false;
  Navigation::LoadError load_error_ = Navigation::kNoError;
  int http_status_code_ = 0;
  NavigationState navigation_state_ = NavigationState::kWaitingResponse;
};

}  // namespace weblayer

#endif  // WEBLAYER_TEST_WEBLAYER_BROWSER_TEST_UTILS_H_
