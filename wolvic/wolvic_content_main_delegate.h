// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_WOLVIC_CONTENT_MAIN_DELEGATE_H_
#define WOLVIC_WOLVIC_CONTENT_MAIN_DELEGATE_H_

#include <memory>

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/app/content_main_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/shell_paths.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace content {
class WebContents;
class WolvicBrowserContext;
class WolvicContentBrowserClient;
class WolvicContentClient;
class WolvicContentMainDelegate;

class WolvicContentMainDelegate : public ContentMainDelegate {
 public:
  explicit WolvicContentMainDelegate();

  WolvicContentMainDelegate(const WolvicContentMainDelegate&) = delete;
  WolvicContentMainDelegate& operator=(const WolvicContentMainDelegate&) =
      delete;

  ~WolvicContentMainDelegate() override;

  static WolvicContentMainDelegate* Get();

  // ContentMainDelegate implementation:
  absl::optional<int> BasicStartupComplete() override;
  bool ShouldCreateFeatureList(InvokedIn invoked_in) override;
  bool ShouldInitializeMojo(InvokedIn invoked_in) override;
  void PreSandboxStartup() override;
  absl::variant<int, MainFunctionParams> RunProcess(
      const std::string& process_type,
      MainFunctionParams main_function_params) override;
  absl::optional<int> PreBrowserMain() override;
  absl::optional<int> PostEarlyInitialization(InvokedIn invoked_in) override;
  ContentClient* CreateContentClient() override;
  ContentBrowserClient* CreateContentBrowserClient() override;
  ContentGpuClient* CreateContentGpuClient() override;
  ContentRendererClient* CreateContentRendererClient() override;
  ContentUtilityClient* CreateContentUtilityClient() override;

  static void InitializeResourceBundle();

  WolvicBrowserContext* browser_context();

  PrefService* GetPrefs() { return local_state_.get(); }

 protected:
  friend base::android::ScopedJavaLocalRef<jobject> JNI_Tab_CreateWebContents(
      JNIEnv* env);

  void CreateFeatureListAndFieldTrials();
  std::unique_ptr<PrefService> CreateLocalState();
  void SetUpFieldTrials();

  std::unique_ptr<PrefService> local_state_;
  std::unique_ptr<WolvicContentBrowserClient> browser_client_;
  std::unique_ptr<ContentGpuClient> gpu_client_;
  std::unique_ptr<ContentRendererClient> renderer_client_;
  std::unique_ptr<ContentUtilityClient> utility_client_;
  std::unique_ptr<WolvicContentClient> content_client_;
};

}  // namespace content

#endif  //  WOLVIC_WOLVIC_CONTENT_MAIN_DELEGATE_H_
