// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/wolvic_main_parts.h"

#include "content/public/common/result_codes.h"
#include "content/shell/browser/shell_devtools_manager_delegate.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#include "wolvic/wolvic_browser_context.h"

namespace wolvic {

WolvicMainParts::WolvicMainParts() {}

WolvicMainParts::~WolvicMainParts() {}

int WolvicMainParts::PreEarlyInitialization() {
  net::NetworkChangeNotifier::SetFactory(
      new net::NetworkChangeNotifierFactoryAndroid());

  return content::RESULT_CODE_NORMAL_EXIT;
}

int WolvicMainParts::PreMainMessageLoopRun() {
  set_browser_context(new WolvicBrowserContext(false));
  set_off_the_record_browser_context(new WolvicBrowserContext(true));
  content::ShellDevToolsManagerDelegate::StartHttpHandler(
      browser_context_.get());
  return 0;
}

void WolvicMainParts::PostMainMessageLoopRun() {
  content::ShellDevToolsManagerDelegate::StopHttpHandler();
}

void WolvicMainParts::set_browser_context(WolvicBrowserContext* context) {
  browser_context_.reset(context);
}

void WolvicMainParts::set_off_the_record_browser_context(
    WolvicBrowserContext* context) {
  off_the_record_browser_context_.reset(context);
}

}  // namespace wolvic
