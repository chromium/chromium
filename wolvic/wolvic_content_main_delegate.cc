// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/wolvic_content_main_delegate.h"

#include <iostream>
#include <tuple>
#include <utility>

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/current_process.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/crash/core/common/crash_key.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/network_hints/browser/simple_network_hints_handler_impl.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service_factory.h"
#include "components/variations/android/variations_seed_bridge.h"
#include "components/variations/platform_field_trials.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/safe_seed_manager.h"
#include "components/variations/service/variations_field_trial_creator.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/service/variations_service_client.h"
#include "components/variations/variations_safe_seed_store_local_state.h"
#include "components/variations/variations_switches.h"
#include "content/app/android/content_main_android.h"
#include "content/common/content_constants_internal.h"
#include "content/public/app/initialize_mojo_core.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switch_dependent_feature_overrides.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "content/public/gpu/content_gpu_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/utility/content_utility_client.h"
#include "content/shell/android/shell_descriptors.h"
#include "content/shell/browser/shell_paths.h"
#include "ipc/ipc_buildflags.h"
#include "net/cookies/cookie_monster.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "wolvic/browser/metrics/wolvic_enabled_state_provider.h"
#include "wolvic/wolvic_browser_context.h"
#include "wolvic/wolvic_content_browser_client.h"
#include "wolvic/wolvic_content_client.h"
#include "wolvic/renderer/wolvic_content_renderer_client.h"

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
#define IPC_MESSAGE_MACROS_LOG_ENABLED
#include "content/public/common/content_ipc_logging.h"
#define IPC_LOG_TABLE_ADD_ENTRY(msg_id, logger) \
  content::RegisterIPCLogger(msg_id, logger)
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/apk_assets.h"
#include "base/posix/global_descriptors.h"
#include "content/public/browser/android/compositor.h"
#endif

namespace {

void InitLogging(const base::CommandLine& command_line) {
  base::FilePath log_filename =
      command_line.GetSwitchValuePath(switches::kLogFile);
  if (log_filename.empty()) {
    base::PathService::Get(base::DIR_EXE, &log_filename);
    log_filename = log_filename.AppendASCII("wolvic_content.log");
  }

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file_path = log_filename.value().c_str();
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  logging::InitLogging(settings);
  logging::SetLogItems(true /* Process ID */, true /* Thread ID */,
                       true /* Timestamp */, false /* Tick count */);
}

}  // namespace

namespace wolvic {

// TODO(crbug/1219642): Consider not needing VariationsServiceClient just to use
// VariationsFieldTrialCreator.
class ShellVariationsServiceClient
    : public variations::VariationsServiceClient {
 public:
  ShellVariationsServiceClient() = default;
  ~ShellVariationsServiceClient() override = default;

  // variations::VariationsServiceClient:
  base::Version GetVersionForSimulation() override { return base::Version(); }
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return nullptr;
  }
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override {
    return nullptr;
  }
  version_info::Channel GetChannel() override {
    return version_info::Channel::UNKNOWN;
  }
  bool OverridesRestrictParameter(std::string* parameter) override {
    return false;
  }
  bool IsEnterprise() override { return false; }
  // Profiles aren't supported, so nothing to do here.
  void RemoveGoogleGroupsFromPrefsForDeletedProfiles(
      PrefService* local_state) override {}
};

// Returns the full user agent string for the content shell.
std::string GetShellFullUserAgent() {
  std::string product = "Chrome/Wolvic";
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUseMobileUserAgent)) {
    product += " Mobile";
  }
  return content::BuildUserAgentFromProduct(product);
}

// Returns the reduced user agent string for the content shell.
std::string GetShellReducedUserAgent() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return content::GetReducedUserAgent(
      command_line->HasSwitch(switches::kUseMobileUserAgent), "1.0");
}

void BindNetworkHintsHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<network_hints::mojom::NetworkHintsHandler> receiver) {
  DCHECK(frame_host);
  network_hints::SimpleNetworkHintsHandlerImpl::Create(frame_host,
                                                       std::move(receiver));
}

base::flat_set<url::Origin> GetIsolatedContextOriginSetFromFlag() {
  return {};
}

WolvicContentMainDelegate::WolvicContentMainDelegate()
    : session_settings_(std::make_unique<SessionSettings>()) {}

WolvicContentMainDelegate::~WolvicContentMainDelegate() {}

// static
WolvicContentMainDelegate* WolvicContentMainDelegate::Get() {
  return static_cast<WolvicContentMainDelegate*>(
      content::GetContentMainDelegateForTesting());
}

