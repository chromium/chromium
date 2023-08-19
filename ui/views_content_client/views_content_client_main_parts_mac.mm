// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "content/public/browser/context_factory.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/result_codes.h"
#include "content/shell/browser/shell_application_mac.h"
#include "content/shell/browser/shell_browser_context.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views_content_client/views_content_client.h"
#include "ui/views_content_client/views_content_client_main_parts.h"

// A simple NSApplicationDelegate that provides a basic mainMenu and can
// activate a task when the application has finished loading.
@interface ViewsContentClientAppController : NSObject <NSApplicationDelegate> {
 @private
  base::OnceClosure _onApplicationDidFinishLaunching;
}

// Set the task to run after receiving -applicationDidFinishLaunching:.
- (void)setOnApplicationDidFinishLaunching:(base::OnceClosure)task;

@end

namespace ui {

namespace {

class ViewsContentClientMainPartsMac : public ViewsContentClientMainParts {
 public:
  explicit ViewsContentClientMainPartsMac(
      ViewsContentClient* views_content_client);

  ViewsContentClientMainPartsMac(const ViewsContentClientMainPartsMac&) =
      delete;
  ViewsContentClientMainPartsMac& operator=(
      const ViewsContentClientMainPartsMac&) = delete;

  ~ViewsContentClientMainPartsMac() override;

  // content::BrowserMainParts:
  int PreMainMessageLoopRun() override;

 private:
  ViewsContentClientAppController* __strong app_controller_;
};

ViewsContentClientMainPartsMac::ViewsContentClientMainPartsMac(
    ViewsContentClient* views_content_client)
    : ViewsContentClientMainParts(views_content_client) {
  // Cache the child process path to avoid triggering an AssertIOAllowed.
  base::FilePath child_process_exe;
  base::PathService::Get(content::CHILD_PROCESS_EXE, &child_process_exe);

  app_controller_ = [[ViewsContentClientAppController alloc] init];
  NSApplication.sharedApplication.delegate = app_controller_;
}

int ViewsContentClientMainPartsMac::PreMainMessageLoopRun() {
  ViewsContentClientMainParts::PreMainMessageLoopRun();

  views_delegate()->set_context_factory(content::GetContextFactory());

  // On Mac, the task must be deferred to applicationDidFinishLaunching. If not,
  // the widget can activate, but (even if configured) the mainMenu won't be
  // ready to switch over in the OSX UI, so it will look strange.
  NSWindow* window_context = nil;
  [app_controller_
      setOnApplicationDidFinishLaunching:
          base::BindOnce(&ViewsContentClient::OnPreMainMessageLoopRun,
                         base::Unretained(views_content_client()),
                         base::Unretained(browser_context()),
                         base::Unretained(window_context))];

  return content::RESULT_CODE_NORMAL_EXIT;
}

ViewsContentClientMainPartsMac::~ViewsContentClientMainPartsMac() {
  NSApplication.sharedApplication.delegate = nil;
}

}  // namespace

// static
std::unique_ptr<ViewsContentClientMainParts>
ViewsContentClientMainParts::Create(ViewsContentClient* views_content_client) {
  return std::make_unique<ViewsContentClientMainPartsMac>(views_content_client);
}

// static
void ViewsContentClientMainParts::PreBrowserMain() {
  // Simply instantiating an instance of ShellCrApplication serves to register
  // it as the application class. Do make sure that no other code has done this
  // first, though.
  CHECK_EQ(NSApp, nil);
  [ShellCrApplication sharedApplication];
}

}  // namespace ui

@implementation ViewsContentClientAppController

- (void)setOnApplicationDidFinishLaunching:(base::OnceClosure)task {
  _onApplicationDidFinishLaunching = std::move(task);
}

- (void)applicationDidFinishLaunching:(NSNotification*)aNotification {
  // To get key events, the application needs to have an activation policy.
  // Unbundled apps (i.e. those without an Info.plist) default to
  // NSApplicationActivationPolicyProhibited.
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

  // Create a basic mainMenu object using the executable filename.
  NSMenu* mainMenu = [[NSMenu alloc] initWithTitle:@""];
  NSMenuItem* appMenuItem = [mainMenu addItemWithTitle:@""
                                                action:nullptr
                                         keyEquivalent:@""];
  [NSApp setMainMenu:mainMenu];

  NSMenu* appMenu = [[NSMenu alloc] initWithTitle:@""];
  NSString* appName = NSProcessInfo.processInfo.processName;
  // TODO(tapted): Localize "Quit" if this is ever used for a released binary.
  // At the time of writing, ui_strings.grd has "Close" but not "Quit".
  NSString* quitTitle = [@"Quit " stringByAppendingString:appName];
  [appMenu addItemWithTitle:quitTitle
                     action:@selector(terminate:)
              keyEquivalent:@"q"];
  [appMenuItem setSubmenu:appMenu];

  CHECK([NSApp isKindOfClass:[ShellCrApplication class]]);

  std::move(_onApplicationDidFinishLaunching).Run();
}

@end
