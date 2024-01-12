// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/app/main_delegate.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/examples/browser/content_browser_client.h"
#include "ui/webui/examples/common/content_client.h"
#include "ui/webui/examples/renderer/content_renderer_client.h"

#if BUILDFLAG(IS_MAC)
#include "ui/webui/examples/app/mac_init.h"
#endif

namespace webui_examples {

MainDelegate::MainDelegate() = default;

MainDelegate::~MainDelegate() = default;

std::optional<int> MainDelegate::BasicStartupComplete() {
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  CHECK(logging::InitLogging(settings));

  content_client_ = std::make_unique<ContentClient>();
  content::SetContentClient(content_client_.get());
  return std::nullopt;
}

void MainDelegate::PreSandboxStartup() {
  base::FilePath pak_file;
  bool res = base::PathService::Get(base::DIR_ASSETS, &pak_file);
  CHECK(res);
  pak_file = pak_file.Append(FILE_PATH_LITERAL("webui_examples.pak"));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);
}

content::ContentBrowserClient* MainDelegate::CreateContentBrowserClient() {
  content_browser_client_ = std::make_unique<ContentBrowserClient>();
  return content_browser_client_.get();
}

std::optional<int> MainDelegate::PreBrowserMain() {
#if BUILDFLAG(IS_MAC)
  MacPreBrowserMain();
#endif
  return content::ContentMainDelegate::PreBrowserMain();
}

content::ContentRendererClient* MainDelegate::CreateContentRendererClient() {
  content_renderer_client_ = std::make_unique<ContentRendererClient>();
  return content_renderer_client_.get();
}

}  // namespace webui_examples
