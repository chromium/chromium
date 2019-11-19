// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_APP_CONTENT_MAIN_DELEGATE_IMPL_H_
#define WEBLAYER_APP_CONTENT_MAIN_DELEGATE_IMPL_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/app/content_main_delegate.h"
#include "weblayer/public/main.h"

namespace weblayer {
class ContentClientImpl;
class ContentBrowserClientImpl;
class ContentRendererClientImpl;
class ContentUtilityClientImpl;

class ContentMainDelegateImpl : public content::ContentMainDelegate {
 public:
  explicit ContentMainDelegateImpl(MainParams params);
  ~ContentMainDelegateImpl() override;

  // ContentMainDelegate implementation:
  bool BasicStartupComplete(int* exit_code) override;
  void PreSandboxStartup() override;
  int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;
  content::ContentUtilityClient* CreateContentUtilityClient() override;

 private:
  void InitializeResourceBundle();

  MainParams params_;
  std::unique_ptr<ContentBrowserClientImpl> browser_client_;
  std::unique_ptr<ContentRendererClientImpl> renderer_client_;
  std::unique_ptr<ContentUtilityClientImpl> utility_client_;
  std::unique_ptr<ContentClientImpl> content_client_;

  DISALLOW_COPY_AND_ASSIGN(ContentMainDelegateImpl);
};

}  // namespace weblayer

#endif  // WEBLAYER_APP_CONTENT_MAIN_DELEGATE_IMPL_H_
