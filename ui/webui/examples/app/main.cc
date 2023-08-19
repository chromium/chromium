// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/public/app/content_main.h"
#include "ui/webui/examples/app/main_delegate.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "content/public/app/sandbox_helper_win.h"
#include "sandbox/win/src/sandbox_types.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
#include "base/files/file_path.h"
#include "sandbox/mac/seatbelt_exec.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
int wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  base::CommandLine::Init(0, nullptr);
  sandbox::SandboxInterfaceInfo sandbox_info{};
  content::InitializeSandboxInfo(&sandbox_info);

  webui_examples::MainDelegate delegate;
  content::ContentMainParams params(&delegate);
  params.instance = instance;
  params.sandbox_info = &sandbox_info;

  return content::ContentMain(std::move(params));
}
#elif BUILDFLAG(IS_MAC)
int main(int argc, const char** argv) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  sandbox::SeatbeltExecServer::CreateFromArgumentsResult seatbelt =
      sandbox::SeatbeltExecServer::CreateFromArguments(
          command_line->GetProgram().value().c_str(), argc,
          const_cast<char**>(argv));
  if (seatbelt.sandbox_required) {
    CHECK(seatbelt.server->InitializeSandbox());
  }

  webui_examples::MainDelegate delegate;
  content::ContentMainParams params(&delegate);
  return content::ContentMain(std::move(params));
}
#else
int main(int argc, const char** argv) {
  base::CommandLine::Init(argc, argv);

  webui_examples::MainDelegate delegate;
  content::ContentMainParams params(&delegate);
  return content::ContentMain(std::move(params));
}
#endif  // BUILDFLAG(IS_WIN)
