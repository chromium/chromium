// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/app/content_main_delegate_impl.h"

#include <iostream>
#include <tuple>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/cpu.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/content_capture/common/content_capture_features.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/translate/core/common/translate_util.h"
#include "components/variations/variations_ids_provider.h"
#include "content/public/app/initialize_mojo_core.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/url_constants.h"
#include "media/base/media_switches.h"
#include "services/network/public/cpp/features.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "weblayer/browser/background_fetch/background_fetch_delegate_factory.h"
#include "weblayer/browser/content_browser_client_impl.h"
#include "weblayer/common/content_client_impl.h"
#include "weblayer/common/weblayer_paths.h"
#include "weblayer/public/common/switches.h"
#include "weblayer/renderer/content_renderer_client_impl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/apk_assets.h"
#include "base/android/build_info.h"
#include "base/android/bundle_utils.h"
#include "base/android/java_exception_reporter.h"
#include "base/android/locale_utils.h"
#include "base/i18n/rtl.h"
#include "base/posix/global_descriptors.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/viz/common/features.h"
#include "content/public/browser/android/compositor.h"
#include "ui/base/resource/resource_bundle_android.h"
#include "ui/base/ui_base_switches.h"
#include "weblayer/browser/android/application_info_helper.h"
#include "weblayer/browser/android/exception_filter.h"
#include "weblayer/browser/android_descriptors.h"
#include "weblayer/common/crash_reporter/crash_keys.h"
#include "weblayer/common/crash_reporter/crash_reporter_client.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <initguid.h>
#include "base/logging_win.h"
#endif