absl::optional<int> WolvicContentMainDelegate::BasicStartupComplete() {
  content::Compositor::Initialize();

  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  InitLogging(command_line);
  LOG(INFO) << "Command line: " << command_line.GetCommandLineString();
  content::RegisterShellPathProvider();

  return absl::nullopt;
}

bool WolvicContentMainDelegate::ShouldCreateFeatureList(InvokedIn invoked_in) {
  return absl::holds_alternative<InvokedInChildProcess>(invoked_in);
}

bool WolvicContentMainDelegate::ShouldInitializeMojo(InvokedIn invoked_in) {
  return ShouldCreateFeatureList(invoked_in);
}

void WolvicContentMainDelegate::PreSandboxStartup() {
#if defined(ARCH_CPU_ARM_FAMILY) && \
    (BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
  // Create an instance of the CPU class to parse /proc/cpuinfo and cache
  // cpu_brand info.
  base::CPU cpu_info;
#endif
  InitializeResourceBundle();
}

absl::variant<int, content::MainFunctionParams>
WolvicContentMainDelegate::RunProcess(
    const std::string& process_type,
    content::MainFunctionParams main_function_params) {
  // For non-browser process, return and have the caller run the main loop.
  if (!process_type.empty()) {
    return std::move(main_function_params);
  }

  base::CurrentProcess::GetInstance().SetProcessType(
      base::CurrentProcessType::PROCESS_BROWSER);
  base::trace_event::TraceLog::GetInstance()->SetProcessSortIndex(
      content::kTraceEventBrowserProcessSortIndex);
  //
  // On Android, we defer to the system message loop when the stack unwinds.
  // So here we only create (and leak) a BrowserMainRunner. The shutdown
  // of BrowserMainRunner doesn't happen in Chrome Android and doesn't work
  // properly on Android at all.
  std::unique_ptr<content::BrowserMainRunner> main_runner =
      content::BrowserMainRunner::Create();
  // In browser tests, the |main_function_params| contains a |ui_task| which
  // will execute the testing. The task will be executed synchronously inside
  // Initialize() so we don't depend on the BrowserMainRunner being Run().
  int initialize_exit_code =
      main_runner->Initialize(std::move(main_function_params));
  DCHECK_LT(initialize_exit_code, 0)
      << "BrowserMainRunner::Initialize failed in WolvicContentMainDelegate";
  std::ignore = main_runner.release();
  // Return 0 as BrowserMain() should not be called after this, bounce up to
  // the system message loop for ContentShell, and we're already done thanks
  // to the |ui_task| for browser tests.
  return 0;
}

void WolvicContentMainDelegate::InitializeResourceBundle() {
  // TODO: Initialize pak file
  // On Android, the renderer runs with a different UID and can never access
  // the file system. Use the file descriptor passed in at launch time.

  auto* global_descriptors = base::GlobalDescriptors::GetInstance();
  int pak_fd = global_descriptors->MaybeGet(kShellPakDescriptor);
  base::MemoryMappedFile::Region pak_region;
  if (pak_fd >= 0) {
    pak_region = global_descriptors->GetRegion(kShellPakDescriptor);
  } else {
    pak_fd =
        base::android::OpenApkAsset("assets/wolvic.pak", &pak_region);
    // Loaded from disk for browsertests.
    if (pak_fd < 0) {
      base::FilePath pak_file;
      bool r = base::PathService::Get(base::DIR_ANDROID_APP_DATA, &pak_file);
      DCHECK(r);
      pak_file = pak_file.Append(FILE_PATH_LITERAL("paks"));
      pak_file = pak_file.Append(FILE_PATH_LITERAL("wolvic.pak"));
      int flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
      pak_fd = base::File(pak_file, flags).TakePlatformFile();
      pak_region = base::MemoryMappedFile::Region::kWholeFile;
    }
    global_descriptors->Set(kShellPakDescriptor, pak_fd, pak_region);
  }
  DCHECK_GE(pak_fd, 0);
  // TODO(crbug.com/330930): A better way to prevent fdsan error from a double
  // close is to refactor GlobalDescriptors.{Get,MaybeGet} to return
  // "const base::File&" rather than fd itself.
  base::File android_pak_file(pak_fd);
  ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(
      android_pak_file.Duplicate(), pak_region);
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromFileRegion(
      std::move(android_pak_file), pak_region, ui::k100Percent);
}

absl::optional<int> WolvicContentMainDelegate::PreBrowserMain() {
  absl::optional<int> exit_code =
      content::ContentMainDelegate::PreBrowserMain();
  if (exit_code.has_value()) {
    return exit_code;
  }

  return absl::nullopt;
}

absl::optional<int> WolvicContentMainDelegate::PostEarlyInitialization(
    InvokedIn invoked_in) {
  if (!ShouldCreateFeatureList(invoked_in)) {
    // Apply field trial testing configuration since content did not.
    CreateFeatureListAndFieldTrials();
  }
  if (!ShouldInitializeMojo(invoked_in)) {
    content::InitializeMojoCore();
  }
  return absl::nullopt;
}

content::ContentClient* WolvicContentMainDelegate::CreateContentClient() {
  content_client_ = std::make_unique<WolvicContentClient>();
  return content_client_.get();
}

content::ContentBrowserClient*
WolvicContentMainDelegate::CreateContentBrowserClient() {
  browser_client_ = std::make_unique<WolvicContentBrowserClient>();
  return browser_client_.get();
}

content::ContentGpuClient* WolvicContentMainDelegate::CreateContentGpuClient() {
  gpu_client_ = std::make_unique<content::ContentGpuClient>();
  return gpu_client_.get();
}

content::ContentRendererClient*
WolvicContentMainDelegate::CreateContentRendererClient() {
  renderer_client_ = std::make_unique<WolvicContentRendererClient>();
  return renderer_client_.get();
}

content::ContentUtilityClient*
WolvicContentMainDelegate::CreateContentUtilityClient() {
  utility_client_ = std::make_unique<content::ContentUtilityClient>();
  return utility_client_.get();
}

void WolvicContentMainDelegate::CreateFeatureListAndFieldTrials() {
  local_state_ = CreateLocalState();
  SetUpFieldTrials();
  // Schedule a Local State write since the above function resulted in some
  // prefs being updated.
  local_state_->CommitPendingWrite();
}

std::unique_ptr<PrefService> WolvicContentMainDelegate::CreateLocalState() {
  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();

  metrics::MetricsService::RegisterPrefs(pref_registry.get());
  variations::VariationsService::RegisterPrefs(pref_registry.get());

  base::FilePath path;
  CHECK(base::PathService::Get(content::SHELL_DIR_USER_DATA, &path));
  path = path.AppendASCII("Local State");

  PrefServiceFactory pref_service_factory;
  pref_service_factory.set_user_prefs(
      base::MakeRefCounted<JsonPrefStore>(path));

  return pref_service_factory.Create(pref_registry);
}

void WolvicContentMainDelegate::SetUpFieldTrials() {
  WolvicEnabledStateProvider enabled_state_provider;
  base::FilePath path;
  base::PathService::Get(content::SHELL_DIR_USER_DATA, &path);
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager =
      metrics::MetricsStateManager::Create(
          local_state_.get(), &enabled_state_provider, std::wstring(),
          path.AppendASCII("Local State"), metrics::StartupVisibility::kUnknown,
          {
              .force_benchmarking_mode = false,
          });
  metrics_state_manager->InstantiateFieldTrialList();

  std::vector<std::string> variation_ids;
  auto feature_list = std::make_unique<base::FeatureList>();

  std::unique_ptr<variations::SeedResponse> initial_seed;
#if BUILDFLAG(IS_ANDROID)
  if (!local_state_->HasPrefPath(variations::prefs::kVariationsSeedSignature)) {
    DVLOG(1) << "Importing first run seed from Java preferences.";
    initial_seed = variations::android::GetVariationsFirstRunSeed();
  }
#endif

  ShellVariationsServiceClient variations_service_client;
  variations::VariationsFieldTrialCreator field_trial_creator(
      &variations_service_client,
      std::make_unique<variations::VariationsSeedStore>(
          local_state_.get(), std::move(initial_seed),
          /*signature_verification_enabled=*/true,
          std::make_unique<variations::VariationsSafeSeedStoreLocalState>(
              local_state_.get())),
      variations::UIStringOverrider(),
      /*limited_entropy_synthetic_trial=*/nullptr);

  variations::SafeSeedManager safe_seed_manager(local_state_.get());

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  // Since this is a test-only code path, some arguments to SetUpFieldTrials are
  // null.
  // TODO(crbug/1248066): Consider passing a low entropy source.
  variations::PlatformFieldTrials platform_field_trials;
  variations::SyntheticTrialRegistry synthetic_trial_registry;
  field_trial_creator.SetUpFieldTrials(
      variation_ids,
      command_line->GetSwitchValueASCII(
          variations::switches::kForceVariationIds),
      content::GetSwitchDependentFeatureOverrides(*command_line),
      std::move(feature_list), metrics_state_manager.get(),
      &synthetic_trial_registry, &platform_field_trials, &safe_seed_manager,
      /*add_entropy_source_to_variations_ids=*/false);
}

}  // namespace wolvic
