// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_AUTOFILL_ASSISTANT_WEBLAYER_DEPENDENCIES_H_
#define WEBLAYER_BROWSER_AUTOFILL_ASSISTANT_WEBLAYER_DEPENDENCIES_H_

#include "base/android/scoped_java_ref.h"
#include "base/strings/string_piece.h"
#include "components/autofill_assistant/browser/android/dependencies_android.h"
#include "components/autofill_assistant/browser/assistant_field_trial_util.h"
#include "components/autofill_assistant/browser/common_dependencies.h"
#include "components/autofill_assistant/browser/platform_dependencies.h"
#include "components/autofill_assistant/content/browser/annotate_dom_model_service.h"
#include "components/metrics/metrics_service_accessor.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/security_state/core/security_state.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

namespace consent_auditor {
class ConsentAuditor;
}  // namespace consent_auditor

namespace weblayer {

// Interface for platform delegates that provide platform-dependent features
// and dependencies to the starter.
// TODO(b/201964911): rename to make it consistent with the other dependencies
// classes.
class WebLayerDependencies : public ::autofill_assistant::DependenciesAndroid,
                             public ::autofill_assistant::CommonDependencies,
                             public ::autofill_assistant::PlatformDependencies {
 public:
  WebLayerDependencies(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jstatic_dependencies);

  // DependenciesAndroid:
  const ::autofill_assistant::CommonDependencies* GetCommonDependencies()
      const override;
  const ::autofill_assistant::PlatformDependencies* GetPlatformDependencies()
      const override;

  // CommonDependencies:
  std::unique_ptr<::autofill_assistant::AssistantFieldTrialUtil>
  CreateFieldTrialUtil() const override;
  autofill::PersonalDataManager* GetPersonalDataManager() const override;
  password_manager::PasswordManagerClient* GetPasswordManagerClient(
      content::WebContents* web_contents) const override;
  std::string GetLocale() const override;
  std::string GetLatestCountryCode() const override;
  std::string GetStoredPermanentCountryCode() const override;
  std::string GetSignedInEmail() const override;
  PrefService* GetPrefs() const override;
  security_state::SecurityLevel GetSecurityLevel(
      content::WebContents* web_contents) const override;
  bool IsSupervisedUser() const override;
  ::autofill_assistant::AnnotateDomModelService*
  GetOrCreateAnnotateDomModelService() const override;
  bool IsWebLayer() const override;
  signin::IdentityManager* GetIdentityManager() const override;
  consent_auditor::ConsentAuditor* GetConsentAuditor() const override;
  version_info::Channel GetChannel() const override;

  // Overrides PlatformDependencies
  bool IsCustomTab(const content::WebContents& web_contents) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_AUTOFILL_ASSISTANT_WEBLAYER_DEPENDENCIES_H_
