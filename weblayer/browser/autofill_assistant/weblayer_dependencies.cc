// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/autofill_assistant/weblayer_dependencies.h"

#include "base/android/jni_string.h"
#include "base/android/locale_utils.h"
#include "components/autofill_assistant/browser/android/dependencies_android.h"
#include "components/autofill_assistant/browser/common_dependencies.h"
#include "components/autofill_assistant/browser/dependencies_util.h"
#include "components/autofill_assistant/browser/platform_dependencies.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/security_state/core/security_state.h"
#include "components/version_info/android/channel_getter.h"
#include "weblayer/browser/autofill_assistant/weblayer_assistant_field_trial_util.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/feature_list_creator.h"
#include "weblayer/browser/java/jni/WebLayerAssistantStaticDependencies_jni.h"
#include "weblayer/browser/profile_impl.h"

using ::autofill_assistant::CommonDependencies;
using ::autofill_assistant::DependenciesAndroid;
using ::autofill_assistant::PlatformDependencies;
using ::base::android::AttachCurrentThread;
using ::base::android::ConvertJavaStringToUTF8;
using ::base::android::JavaParamRef;
using ::base::android::ScopedJavaLocalRef;

namespace weblayer {

static jlong JNI_WebLayerAssistantStaticDependencies_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& jstatic_dependencies) {
  // The dynamic_cast is necessary here to safely cast the resulting intptr back
  // to DependenciesAndroid using reinterpret_cast.
  return reinterpret_cast<intptr_t>(dynamic_cast<DependenciesAndroid*>(
      new WebLayerDependencies(env, jstatic_dependencies)));
}

static ScopedJavaLocalRef<jobject>
JNI_WebLayerAssistantStaticDependencies_GetJavaProfile(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  if (!web_contents) {
    return nullptr;
  }
  return ScopedJavaLocalRef<jobject>(
      ProfileImpl::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetJavaProfile());
}

static jlong JNI_WebLayerAssistantStaticDependencies_GetSimpleFactoryKey(
    JNIEnv* env,
    jlong browser_context_ptr) {
  content::BrowserContext* browser_context =
      reinterpret_cast<content::BrowserContext*>(browser_context_ptr);
  if (!browser_context) {
    return 0;
  }
  SimpleFactoryKey* key =
      static_cast<BrowserContextImpl*>(browser_context)->simple_factory_key();
  return reinterpret_cast<intptr_t>(key);
}

WebLayerDependencies::WebLayerDependencies(
    JNIEnv* env,
    const JavaParamRef<jobject>& jstatic_dependencies)
    : DependenciesAndroid(env, jstatic_dependencies) {}

const CommonDependencies* WebLayerDependencies::GetCommonDependencies() const {
  return this;
}

const PlatformDependencies* WebLayerDependencies::GetPlatformDependencies()
    const {
  return this;
}

std::unique_ptr<::autofill_assistant::AssistantFieldTrialUtil>
WebLayerDependencies::CreateFieldTrialUtil() const {
  return std::make_unique<WebLayerAssistantFieldTrialUtil>();
}

autofill::PersonalDataManager* WebLayerDependencies::GetPersonalDataManager()
    const {
  // TODO(b/222671580): Add NOTREACHED?
  return nullptr;
}

password_manager::PasswordManagerClient*
WebLayerDependencies::GetPasswordManagerClient(
    content::WebContents* web_contents) const {
  // TODO(b/222671580): Add NOTREACHED?
  return nullptr;
}

PrefService* WebLayerDependencies::GetPrefs() const {
  return nullptr;
}

std::string WebLayerDependencies::GetSignedInEmail() const {
  return std::string();
}

bool WebLayerDependencies::IsSupervisedUser() const {
  // WebLayer does not support supervised users.
  return false;
}

security_state::SecurityLevel WebLayerDependencies::GetSecurityLevel(
    content::WebContents* web_contents) const {
  return security_state::SecurityLevel::NONE;
}

std::string WebLayerDependencies::GetLocale() const {
  return base::android::GetDefaultLocaleString();
}

std::string WebLayerDependencies::GetLatestCountryCode() const {
  return autofill_assistant::dependencies_util::GetLatestCountryCode(
      FeatureListCreator::GetInstance()->variations_service());
}

std::string WebLayerDependencies::GetStoredPermanentCountryCode() const {
  return autofill_assistant::dependencies_util::GetStoredPermanentCountryCode(
      FeatureListCreator::GetInstance()->variations_service());
}

::autofill_assistant::AnnotateDomModelService*
WebLayerDependencies::GetOrCreateAnnotateDomModelService() const {
  // TODO(b/222671580): Add NOTREACHED?
  return nullptr;
}

bool WebLayerDependencies::IsCustomTab(
    const content::WebContents& web_contents) const {
  return false;
}

bool WebLayerDependencies::IsWebLayer() const {
  return true;
}

signin::IdentityManager* WebLayerDependencies::GetIdentityManager() const {
  // TODO(b/222671580): implement.
  return nullptr;
}

consent_auditor::ConsentAuditor* WebLayerDependencies::GetConsentAuditor()
    const {
  return nullptr;
}

version_info::Channel WebLayerDependencies::GetChannel() const {
  return version_info::android::GetChannel();
}

}  // namespace weblayer
