// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_APP_CONTENT_MAIN_DELEGATE_IMPL_H_
#define WEBLAYER_APP_CONTENT_MAIN_DELEGATE_IMPL_H_

#include <memory>

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "content/public/app/content_main_delegate.h"
#include "weblayer/public/main.h"

namespace weblayer {
class ContentBrowserClientImpl;
class ContentClientImpl;
class ContentRendererClientImpl;

class ContentMainDelegateImpl : public content::ContentMainDelegate {
 public:
  explicit ContentMainDelegateImpl(MainParams params);

  ContentMainDelegateImpl(const ContentMainDelegateImpl&) = delete;
  ContentMainDelegateImpl& operator=(const ContentMainDelegateImpl&) = delete;

  ~ContentMainDelegateImpl() override;

  // ContentMainDelegate implementation:
  absl::optional<int> BasicStartupComplete() override;
  bool ShouldCreateFeatureList(InvokedIn invoked_in) override;
  bool ShouldInitializeMojo(InvokedIn invoked_in) override;
  variations::VariationsIdsProvider* CreateVariationsIdsProvider() override;
  void PreSandboxStartup() override;
  absl::optional<int> PostEarlyInitialization(InvokedIn invoked_in) override;
  absl::variant<int, content::MainFunctionParams> RunProcess(
      const std::string& process_type,
      content::MainFunctionParams main_function_params) override;
  content::ContentClient* CreateContentClient() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;

 private:
  void InitializeResourceBundle();

  MainParams params_;
  std::unique_ptr<ContentBrowserClientImpl> browser_client_;
  std::unique_ptr<ContentRendererClientImpl> renderer_client_;
  std::unique_ptr<ContentClientImpl> content_client_;
};

}  // namespace weblayer

#endif  // WEBLAYER_APP_CONTENT_MAIN_DELEGATE_IMPL_H_
