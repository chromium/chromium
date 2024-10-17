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
#include "wolvic/browser/session_settings.h"

class PrefService;

namespace content {
class WebContents;
}

namespace wolvic {

class WolvicContentBrowserClient;
class WolvicContentClient;
class WolvicContentMainDelegate;

class WolvicContentMainDelegate : public content::ContentMainDelegate {
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
  absl::variant<int, content::MainFunctionParams> RunProcess(
      const std::string& process_type,
      content::MainFunctionParams main_function_params) override;
  absl::optional<int> PreBrowserMain() override;
  absl::optional<int> PostEarlyInitialization(InvokedIn invoked_in) override;
  content::ContentClient* CreateContentClient() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentGpuClient* CreateContentGpuClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;
  content::ContentUtilityClient* CreateContentUtilityClient() override;

  static void InitializeResourceBundle();

  PrefService* GetPrefs() { return local_state_.get(); }

 protected:
  friend base::android::ScopedJavaLocalRef<jobject> JNI_Tab_CreateWebContents(
      JNIEnv* env);

  void CreateFeatureListAndFieldTrials();
  std::unique_ptr<PrefService> CreateLocalState();
  void SetUpFieldTrials();

  std::unique_ptr<SessionSettings> session_settings_;
  std::unique_ptr<PrefService> local_state_;
  std::unique_ptr<WolvicContentBrowserClient> browser_client_;
  std::unique_ptr<content::ContentGpuClient> gpu_client_;
  std::unique_ptr<content::ContentRendererClient> renderer_client_;
  std::unique_ptr<content::ContentUtilityClient> utility_client_;
  std::unique_ptr<WolvicContentClient> content_client_;
};

}  // namespace wolvic

#endif  //  WOLVIC_WOLVIC_CONTENT_MAIN_DELEGATE_H_