namespace weblayer {

namespace {

void InitLogging(MainParams* params) {
  if (params->log_filename.empty())
    return;

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file_path = params->log_filename.value().c_str();
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  logging::InitLogging(settings);
  logging::SetLogItems(true /* Process ID */, true /* Thread ID */,
                       true /* Timestamp */, false /* Tick count */);
}

// Enables each feature in |features_to_enable| unless it is already set in the
// command line, and similarly disables each feature in |features_to_disable|
// unless it is already set in the command line.
void ConfigureFeaturesIfNotSet(
    const std::vector<const base::Feature*>& features_to_enable,
    const std::vector<const base::Feature*>& features_to_disable) {
  auto* cl = base::CommandLine::ForCurrentProcess();
  std::vector<std::string> enabled_features;
  base::flat_set<std::string> feature_names_enabled_via_command_line;
  std::string enabled_features_str =
      cl->GetSwitchValueASCII(::switches::kEnableFeatures);
  for (const auto& f :
       base::FeatureList::SplitFeatureListString(enabled_features_str)) {
    enabled_features.emplace_back(f);

    // "<" is used as separator for field trial/groups.
    std::vector<base::StringPiece> parts = base::SplitStringPiece(
        f, "<", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    // Split with supplied params should always return at least one entry.
    DCHECK(!parts.empty());
    if (parts[0].length() > 0)
      feature_names_enabled_via_command_line.insert(std::string(parts[0]));
  }

  std::vector<std::string> disabled_features;
  std::string disabled_features_str =
      cl->GetSwitchValueASCII(::switches::kDisableFeatures);
  for (const auto& f :
       base::FeatureList::SplitFeatureListString(disabled_features_str)) {
    disabled_features.emplace_back(f);
  }

  for (const auto* feature : features_to_enable) {
    if (!base::Contains(disabled_features, feature->name) &&
        !base::Contains(feature_names_enabled_via_command_line,
                        feature->name)) {
      enabled_features.push_back(feature->name);
    }
  }
  cl->AppendSwitchASCII(::switches::kEnableFeatures,
                        base::JoinString(enabled_features, ","));

  for (const auto* feature : features_to_disable) {
    if (!base::Contains(disabled_features, feature->name) &&
        !base::Contains(feature_names_enabled_via_command_line,
                        feature->name)) {
      disabled_features.push_back(feature->name);
    }
  }
  cl->AppendSwitchASCII(::switches::kDisableFeatures,
                        base::JoinString(disabled_features, ","));
}

}  // namespace

ContentMainDelegateImpl::ContentMainDelegateImpl(MainParams params)
    : params_(std::move(params)) {
#if !BUILDFLAG(IS_ANDROID)
  // On non-Android, the application start time is recorded in this constructor,
  // which runs early during application lifetime. On Android, the application
  // start time is sampled when the Java code is entered, and it is retrieved
  // from C++ after initializing the JNI (see
  // BrowserMainPartsImpl::PreMainMessageLoopRun()).
  startup_metric_utils::RecordApplicationStartTime(base::TimeTicks::Now());
#endif
}

ContentMainDelegateImpl::~ContentMainDelegateImpl() = default;

absl::optional<int> ContentMainDelegateImpl::BasicStartupComplete() {
  // Disable features which are not currently supported in WebLayer. This allows
  // sites to do feature detection, and prevents crashes in some not fully
  // implemented features.
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  // TODO(crbug.com/1025610): make notifications work with WebLayer.
  // This also turns off Push messaging.
  cl->AppendSwitch(::switches::kDisableNotifications);

  std::vector<const base::Feature*> enabled_features = {
#if BUILDFLAG(IS_ANDROID)
    // Overlay promotion requires some guarantees we don't have on WebLayer
    // (e.g. ensuring fullscreen, no movement of the parent view). Given that
    // we're unsure about the benefits when used embedded in a parent app, we
    // will only promote to overlays if needed for secure videos.
    &media::kUseAndroidOverlayForSecureOnly,
#endif
  };

  std::vector<const base::Feature*> disabled_features = {
    // TODO(crbug.com/1313771): Support Digital Goods API.
    &::features::kDigitalGoodsApi,
    // TODO(crbug.com/1091212): make Notification triggers work with
    // WebLayer.
    &::features::kNotificationTriggers,
    // TODO(crbug.com/1091211): Support PeriodicBackgroundSync on WebLayer.
    &::features::kPeriodicBackgroundSync,
    // TODO(crbug.com/1174856): Support Portals.
    &blink::features::kPortals,
    // TODO(crbug.com/1144912): Support BackForwardCache on WebLayer.
    &::features::kBackForwardCache,
    // TODO(crbug.com/1247836): Enable TFLite/Optimization Guide on WebLayer.
    &translate::kTFLiteLanguageDetectionEnabled,
    // TODO(crbug.com/1338402): Add support for WebLayer. Disabling autofill is
    // not yet supported.
    &blink::features::kAnonymousIframeOriginTrial,

#if BUILDFLAG(IS_ANDROID)
    &::features::kDynamicColorGamut,
#else
    // WebOTP is supported only on Android in WebLayer.
    &::features::kWebOTP,
#endif
  };

#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_OREO) {
    enabled_features.push_back(
        &autofill::features::kAutofillExtractAllDatalists);
    enabled_features.push_back(
        &autofill::features::kAutofillSkipComparingInferredLabels);
  }

  if (GetApplicationMetadataAsBoolean(
          "org.chromium.weblayer.ENABLE_LOGGING_OF_JS_CONSOLE_MESSAGES",
          /*default_value=*/false)) {
    enabled_features.push_back(&features::kLogJsConsoleMessages);
  }
#endif

  ConfigureFeaturesIfNotSet(enabled_features, disabled_features);

  // TODO(crbug.com/1097105): Support Web GPU on WebLayer.
  blink::WebRuntimeFeatures::EnableWebGPU(false);

  // TODO(crbug.com/1338402): Add support for WebLayer. Disabling autofill is
  // not yet supported.
  blink::WebRuntimeFeatures::EnableAnonymousIframe(false);

#if BUILDFLAG(IS_ANDROID)
  content::Compositor::Initialize();
#endif

  InitLogging(&params_);

  RegisterPathProvider();

