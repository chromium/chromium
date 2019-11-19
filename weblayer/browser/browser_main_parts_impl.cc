// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_main_parts_impl.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/message_loop/message_loop_current.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/url_constants.h"
#include "services/service_manager/embedder/result_codes.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "weblayer/browser/webui/web_ui_controller_factory.h"
#include "weblayer/public/main.h"

#if defined(OS_ANDROID)
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#endif

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"  // nogncheck
#endif
#if defined(USE_AURA) && defined(USE_X11)
#include "ui/events/devices/x11/touch_factory_x11.h"  // nogncheck
#endif
#if !defined(OS_CHROMEOS) && defined(USE_AURA) && defined(OS_LINUX)
#include "ui/base/ime/init/input_method_initializer.h"
#endif

namespace weblayer {

namespace {

void StopMessageLoop(base::OnceClosure quit_closure) {
  for (auto it = content::RenderProcessHost::AllHostsIterator(); !it.IsAtEnd();
       it.Advance()) {
    it.GetCurrentValue()->DisableKeepAliveRefCount();
  }

  std::move(quit_closure).Run();
}

}  // namespace

BrowserMainPartsImpl::BrowserMainPartsImpl(
    MainParams* params,
    const content::MainFunctionParams& main_function_params)
    : params_(params), main_function_params_(main_function_params) {}

BrowserMainPartsImpl::~BrowserMainPartsImpl() = default;

void BrowserMainPartsImpl::PreMainMessageLoopStart() {
#if defined(USE_AURA) && defined(USE_X11)
  ui::TouchFactory::SetTouchDeviceListFromCommandLine();
#endif
}

int BrowserMainPartsImpl::PreEarlyInitialization() {
#if defined(USE_X11)
  ui::SetDefaultX11ErrorHandlers();
#endif
#if defined(USE_AURA) && defined(OS_LINUX)
  ui::InitializeInputMethodForTesting();
#endif
#if defined(OS_ANDROID)
  net::NetworkChangeNotifier::SetFactory(
      new net::NetworkChangeNotifierFactoryAndroid());
#endif
  return service_manager::RESULT_CODE_NORMAL_EXIT;
}

void BrowserMainPartsImpl::PreMainMessageLoopRun() {
  ui::MaterialDesignController::Initialize();
  params_->delegate->PreMainMessageLoopRun();

  content::WebUIControllerFactory::RegisterFactory(
      WebUIControllerFactory::GetInstance());

  if (main_function_params_.ui_task) {
    main_function_params_.ui_task->Run();
    delete main_function_params_.ui_task;
    run_message_loop_ = false;
  }
}

bool BrowserMainPartsImpl::MainMessageLoopRun(int* result_code) {
  return !run_message_loop_;
}

void BrowserMainPartsImpl::PreDefaultMainMessageLoopRun(
    base::OnceClosure quit_closure) {
  // Wrap the method that stops the message loop so we can do other shutdown
  // cleanup inside content.
  params_->delegate->SetMainMessageLoopQuitClosure(
      base::BindOnce(StopMessageLoop, std::move(quit_closure)));
}

}  // namespace weblayer
