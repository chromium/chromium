// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/shell/browser/shell.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "url/gurl.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/public/tab.h"

namespace weblayer {

// Null until/unless the default main message loop is running.
base::NoDestructor<base::OnceClosure> g_quit_main_message_loop;

const int kDefaultTestWindowWidthDip = 1000;
const int kDefaultTestWindowHeightDip = 700;

std::vector<Shell*> Shell::windows_;

Shell::Shell(std::unique_ptr<Tab> tab)
    : tab_(std::move(tab)), window_(nullptr) {
  windows_.push_back(this);
  if (tab_) {
    tab_->AddObserver(this);
    tab_->GetNavigationController()->AddObserver(this);
  }
}

Shell::~Shell() {
  if (tab_) {
    tab_->GetNavigationController()->RemoveObserver(this);
    tab_->RemoveObserver(this);
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
  tab_.reset();

  if (windows_.empty()) {
    if (*g_quit_main_message_loop)
      std::move(*g_quit_main_message_loop).Run();
  }
}

Shell* Shell::CreateShell(std::unique_ptr<Tab> tab,
                          const gfx::Size& initial_size) {
  Shell* shell = new Shell(std::move(tab));
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

  // If there were no windows open then the message loop quit closure will
  // not have been run.
  if (*g_quit_main_message_loop)
    std::move(*g_quit_main_message_loop).Run();

  PlatformExit();
}

void Shell::SetMainMessageLoopQuitClosure(base::OnceClosure quit_closure) {
  *g_quit_main_message_loop = std::move(quit_closure);
}

Tab* Shell::tab() {
#if defined(OS_ANDROID)
  // TODO(jam): this won't work if we need more than one Shell in a test.
  return Tab::GetLastTabForTesting();
#else
  return tab_.get();
#endif
}

void Shell::Initialize() {
  PlatformInitialize(GetShellDefaultSize());
}

void Shell::DisplayedUrlChanged(const GURL& url) {
  PlatformSetAddressBarURL(url);
}

void Shell::LoadStateChanged(bool is_loading, bool to_different_document) {
  NavigationController* navigation_controller = tab_->GetNavigationController();

  PlatformEnableUIControl(STOP_BUTTON, is_loading && to_different_document);

  // TODO(estade): These should be updated in callbacks that correspond to the
  // back/forward list changing, such as NavigationEntriesDeleted.
  PlatformEnableUIControl(BACK_BUTTON, navigation_controller->CanGoBack());
  PlatformEnableUIControl(FORWARD_BUTTON,
                          navigation_controller->CanGoForward());
}

void Shell::LoadProgressChanged(double progress) {
  PlatformSetLoadProgress(progress);
}

gfx::Size Shell::AdjustWindowSize(const gfx::Size& initial_size) {
  if (!initial_size.IsEmpty())
    return initial_size;
  return GetShellDefaultSize();
}

Shell* Shell::CreateNewWindow(weblayer::Profile* web_profile,
                              const GURL& url,
                              const gfx::Size& initial_size) {
#if defined(OS_ANDROID)
  std::unique_ptr<Tab> tab;
#else
  auto tab = Tab::Create(web_profile);
#endif

  Shell* shell = CreateShell(std::move(tab), AdjustWindowSize(initial_size));
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
