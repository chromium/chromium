// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_main_parts_impl.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/task/current_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/cookie_settings_factory.h"
#include "weblayer/browser/feature_list_creator.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/i18n_util.h"
#include "weblayer/browser/no_state_prefetch/prerender_link_manager_factory.h"
#include "weblayer/browser/no_state_prefetch/prerender_manager_factory.h"
#include "weblayer/browser/permissions/weblayer_permissions_client.h"
#include "weblayer/browser/stateful_ssl_host_state_delegate_factory.h"
#include "weblayer/browser/translate_accept_languages_factory.h"
#include "weblayer/browser/translate_ranker_factory.h"
#include "weblayer/browser/webui/web_ui_controller_factory.h"
#include "weblayer/public/main.h"

#if defined(OS_ANDROID)
#include "base/command_line.h"
#include "components/crash/content/browser/child_exit_observer_android.h"
#include "components/crash/content/browser/child_process_crash_observer_android.h"
#include "components/crash/core/common/crash_key.h"
#include "components/javascript_dialogs/android/app_modal_dialog_view_android.h"  // nogncheck
#include "components/javascript_dialogs/app_modal_dialog_manager.h"  // nogncheck
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#include "weblayer/browser/android/metrics/uma_utils.h"
#include "weblayer/browser/java/jni/MojoInterfaceRegistrar_jni.h"
#include "weblayer/browser/media/local_presentation_manager_factory.h"
#include "weblayer/browser/media/media_router_factory.h"
#include "weblayer/browser/weblayer_factory_impl_android.h"
#include "weblayer/common/features.h"
#endif

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"  // nogncheck
#endif
#if defined(USE_AURA) && defined(USE_X11)
#include "ui/base/ui_base_features.h"
#include "ui/events/devices/x11/touch_factory_x11.h"  // nogncheck
#endif
#if !defined(OS_CHROMEOS) && defined(USE_AURA) && defined(OS_LINUX)
#include "ui/base/ime/init/input_method_initializer.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "weblayer/browser/captive_portal_service_factory.h"
#endif

namespace weblayer {

namespace {

// Instantiates all weblayer KeyedService factories, which is
// especially important for services that should be created at profile
// creation time as compared to lazily on first access.
void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  CaptivePortalServiceFactory::GetInstance();
#endif
  HostContentSettingsMapFactory::GetInstance();
  StatefulSSLHostStateDelegateFactory::GetInstance();
  CookieSettingsFactory::GetInstance();
  TranslateAcceptLanguagesFactory::GetInstance();
  TranslateRankerFactory::GetInstance();
  PrerenderLinkManagerFactory::GetInstance();
  PrerenderManagerFactory::GetInstance();
#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kMediaRouter)) {
    LocalPresentationManagerFactory::GetInstance();
    MediaRouterFactory::GetInstance();
  }
#endif
}

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
    const content::MainFunctionParams& main_function_params,
    std::unique_ptr<PrefService> local_state)
    : params_(params),
      main_function_params_(main_function_params),
      local_state_(std::move(local_state)) {}

BrowserMainPartsImpl::~BrowserMainPartsImpl() = default;

int BrowserMainPartsImpl::PreCreateThreads() {
  // Make sure permissions client has been set.
  WebLayerPermissionsClient::GetInstance();
#if defined(OS_ANDROID)
  // The ChildExitObserver needs to be created before any child process is
  // created because it needs to be notified during process creation.
  crash_reporter::ChildExitObserver::Create();
  crash_reporter::ChildExitObserver::GetInstance()->RegisterClient(
      std::make_unique<crash_reporter::ChildProcessCrashObserver>());

  crash_reporter::InitializeCrashKeys();

  // MediaSession was implemented in M85, and requires both implementation and
  // client libraries to be at least that new. The version check has to be in
  // the browser process, but the command line flag is automatically propagated
  // to renderers.
  if (WebLayerFactoryImplAndroid::GetClientMajorVersion() < 85) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kDisableMediaSessionAPI);
  }
#endif

  return content::RESULT_CODE_NORMAL_EXIT;
}

