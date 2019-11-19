// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/shell/app/shell_main_params.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "net/base/filename_util.h"
#include "url/gurl.h"
#include "weblayer/public/main.h"
#include "weblayer/public/profile.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/shell/common/shell_switches.h"

#if defined(OS_WIN)
#include "base/base_paths_win.h"
#elif defined(OS_LINUX)
#include "base/nix/xdg_util.h"
#endif

namespace weblayer {

namespace {

GURL GetStartupURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kNoInitialNavigation))
    return GURL();

  const base::CommandLine::StringVector& args = command_line->GetArgs();

#if defined(OS_ANDROID)
  // Delay renderer creation on Android until surface is ready.
  return GURL();
#endif

  if (args.empty())
    return GURL("https://www.google.com/");

  GURL url(args[0]);
  if (url.is_valid() && url.has_scheme())
    return url;

  return net::FilePathToFileURL(
      base::MakeAbsoluteFilePath(base::FilePath(args[0])));
}

class MainDelegateImpl : public MainDelegate {
 public:
  void PreMainMessageLoopRun() override {
    InitializeProfiles();

    Shell::Initialize();

    Shell::CreateNewWindow(profile_.get(), GetStartupURL(), gfx::Size());
  }

  void SetMainMessageLoopQuitClosure(base::OnceClosure quit_closure) override {
    Shell::SetMainMessageLoopQuitClosure(std::move(quit_closure));
  }

 private:
  void InitializeProfiles() {
    base::FilePath path;

    base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    if (cmd_line->HasSwitch(switches::kWebLayerShellDataPath)) {
      path = cmd_line->GetSwitchValuePath(switches::kWebLayerShellDataPath);
      if (base::DirectoryExists(path) || base::CreateDirectory(path)) {
        // Profile needs an absolute path, which we would normally get via
        // PathService. In this case, manually ensure the path is absolute.
        if (!path.IsAbsolute())
          path = base::MakeAbsoluteFilePath(path);
      } else {
        LOG(ERROR) << "Unable to create data-path directory: " << path.value();
      }
    } else {
#if defined(OS_WIN)
      CHECK(base::PathService::Get(base::DIR_LOCAL_APP_DATA, &path));
      path = path.AppendASCII("web_shell");
#elif defined(OS_LINUX)
      std::unique_ptr<base::Environment> env(base::Environment::Create());
      base::FilePath config_dir(
          base::nix::GetXDGDirectory(env.get(), base::nix::kXdgConfigHomeEnvVar,
                                     base::nix::kDotConfigDir));
      path = config_dir.AppendASCII("web_shell");
#elif defined(OS_ANDROID)
      CHECK(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &path));
      path = path.AppendASCII("web_shell");
#else
      NOTIMPLEMENTED();
#endif
      if (!base::PathExists(path))
        base::CreateDirectory(path);
    }

    profile_ = Profile::Create(path);

    // TODO: create an incognito profile as well.
  }

  std::unique_ptr<Profile> profile_;
};

}  // namespace

MainParams CreateMainParams() {
  static const base::NoDestructor<MainDelegateImpl> weblayer_delegate;
  MainParams params;
  params.delegate = const_cast<MainDelegateImpl*>(&(*weblayer_delegate));

  base::PathService::Get(base::DIR_EXE, &params.log_filename);
  params.log_filename = params.log_filename.AppendASCII("weblayer_shell.log");

  params.pak_name = "weblayer.pak";

  return params;
}

}  //  namespace weblayer
