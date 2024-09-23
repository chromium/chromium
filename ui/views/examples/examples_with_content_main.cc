// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_provider_manager.h"
#include "ui/views/examples/examples_color_mixer.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/examples/examples_window_with_content.h"
#include "ui/views_content_client/views_content_client.h"

#if BUILDFLAG(IS_MAC)
#include "sandbox/mac/seatbelt_exec.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#include "content/public/app/sandbox_helper_win.h"
#include "sandbox/win/src/sandbox_types.h"
#endif

namespace {

void OnResourcesLoaded() {
  base::FilePath views_examples_resources_pak_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS,
                               &views_examples_resources_pak_path));
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      views_examples_resources_pak_path.AppendASCII(
          "views_examples_resources.pak"),
      ui::k100Percent);
}

void ShowContentExampleWindow(ui::ViewsContentClient* views_content_client,
                              content::BrowserContext* browser_context,
                              gfx::NativeWindow window_context) {
  views::examples::ShowExamplesWindowWithContent(
      std::move(views_content_client->quit_closure()), browser_context,
      window_context);

  // These lines serve no purpose other than to introduce an explicit content
  // dependency. If the main executable doesn't have this dependency, the linker
  // has more flexibility to reorder library dependencies in a shared component
  // build. On linux, this can cause libc to appear before libcontent in the
  // dlsym search path, which breaks (usually valid) assumptions made in
  // sandbox::InitLibcUrandomOverrides(). See http://crbug.com/374712.
  if (!browser_context) {
    browser_context->SaveSessionState();
    NOTREACHED();
  }
}

}  // namespace

#if BUILDFLAG(IS_WIN)
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  base::CommandLine::Init(0, nullptr);
  sandbox::SandboxInterfaceInfo sandbox_info = {nullptr};
  content::InitializeSandboxInfo(&sandbox_info);
  ui::ViewsContentClient views_content_client(instance, &sandbox_info);
#else
int main(int argc, const char** argv) {
  base::CommandLine::Init(argc, argv);

  ui::ColorProviderManager::Get().AppendColorProviderInitializer(
      base::BindRepeating(&views::examples::AddExamplesColorMixers));

  ui::ViewsContentClient views_content_client(argc, argv);
#endif

  if (views::examples::CheckCommandLineUsage())
    return 0;

#if BUILDFLAG(IS_MAC)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // ViewsContentClient expects a const char** argv and
  // CreateFromArgumentsResult expects a regular char** argv. Given this is a
  // test program, a refactor from either end didn't seem worth it. As a result,
  // use a const_cast instead.
  sandbox::SeatbeltExecServer::CreateFromArgumentsResult seatbelt =
      sandbox::SeatbeltExecServer::CreateFromArguments(
          command_line->GetProgram().value().c_str(), argc,
          const_cast<char**>(argv));
  if (seatbelt.sandbox_required)
    CHECK(seatbelt.server->InitializeSandbox());
#endif

  views_content_client.set_on_resources_loaded_callback(
      base::BindOnce(&OnResourcesLoaded));
  views_content_client.set_on_pre_main_message_loop_run_callback(base::BindOnce(
      &ShowContentExampleWindow, base::Unretained(&views_content_client)));
  return views_content_client.RunMain();
}
