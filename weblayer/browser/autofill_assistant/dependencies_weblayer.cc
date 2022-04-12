// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/autofill_assistant/dependencies_weblayer.h"

#include "weblayer/browser/java/jni/WebLayerAssistantStaticDependencies_jni.h"

using ::base::android::JavaParamRef;

namespace autofill_assistant {

static jlong JNI_WebLayerAssistantStaticDependencies_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& jstatic_dependencies) {
  // The dynamic_cast is necessary here to safely cast the resulting intptr back
  // to Dependencies using reinterpret_cast.
  return reinterpret_cast<intptr_t>(dynamic_cast<Dependencies*>(
      new DependenciesWebLayer(env, jstatic_dependencies)));
}

DependenciesWebLayer::DependenciesWebLayer(
    JNIEnv* env,
    const JavaParamRef<jobject>& jstatic_dependencies)
    : Dependencies(env, jstatic_dependencies) {}

std::unique_ptr<AssistantFieldTrialUtil>
DependenciesWebLayer::CreateFieldTrialUtil() const {
  // TODO(b/222671580): Implement
  return nullptr;
}

autofill::PersonalDataManager* DependenciesWebLayer::GetPersonalDataManager()
    const {
  // TODO(b/222671580): Add NOTREACHED?
  return nullptr;
}

password_manager::PasswordManagerClient*
DependenciesWebLayer::GetPasswordManagerClient(
    content::WebContents* web_contents) const {
  // TODO(b/222671580): Add NOTREACHED?
  return nullptr;
}

std::string DependenciesWebLayer::GetChromeSignedInEmailAddress(
    content::WebContents* web_contents) const {
  return "";
  // TODO(b/222671580): Implement
}

variations::VariationsService* DependenciesWebLayer::GetVariationsService()
    const {
  // TODO(b/222671580): Implement
  return nullptr;
}

AnnotateDomModelService*
DependenciesWebLayer::GetOrCreateAnnotateDomModelService(
    content::BrowserContext* browser_context) const {
  // TODO(b/222671580): Add NOTREACHED?
  return nullptr;
}

bool DependenciesWebLayer::IsCustomTab(
    const content::WebContents& web_contents) const {
  // TODO(b/222671580): Implement
  return false;
}

bool DependenciesWebLayer::IsWebLayer() const {
  return true;
}

}  // namespace autofill_assistant
