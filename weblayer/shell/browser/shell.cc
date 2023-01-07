// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/shell/browser/shell.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "url/gurl.h"
#include "weblayer/browser/browser_impl.h"
#include "weblayer/browser/browser_list.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/public/prerender_controller.h"

namespace weblayer {

// Null until/unless the default main message loop is running.
base::NoDestructor<base::OnceClosure> g_quit_main_message_loop;

const int kDefaultTestWindowWidthDip = 1000;
const int kDefaultTestWindowHeightDip = 700;

std::vector<Shell*> Shell::windows_;

Shell::Shell(std::unique_ptr<Browser> browser)
    : browser_(std::move(browser)), window_(nullptr) {
  windows_.push_back(this);
  if (tab()) {
    tab()->AddObserver(this);
    tab()->GetNavigationController()->AddObserver(this);
#if !BUILDFLAG(IS_ANDROID)  // Android does this in Java.
    static_cast<TabImpl*>(tab())->profile()->SetDownloadDelegate(this);
#endif
  }
}

Shell::~Shell() {
  if (tab()) {
    tab()->GetNavigationController()->RemoveObserver(this);
    tab()->RemoveObserver(this);
#if !BUILDFLAG(IS_ANDROID)  // Android does this in Java.
    static_cast<TabImpl*>(tab())->profile()->SetDownloadDelegate(nullptr);
#endif
  }
  PlatformCleanUp();

  for (size_t i = 0; i < windows_.size(); ++i) {
    if (windows_[i] == this) {
      windows_.erase(windows_.begin() + i);
      break;
    }
  }

  // Always destroy WebContents before calling PlatformExit(). WebContents
  // destruction sequence may depend on the resources destroyed in
  // PlatformExit() (e.g. the display::Screen singleton).
  if (tab()) {
    auto* const profile = static_cast<TabImpl*>(tab())->profile();
    profile->GetPrerenderController()->DestroyAllContents();
  }
  browser_.reset();

  if (windows_.empty()) {
    PlatformExit();

    if (*g_quit_main_message_loop)
      std::move(*g_quit_main_message_loop).Run();
  }
}

Shell* Shell::CreateShell(std::unique_ptr<Browser> browser,
                          const gfx::Size& initial_size) {
  Shell* shell = new Shell(std::move(browser));
  shell->PlatformCreateWindow(initial_size.width(), initial_size.height());

  shell->PlatformSetContents();

  shell->PlatformResizeSubViews();

  return shell;
}

void Shell::CloseAllWindows() {
  std::vector<Shell*> open_windows(windows_);
  for (size_t i = 0; i < open_windows.size(); ++i)
    open_windows[i]->Close();

  // Pump the message loop to allow window teardown tasks to run.
  base::RunLoop().RunUntilIdle();
}

void Shell::SetMainMessageLoopQuitClosure(base::OnceClosure quit_closure) {
  *g_quit_main_message_loop = std::move(quit_closure);
}

Tab* Shell::tab() {
  if (!browser())
    return nullptr;
  if (browser()->GetTabs().empty())
    return nullptr;
  return browser()->GetTabs()[0];
}

Browser* Shell::browser() {
#if BUILDFLAG(IS_ANDROID)
  // TODO(jam): this won't work if we need more than one Shell in a test.
  const auto& browsers = BrowserList::GetInstance()->browsers();
  if (browsers.empty())
    return nullptr;
  return *(browsers.begin());
#else
  return browser_.get();
#endif
}

void Shell::Initialize() {
  PlatformInitialize(GetShellDefaultSize());
}

void Shell::DisplayedUrlChanged(const GURL& url) {
  PlatformSetAddressBarURL(url);
}

void Shell::LoadStateChanged(bool is_loading, bool should_show_loading_ui) {
  NavigationController* navigation_controller =
      tab()->GetNavigationController();

  PlatformEnableUIControl(STOP_BUTTON, is_loading && should_show_loading_ui);

  // TODO(estade): These should be updated in callbacks that correspond to the
  // back/forward list changing, such as NavigationEntriesDeleted.
  PlatformEnableUIControl(BACK_BUTTON, navigation_controller->CanGoBack());
  PlatformEnableUIControl(FORWARD_BUTTON,
                          navigation_controller->CanGoForward());
}

void Shell::LoadProgressChanged(double progress) {
  PlatformSetLoadProgress(progress);
}

bool Shell::InterceptDownload(const GURL& url,
                              const std::string& user_agent,
                              const std::string& content_disposition,
                              const std::string& mime_type,
                              int64_t content_length) {
  return false;
}

void Shell::AllowDownload(Tab* tab,
                          const GURL& url,
                          const std::string& request_method,
                          absl::optional<url::Origin> request_initiator,
                          AllowDownloadCallback callback) {
  std::move(callback).Run(true);
}

gfx::Size Shell::AdjustWindowSize(const gfx::Size& initial_size) {
  if (!initial_size.IsEmpty())
    return initial_size;
  return GetShellDefaultSize();
}

#if BUILDFLAG(IS_ANDROID)
Shell* Shell::CreateNewWindow(const GURL& url, const gfx::Size& initial_size) {
  // On Android, the browser is owned by the Java side.
  return CreateNewWindowWithBrowser(nullptr, url, initial_size);
}
#else
Shell* Shell::CreateNewWindow(Profile* web_profile,
                              const GURL& url,
                              const gfx::Size& initial_size) {
  auto browser = Browser::Create(web_profile, nullptr);
  browser->CreateTab();
  return CreateNewWindowWithBrowser(std::move(browser), url, initial_size);
}
#endif

Shell* Shell::CreateNewWindowWithBrowser(std::unique_ptr<Browser> browser,
                                         const GURL& url,
                                         const gfx::Size& initial_size) {
  Shell* shell =
      CreateShell(std::move(browser), AdjustWindowSize(initial_size));
  if (!url.is_empty())
    shell->LoadURL(url);
  return shell;
}

void Shell::LoadURL(const GURL& url) {
  tab()->GetNavigationController()->Navigate(url);
}

void Shell::GoBackOrForward(int offset) {
  if (offset == -1)
    tab()->GetNavigationController()->GoBack();
  else if (offset == 1)
    tab()->GetNavigationController()->GoForward();
}

void Shell::Reload() {
  tab()->GetNavigationController()->Reload();
}

void Shell::ReloadBypassingCache() {}

void Shell::Stop() {
  tab()->GetNavigationController()->Stop();
}

gfx::Size Shell::GetShellDefaultSize() {
  static gfx::Size default_shell_size;
  if (!default_shell_size.IsEmpty())
    return default_shell_size;
  default_shell_size =
      gfx::Size(kDefaultTestWindowWidthDip, kDefaultTestWindowHeightDip);
  return default_shell_size;
}

}  // namespace weblayer
