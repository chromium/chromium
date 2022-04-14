// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/autofill_assistant/weblayer_dependencies.h"

#include "weblayer/browser/autofill_assistant/weblayer_assistant_field_trial_util.h"
#include "weblayer/browser/feature_list_creator.h"
#include "weblayer/browser/java/jni/WebLayerAssistantStaticDependencies_jni.h"

using ::autofill_assistant::Dependencies;
using ::base::android::JavaParamRef;

namespace weblayer {

static jlong JNI_WebLayerAssistantStaticDependencies_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& jstatic_dependencies) {
  // The dynamic_cast is necessary here to safely cast the resulting intptr back
  // to Dependencies using reinterpret_cast.
  return reinterpret_cast<intptr_t>(dynamic_cast<Dependencies*>(
      new WebLayerDependencies(env, jstatic_dependencies)));
}

WebLayerDependencies::WebLayerDependencies(
    JNIEnv* env,
    const JavaParamRef<jobject>& jstatic_dependencies)
    : Dependencies(env, jstatic_dependencies) {}

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

std::string WebLayerDependencies::GetChromeSignedInEmailAddress(
    content::WebContents* web_contents) const {
  return "";
  // TODO(b/222671580): Implement
}

variations::VariationsService* WebLayerDependencies::GetVariationsService()
    const {
  return FeatureListCreator::GetInstance()->variations_service();
}

::autofill_assistant::AnnotateDomModelService*
WebLayerDependencies::GetOrCreateAnnotateDomModelService(
    content::BrowserContext* browser_context) const {
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

}  // namespace weblayer
