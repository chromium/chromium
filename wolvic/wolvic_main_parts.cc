// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/wolvic_main_parts.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "content/public/common/result_codes.h"
#include "content/shell/browser/shell_devtools_manager_delegate.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#include "wolvic/browser/mojo/wolvic_interface_registrar.h"
#include "wolvic/browser/webdata_services/web_data_service_factory.h"
#include "wolvic/wolvic_browser_context.h"

namespace wolvic {

// TODO(jfernandez): Should define these constants in a separated file ?
namespace wolvic {
  const char kInitialProfile[] = "Default";
}

namespace {

  static base::FilePath GetInitialProfileDir() {
  base::FilePath profile_dir;
  base::PathService::Get(base::DIR_ANDROID_APP_DATA, &profile_dir);
  return profile_dir.AppendASCII(wolvic::kInitialProfile);
  }
}

WolvicMainParts::WolvicMainParts() {}

WolvicMainParts::~WolvicMainParts() {}

int WolvicMainParts::PreEarlyInitialization() {
  net::NetworkChangeNotifier::SetFactory(
      new net::NetworkChangeNotifierFactoryAndroid());

  return content::RESULT_CODE_NORMAL_EXIT;
}

int WolvicMainParts::PreMainMessageLoopRun() {
  // Required before profile creation).
  PreProfileInit();

  set_browser_context(new WolvicBrowserContext(GetInitialProfileDir(), false));
  set_off_the_record_browser_context(new WolvicBrowserContext(GetInitialProfileDir(), true));
  content::ShellDevToolsManagerDelegate::StartHttpHandler(
      browser_context_.get());

  PostBrowserStart();

  return 0;
}

void WolvicMainParts::PreProfileInit() {
  EnsureBrowserContextKeyedServiceFactoriesBuilt();
}

void WolvicMainParts::EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  WebDataServiceFactory::GetInstance();
}

void WolvicMainParts::PostMainMessageLoopRun() {
  content::ShellDevToolsManagerDelegate::StopHttpHandler();
}

void WolvicMainParts::PostBrowserStart() {
  LOG(WARNING) << "WolvicMainParts::PostBrowserStart --";

  RegisterWolvicJavaMojoInterfaces();
}

void WolvicMainParts::set_browser_context(WolvicBrowserContext* context) {
  browser_context_.reset(context);
}

void WolvicMainParts::set_off_the_record_browser_context(
    WolvicBrowserContext* context) {
  off_the_record_browser_context_.reset(context);
}

}  // namespace wolvic