  return absl::nullopt;
}

bool ContentMainDelegateImpl::ShouldCreateFeatureList(InvokedIn invoked_in) {
#if BUILDFLAG(IS_ANDROID)
  // On android WebLayer is in charge of creating its own FeatureList in the
  // browser process.
  return absl::holds_alternative<InvokedInChildProcess>(invoked_in);
#else
  // TODO(weblayer-dev): Support feature lists on desktop.
  return true;
#endif
}

bool ContentMainDelegateImpl::ShouldInitializeMojo(InvokedIn invoked_in) {
  return ShouldCreateFeatureList(invoked_in);
}

variations::VariationsIdsProvider*
ContentMainDelegateImpl::CreateVariationsIdsProvider() {
  // As the embedder supplies the set of ids, the signed-in state does not make
  // sense and is ignored.
  return variations::VariationsIdsProvider::Create(
      variations::VariationsIdsProvider::Mode::kIgnoreSignedInState);
}

void ContentMainDelegateImpl::PreSandboxStartup() {
// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if defined(ARCH_CPU_ARM_FAMILY) &&                  \
    (BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || \
     BUILDFLAG(IS_CHROMEOS_LACROS))
  // Create an instance of the CPU class to parse /proc/cpuinfo and cache
  // cpu_brand info.
  base::CPU cpu_info;
#endif

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  const bool is_browser_process =
      command_line.GetSwitchValueASCII(::switches::kProcessType).empty();
  if (is_browser_process &&
      command_line.HasSwitch(switches::kWebLayerUserDataDir)) {
    base::FilePath path =
        command_line.GetSwitchValuePath(switches::kWebLayerUserDataDir);
    if (base::DirectoryExists(path) || base::CreateDirectory(path)) {
      // Profile needs an absolute path, which we would normally get via
      // PathService. In this case, manually ensure the path is absolute.
      if (!path.IsAbsolute())
        path = base::MakeAbsoluteFilePath(path);
    } else {
      LOG(ERROR) << "Unable to create data-path directory: " << path.value();
    }
    CHECK(base::PathService::OverrideAndCreateIfNeeded(
        DIR_USER_DATA, path, true /* is_absolute */, false /* create */));
  }

  InitializeResourceBundle();

#if BUILDFLAG(IS_ANDROID)
  EnableCrashReporter(
      command_line.GetSwitchValueASCII(::switches::kProcessType));
  if (is_browser_process) {
    base::android::SetJavaExceptionFilter(
        base::BindRepeating(&WebLayerJavaExceptionFilter));
  }
  SetWebLayerCrashKeys();
#endif
}

absl::optional<int> ContentMainDelegateImpl::PostEarlyInitialization(
    InvokedIn invoked_in) {
  if (absl::holds_alternative<InvokedInBrowserProcess>(invoked_in)) {
    browser_client_->CreateFeatureListAndFieldTrials();
  }
  if (!ShouldInitializeMojo(invoked_in)) {
    // Since we've told Content not to initialize Mojo on its own, we must do it
    // here manually.
    content::InitializeMojoCore();
  }
  return absl::nullopt;
}

absl::variant<int, content::MainFunctionParams>
ContentMainDelegateImpl::RunProcess(
    const std::string& process_type,
    content::MainFunctionParams main_function_params) {
  // For non-browser process, return and have the caller run the main loop.
  if (!process_type.empty())
    return std::move(main_function_params);

#if !BUILDFLAG(IS_ANDROID)
  // On non-Android, we can return |main_function_params| back and have the
  // caller run BrowserMain() normally.
  return std::move(main_function_params);
#else
  // On Android, we defer to the system message loop when the stack unwinds.
  // So here we only create (and leak) a BrowserMainRunner. The shutdown
  // of BrowserMainRunner doesn't happen in Chrome Android and doesn't work
  // properly on Android at all.
  auto main_runner = content::BrowserMainRunner::Create();
  // In browser tests, the |main_function_params| contains a |ui_task| which
  // will execute the testing. The task will be executed synchronously inside
  // Initialize() so we don't depend on the BrowserMainRunner being Run().
  int initialize_exit_code =
      main_runner->Initialize(std::move(main_function_params));
  DCHECK_LT(initialize_exit_code, 0)
      << "BrowserMainRunner::Initialize failed in MainDelegate";
  std::ignore = main_runner.release();
  // Return 0 as BrowserMain() should not be called after this, bounce up to
  // the system message loop for ContentShell, and we're already done thanks
  // to the |ui_task| for browser tests.
  return 0;
#endif
}

void ContentMainDelegateImpl::InitializeResourceBundle() {
#if BUILDFLAG(IS_ANDROID)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  bool is_browser_process =
      command_line.GetSwitchValueASCII(::switches::kProcessType).empty();
  if (is_browser_process) {
    // If we're not being loaded from a bundle, locales will be loaded from the
    // webview stored-locales directory. Otherwise, we are in Monochrome, and
    // we load both chrome and webview's locale assets.
    if (base::android::BundleUtils::IsBundle())
      ui::SetLoadSecondaryLocalePaks(true);
    else
      ui::SetLocalePaksStoredInApk(true);
    // Passing an empty |pref_locale| yields the system default locale.
    std::string locale = ui::ResourceBundle::InitSharedInstanceWithLocale(
        {} /*pref_locale*/, nullptr, ui::ResourceBundle::LOAD_COMMON_RESOURCES);

    if (locale.empty()) {
      LOG(WARNING) << "Failed to load locale .pak from apk.";
    }

    // Try to directly mmap the resources.pak from the apk. Fall back to load
    // from file, using PATH_SERVICE, otherwise.
    base::FilePath pak_file_path;
    base::PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &pak_file_path);
    pak_file_path = pak_file_path.AppendASCII("resources.pak");
    ui::LoadMainAndroidPackFile("assets/resources.pak", pak_file_path);

    // The English-only workaround is not needed for bundles, since bundles will
    // contain assets for all locales.
    if (!base::android::BundleUtils::IsBundle()) {
      constexpr char kWebLayerLocalePath[] =
          "assets/stored-locales/weblayer/en-US.pak";
      base::MemoryMappedFile::Region region;
      int fd = base::android::OpenApkAsset(kWebLayerLocalePath, &region);
      CHECK_GE(fd, 0) << "Could not find " << kWebLayerLocalePath << " in APK.";
      ui::ResourceBundle::GetSharedInstance()
          .LoadSecondaryLocaleDataWithPakFileRegion(base::File(fd), region);
      base::GlobalDescriptors::GetInstance()->Set(
          kWebLayerSecondaryLocalePakDescriptor, fd, region);
    }
  } else {
    base::i18n::SetICUDefaultLocale(
        command_line.GetSwitchValueASCII(::switches::kLang));

    auto* global_descriptors = base::GlobalDescriptors::GetInstance();
    int pak_fd = global_descriptors->Get(kWebLayerLocalePakDescriptor);
    base::MemoryMappedFile::Region pak_region =
        global_descriptors->GetRegion(kWebLayerLocalePakDescriptor);
    ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(base::File(pak_fd),
                                                            pak_region);

    pak_fd = global_descriptors->Get(kWebLayerSecondaryLocalePakDescriptor);
    pak_region =
        global_descriptors->GetRegion(kWebLayerSecondaryLocalePakDescriptor);
    ui::ResourceBundle::GetSharedInstance()
        .LoadSecondaryLocaleDataWithPakFileRegion(base::File(pak_fd),
                                                  pak_region);

    std::vector<std::pair<int, ui::ResourceScaleFactor>> extra_paks = {
        {kWebLayerMainPakDescriptor, ui::kScaleFactorNone},
        {kWebLayer100PercentPakDescriptor, ui::k100Percent}};

    for (const auto& pak_info : extra_paks) {
      pak_fd = global_descriptors->Get(pak_info.first);
      pak_region = global_descriptors->GetRegion(pak_info.first);
      ui::ResourceBundle::GetSharedInstance().AddDataPackFromFileRegion(
          base::File(pak_fd), pak_region, pak_info.second);
    }
  }
#else
  base::FilePath pak_file;
  bool r = base::PathService::Get(base::DIR_ASSETS, &pak_file);
  DCHECK(r);
  pak_file = pak_file.AppendASCII(params_.pak_name);
  ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);
#endif
}

content::ContentClient* ContentMainDelegateImpl::CreateContentClient() {
  content_client_ = std::make_unique<ContentClientImpl>();
  return content_client_.get();
}

content::ContentBrowserClient*
ContentMainDelegateImpl::CreateContentBrowserClient() {
  browser_client_ = std::make_unique<ContentBrowserClientImpl>(&params_);
  return browser_client_.get();
}

content::ContentRendererClient*
ContentMainDelegateImpl::CreateContentRendererClient() {
  renderer_client_ = std::make_unique<ContentRendererClientImpl>();
  return renderer_client_.get();
}

}  // namespace weblayer
