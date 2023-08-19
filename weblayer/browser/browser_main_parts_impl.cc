// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_main_parts_impl.h"

#include "base/base_switches.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/task/task_traits.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/switches.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/embedder/performance_manager_lifetime.h"
#include "components/prefs/pref_service.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/startup_metric_utils/common/startup_metric_utils.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "weblayer/browser/accept_languages_service_factory.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/cookie_settings_factory.h"
#include "weblayer/browser/feature_list_creator.h"
#include "weblayer/browser/heavy_ad_service_factory.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/i18n_util.h"
#include "weblayer/browser/no_state_prefetch/no_state_prefetch_link_manager_factory.h"
#include "weblayer/browser/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "weblayer/browser/origin_trials_factory.h"
#include "weblayer/browser/permissions/weblayer_permissions_client.h"
#include "weblayer/browser/stateful_ssl_host_state_delegate_factory.h"
#include "weblayer/browser/subresource_filter_profile_context_factory.h"
#include "weblayer/browser/translate_ranker_factory.h"
#include "weblayer/browser/web_data_service_factory.h"
#include "weblayer/browser/webui/web_ui_controller_factory.h"
#include "weblayer/grit/weblayer_resources.h"
#include "weblayer/public/main.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/command_line.h"
#include "components/crash/content/browser/child_exit_observer_android.h"
#include "components/crash/content/browser/child_process_crash_observer_android.h"
#include "components/crash/core/common/crash_key.h"
#include "components/javascript_dialogs/android/app_modal_dialog_view_android.h"  // nogncheck
#include "components/javascript_dialogs/app_modal_dialog_manager.h"  // nogncheck
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"
#include "components/variations/variations_ids_provider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#include "weblayer/browser/android/metrics/uma_utils.h"
#include "weblayer/browser/android/metrics/weblayer_metrics_service_client.h"
#include "weblayer/browser/java/jni/MojoInterfaceRegistrar_jni.h"
#include "weblayer/browser/media/local_presentation_manager_factory.h"
#include "weblayer/browser/media/media_router_factory.h"
#include "weblayer/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#include "weblayer/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "weblayer/browser/site_engagement/site_engagement_service_factory.h"
#include "weblayer/browser/webapps/weblayer_webapps_client.h"
#include "weblayer/browser/weblayer_factory_impl_android.h"
#include "weblayer/common/features.h"
#endif

// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if defined(USE_AURA) && (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
#include "ui/base/ime/init/input_method_initializer.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "weblayer/browser/captive_portal_service_factory.h"
#endif

namespace weblayer {

namespace {

// Indexes and publishes the subresource filter ruleset data from resources in
// the resource bundle.
void PublishSubresourceFilterRulesetFromResourceBundle() {
  // First obtain the version of the ruleset data from the manifest.
  std::string ruleset_manifest_string =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_SUBRESOURCE_FILTER_UNINDEXED_RULESET_MANIFEST_JSON);
  auto ruleset_manifest = base::JSONReader::Read(ruleset_manifest_string);
  DCHECK(ruleset_manifest);
  std::string* content_version =
      ruleset_manifest->GetDict().FindString("version");

  // Instruct the RulesetService to obtain the unindexed ruleset data from the
  // ResourceBundle and give it the version of that data.
  auto* ruleset_service =
      BrowserProcess::GetInstance()->subresource_filter_ruleset_service();
  subresource_filter::UnindexedRulesetInfo ruleset_info;
  ruleset_info.resource_id = IDR_SUBRESOURCE_FILTER_UNINDEXED_RULESET;
  ruleset_info.content_version = *content_version;
  ruleset_service->IndexAndStoreAndPublishRulesetIfNeeded(ruleset_info);
}

// Instantiates all weblayer KeyedService factories, which is
// especially important for services that should be created at profile
// creation time as compared to lazily on first access.
void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  CaptivePortalServiceFactory::GetInstance();
#endif
  HeavyAdServiceFactory::GetInstance();
  HostContentSettingsMapFactory::GetInstance();
  StatefulSSLHostStateDelegateFactory::GetInstance();
  CookieSettingsFactory::GetInstance();
  AcceptLanguagesServiceFactory::GetInstance();
  TranslateRankerFactory::GetInstance();
  NoStatePrefetchLinkManagerFactory::GetInstance();
  NoStatePrefetchManagerFactory::GetInstance();
  SubresourceFilterProfileContextFactory::GetInstance();
  OriginTrialsFactory::GetInstance();
#if BUILDFLAG(IS_ANDROID)
  SiteEngagementServiceFactory::GetInstance();
  SafeBrowsingMetricsCollectorFactory::GetInstance();
  SafeBrowsingNavigationObserverManagerFactory::GetInstance();
  if (MediaRouterFactory::IsFeatureEnabled()) {
    LocalPresentationManagerFactory::GetInstance();
    MediaRouterFactory::GetInstance();
  }
#endif
  WebDataServiceFactory::GetInstance();
}

void StopMessageLoop(base::OnceClosure quit_closure) {
  for (auto it = content::RenderProcessHost::AllHostsIterator(); !it.IsAtEnd();
       it.Advance()) {
    it.GetCurrentValue()->DisableRefCounts();
  }

  std::move(quit_closure).Run();
}

}  // namespace