void BrowserMainPartsImpl::PreMainMessageLoopStart() {
#if defined(USE_AURA) && defined(USE_X11)
  if (!features::IsUsingOzonePlatform())
    ui::TouchFactory::SetTouchDeviceListFromCommandLine();
#endif
}

int BrowserMainPartsImpl::PreEarlyInitialization() {
  browser_process_ = std::make_unique<BrowserProcess>(std::move(local_state_));

#if defined(USE_X11)
  if (!features::IsUsingOzonePlatform())
    ui::SetDefaultX11ErrorHandlers();
#endif
#if defined(USE_AURA) && (defined(OS_LINUX) || defined(OS_CHROMEOS))
  ui::InitializeInputMethodForTesting();
#endif
#if defined(OS_ANDROID)
  net::NetworkChangeNotifier::SetFactory(
      new net::NetworkChangeNotifierFactoryAndroid());
#endif

  translate::TranslateDownloadManager* download_manager =
      translate::TranslateDownloadManager::GetInstance();
  download_manager->set_url_loader_factory(
      BrowserProcess::GetInstance()->GetSharedURLLoaderFactory());
  download_manager->set_application_locale(i18n::GetApplicationLocale());

  return content::RESULT_CODE_NORMAL_EXIT;
}

void BrowserMainPartsImpl::PreMainMessageLoopRun() {
  FeatureListCreator::GetInstance()->PerformPreMainMessageLoopStartup();

  // It's necessary to have a complete dependency graph of
  // BrowserContextKeyedServices before calling out to the delegate (which
  // will potentially create a profile), so that a profile creation message is
  // properly dispatched to the factories that want to create their services
  // at profile creation time.
  EnsureBrowserContextKeyedServiceFactoriesBuilt();

  params_->delegate->PreMainMessageLoopRun();

  content::WebUIControllerFactory::RegisterFactory(
      WebUIControllerFactory::GetInstance());

  BrowserProcess::GetInstance()->PreMainMessageLoopRun();

  if (main_function_params_.ui_task) {
    std::move(*main_function_params_.ui_task).Run();
    delete main_function_params_.ui_task;
    run_message_loop_ = false;
  }

#if defined(OS_ANDROID)
  // On Android, retrieve the application start time from Java and record it. On
  // other platforms, the application start time was already recorded in the
  // constructor of ContentMainDelegateImpl.
  startup_metric_utils::RecordApplicationStartTime(GetApplicationStartTime());
#endif  // defined(OS_ANDROID)
  // Record the time at which the main message loop starts. Must be recorded
  // after application start time (see startup_metric_utils.h).
  startup_metric_utils::RecordBrowserMainMessageLoopStart(
      base::TimeTicks::Now(), /* is_first_run */ false);

#if defined(OS_ANDROID)
  memory_metrics_logger_ = std::make_unique<metrics::MemoryMetricsLogger>();

  // Set the global singleton app modal dialog factory.
  javascript_dialogs::AppModalDialogManager::GetInstance()
      ->SetNativeDialogFactory(base::BindRepeating(
          [](javascript_dialogs::AppModalDialogController* controller)
              -> javascript_dialogs::AppModalDialogView* {
            return new javascript_dialogs::AppModalDialogViewAndroid(
                base::android::AttachCurrentThread(), controller,
                controller->web_contents()->GetTopLevelNativeWindow());
          }));

  Java_MojoInterfaceRegistrar_registerMojoInterfaces(
      base::android::AttachCurrentThread());
#endif
}

bool BrowserMainPartsImpl::MainMessageLoopRun(int* result_code) {
  return !run_message_loop_;
}

void BrowserMainPartsImpl::PostMainMessageLoopRun() {
  params_->delegate->PostMainMessageLoopRun();
  browser_process_->StartTearDown();
}

void BrowserMainPartsImpl::PreDefaultMainMessageLoopRun(
    base::OnceClosure quit_closure) {
  // Wrap the method that stops the message loop so we can do other shutdown
  // cleanup inside content.
  params_->delegate->SetMainMessageLoopQuitClosure(
      base::BindOnce(StopMessageLoop, std::move(quit_closure)));
}

}  // namespace weblayer
