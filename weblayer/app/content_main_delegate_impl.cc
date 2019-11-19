// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/app/content_main_delegate_impl.h"

#include <iostream>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "media/base/media_switches.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "weblayer/browser/content_browser_client_impl.h"
#include "weblayer/common/content_client_impl.h"
#include "weblayer/common/weblayer_paths.h"
#include "weblayer/renderer/content_renderer_client_impl.h"
#include "weblayer/utility/content_utility_client_impl.h"

#if defined(OS_ANDROID)
#include "base/android/apk_assets.h"
#include "base/android/locale_utils.h"
#include "base/i18n/rtl.h"
#include "base/posix/global_descriptors.h"
#include "content/public/browser/android/compositor.h"
#include "ui/base/resource/resource_bundle_android.h"
#include "ui/base/ui_base_switches.h"
#include "weblayer/browser/android_descriptors.h"
#include "weblayer/common/crash_reporter/crash_keys.h"
#include "weblayer/common/crash_reporter/crash_reporter_client.h"
#endif

#if defined(OS_WIN)
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

// Disables each feature in |features_to_disable| unless it is already set in
// the command line.
void DisableFeaturesIfNotSet(
    const std::vector<base::Feature>& features_to_disable) {
  auto* cl = base::CommandLine::ForCurrentProcess();
  std::vector<std::string> enabled_features;
  std::string enabled_features_str =
      cl->GetSwitchValueASCII(switches::kEnableFeatures);
  for (const auto& f :
       base::FeatureList::SplitFeatureListString(enabled_features_str)) {
    enabled_features.emplace_back(f);
  }

  std::vector<std::string> disabled_features;
  std::string disabled_features_str =
      cl->GetSwitchValueASCII(switches::kDisableFeatures);
  for (const auto& f :
       base::FeatureList::SplitFeatureListString(disabled_features_str)) {
    disabled_features.emplace_back(f);
  }

  for (const auto& feature : features_to_disable) {
    if (!base::Contains(disabled_features, feature.name) &&
        !base::Contains(enabled_features, feature.name)) {
      disabled_features.push_back(feature.name);
    }
  }

  cl->AppendSwitchASCII(switches::kDisableFeatures,
                        base::JoinString(disabled_features, ","));
}

}  // namespace

ContentMainDelegateImpl::ContentMainDelegateImpl(MainParams params)
    : params_(std::move(params)) {}

ContentMainDelegateImpl::~ContentMainDelegateImpl() = default;

bool ContentMainDelegateImpl::BasicStartupComplete(int* exit_code) {
  int dummy;
  if (!exit_code)
    exit_code = &dummy;

  // Disable features which are not currently supported in WebLayer. This allows
  // sites to do feature detection, and prevents crashes in some not fully
  // implemented features.
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitch(switches::kDisableNotifications);
  cl->AppendSwitch(switches::kDisableSpeechSynthesisAPI);
  cl->AppendSwitch(switches::kDisableSpeechAPI);
  cl->AppendSwitch(switches::kDisablePermissionsAPI);
  cl->AppendSwitch(switches::kDisablePresentationAPI);
  cl->AppendSwitch(switches::kDisableRemotePlaybackAPI);
#if defined(OS_ANDROID)
  cl->AppendSwitch(switches::kDisableMediaSessionAPI);
#endif
  DisableFeaturesIfNotSet({
    ::features::kWebPayments, ::features::kWebAuth, ::features::kSmsReceiver,
        ::features::kWebXr,
#if defined(OS_ANDROID)
        media::kPictureInPictureAPI,
#endif
  });

#if defined(OS_ANDROID)
  content::Compositor::Initialize();
#endif

  InitLogging(&params_);

  content_client_ = std::make_unique<ContentClientImpl>();
  SetContentClient(content_client_.get());
  RegisterPathProvider();

  return false;
}

void ContentMainDelegateImpl::PreSandboxStartup() {
#if defined(ARCH_CPU_ARM_FAMILY) && (defined(OS_ANDROID) || defined(OS_LINUX))
  // Create an instance of the CPU class to parse /proc/cpuinfo and cache
  // cpu_brand info.
  base::CPU cpu_info;
#endif

  InitializeResourceBundle();

#if defined(OS_ANDROID)
  EnableCrashReporter(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType));
  SetWebLayerCrashKeys();
#endif
}

int ContentMainDelegateImpl::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  // For non-browser process, return and have the caller run the main loop.
  if (!process_type.empty())
    return -1;

#if !defined(OS_ANDROID)
  // On non-Android, we can return -1 and have the caller run BrowserMain()
  // normally.
  return -1;
#else
  // On Android, we defer to the system message loop when the stack unwinds.
  // So here we only create (and leak) a BrowserMainRunner. The shutdown
  // of BrowserMainRunner doesn't happen in Chrome Android and doesn't work
  // properly on Android at all.
  auto main_runner = content::BrowserMainRunner::Create();
  // In browser tests, the |main_function_params| contains a |ui_task| which
  // will execute the testing. The task will be executed synchronously inside
  // Initialize() so we don't depend on the BrowserMainRunner being Run().
  int initialize_exit_code = main_runner->Initialize(main_function_params);
  DCHECK_LT(initialize_exit_code, 0)
      << "BrowserMainRunner::Initialize failed in MainDelegate";
  ignore_result(main_runner.release());
  // Return 0 as BrowserMain() should not be called after this, bounce up to
  // the system message loop for ContentShell, and we're already done thanks
  // to the |ui_task| for browser tests.
  return 0;
#endif
}

void ContentMainDelegateImpl::InitializeResourceBundle() {
#if defined(OS_ANDROID)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  bool is_browser_process =
      command_line.GetSwitchValueASCII(switches::kProcessType).empty();
  if (is_browser_process) {
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

    constexpr char kWebLayerLocalePath[] =
        "assets/stored-locales/weblayer/en-US.pak";
    base::MemoryMappedFile::Region region;
    int fd = base::android::OpenApkAsset(kWebLayerLocalePath, &region);
    CHECK_GE(fd, 0) << "Could not find " << kWebLayerLocalePath << " in APK.";
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromFileRegion(
        base::File(fd), region, ui::SCALE_FACTOR_NONE);
    base::GlobalDescriptors::GetInstance()->Set(
        kWebLayerSecondaryLocalePakDescriptor, fd, region);
  } else {
    base::i18n::SetICUDefaultLocale(
        command_line.GetSwitchValueASCII(switches::kLang));

    auto* global_descriptors = base::GlobalDescriptors::GetInstance();
    int pak_fd = global_descriptors->Get(kWebLayerLocalePakDescriptor);
    base::MemoryMappedFile::Region pak_region =
        global_descriptors->GetRegion(kWebLayerLocalePakDescriptor);
    ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(base::File(pak_fd),
                                                            pak_region);

    std::pair<int, ui::ScaleFactor> extra_paks[] = {
        {kWebLayerMainPakDescriptor, ui::SCALE_FACTOR_NONE},
        {kWebLayerSecondaryLocalePakDescriptor, ui::SCALE_FACTOR_NONE},
        {kWebLayer100PercentPakDescriptor, ui::SCALE_FACTOR_100P}};

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

content::ContentUtilityClient*
ContentMainDelegateImpl::CreateContentUtilityClient() {
  utility_client_ = std::make_unique<ContentUtilityClientImpl>();
  return utility_client_.get();
}

}  // namespace weblayer