BrowserMainPartsImpl::BrowserMainPartsImpl(
    MainParams* params,
    std::unique_ptr<PrefService> local_state)
    : params_(params), local_state_(std::move(local_state)) {}

BrowserMainPartsImpl::~BrowserMainPartsImpl() = default;

int BrowserMainPartsImpl::PreCreateThreads() {
  // Make sure permissions client has been set.
  WebLayerPermissionsClient::GetInstance();
#if BUILDFLAG(IS_ANDROID)
  // The ChildExitObserver needs to be created before any child process is
  // created because it needs to be notified during process creation.
  child_exit_observer_ = std::make_unique<crash_reporter::ChildExitObserver>();
  child_exit_observer_->RegisterClient(
      std::make_unique<crash_reporter::ChildProcessCrashObserver>());

  crash_reporter::InitializeCrashKeys();
  CHECK(metrics::SubprocessMetricsProvider::CreateInstance());

  // WebLayer initializes the MetricsService once consent is determined.
  // Determining consent is async and potentially slow. VariationsIdsProvider
  // is responsible for updating the X-Client-Data header.
  // SyntheticTrialsActiveGroupIdProvider is responsible for updating the
  // variations crash keys. To ensure the header and crash keys are always
  // provided, they are registered now.
  //
  // Chrome registers these providers from PreCreateThreads() as well.
  auto* synthetic_trial_registry = WebLayerMetricsServiceClient::GetInstance()
                                       ->GetMetricsService()
                                       ->GetSyntheticTrialRegistry();
  synthetic_trial_registry->AddSyntheticTrialObserver(
      variations::VariationsIdsProvider::GetInstance());
  synthetic_trial_registry->AddSyntheticTrialObserver(
      variations::SyntheticTrialsActiveGroupIdProvider::GetInstance());
#endif

  return content::RESULT_CODE_NORMAL_EXIT;
}

int BrowserMainPartsImpl::PreEarlyInitialization() {
  browser_process_ = std::make_unique<BrowserProcess>(std::move(local_state_));

// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if defined(USE_AURA) && (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  ui::InitializeInputMethodForTesting();
#endif
#if BUILDFLAG(IS_ANDROID)
  net::NetworkChangeNotifier::SetFactory(
      new net::NetworkChangeNotifierFactoryAndroid());

  WebLayerWebappsClient::Create();
#endif

  return content::RESULT_CODE_NORMAL_EXIT;
}

void BrowserMainPartsImpl::PostCreateThreads() {
  performance_manager_lifetime_ =
      std::make_unique<performance_manager::PerformanceManagerLifetime>(
          performance_manager::GraphFeatures::WithMinimal()
              // Reports performance-related UMA/UKM.
              .EnableMetricsCollector(),
          base::DoNothing());

  translate::TranslateDownloadManager* download_manager =
      translate::TranslateDownloadManager::GetInstance();
  download_manager->set_url_loader_factory(
      BrowserProcess::GetInstance()->GetSharedURLLoaderFactory());
  download_manager->set_application_locale(i18n::GetApplicationLocale());
}

int BrowserMainPartsImpl::PreMainMessageLoopRun() {
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

  // Publish the ruleset data. On the vast majority of runs this will
  // effectively be a no-op as the version of the data changes at most once per
  // release. Nonetheless, post it as a best-effort task to take it off the
  // critical path of startup. Note that best-effort tasks are guaranteed to
  // execute within a reasonable delay (assuming of course that the app isn't
  // shut down first).
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&PublishSubresourceFilterRulesetFromResourceBundle));

#if BUILDFLAG(IS_ANDROID)
  // On Android, retrieve the application start time from Java and record it. On
  // other platforms, the application start time was already recorded in the
  // constructor of ContentMainDelegateImpl.
  startup_metric_utils::GetCommon().RecordApplicationStartTime(
      GetApplicationStartTime());
#endif  // BUILDFLAG(IS_ANDROID)
  // Record the time at which the main message loop starts. Must be recorded
  // after application start time (see startup_metric_utils.h).
  startup_metric_utils::GetBrowser().RecordBrowserMainMessageLoopStart(
      base::TimeTicks::Now(), /* is_first_run */ false);

#if BUILDFLAG(IS_ANDROID)
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

  return content::RESULT_CODE_NORMAL_EXIT;
}

void BrowserMainPartsImpl::WillRunMainMessageLoop(
    std::unique_ptr<base::RunLoop>& run_loop) {
  // Wrap the method that stops the message loop so we can do other shutdown
  // cleanup inside content.
  params_->delegate->SetMainMessageLoopQuitClosure(
      base::BindOnce(StopMessageLoop, run_loop->QuitClosure()));
}

void BrowserMainPartsImpl::OnFirstIdle() {
  startup_metric_utils::GetBrowser().RecordBrowserMainLoopFirstIdle(
      base::TimeTicks::Now());
}

void BrowserMainPartsImpl::PostMainMessageLoopRun() {
  params_->delegate->PostMainMessageLoopRun();
  browser_process_->StartTearDown();

  performance_manager_lifetime_.reset();
}

}  // namespace weblayer
