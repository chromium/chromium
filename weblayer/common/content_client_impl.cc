// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/common/content_client_impl.h"

#include "build/build_config.h"
#include "components/embedder_support/origin_trials/origin_trial_policy_impl.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_util.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/public/common/url_constants.h"
#endif

namespace weblayer {

ContentClientImpl::ContentClientImpl() = default;

ContentClientImpl::~ContentClientImpl() = default;

std::u16string ContentClientImpl::GetLocalizedString(int message_id) {
  return l10n_util::GetStringUTF16(message_id);
}

std::u16string ContentClientImpl::GetLocalizedString(
    int message_id,
    const std::u16string& replacement) {
  return l10n_util::GetStringFUTF16(message_id, replacement);
}

base::StringPiece ContentClientImpl::GetDataResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* ContentClientImpl::GetDataResourceBytes(
    int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

std::string ContentClientImpl::GetDataResourceString(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      resource_id);
}

void ContentClientImpl::SetGpuInfo(const gpu::GPUInfo& gpu_info) {
  gpu::SetKeysForCrashLogging(gpu_info);
}

gfx::Image& ContentClientImpl::GetNativeImageNamed(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

blink::OriginTrialPolicy* ContentClientImpl::GetOriginTrialPolicy() {
  // Prevent initialization race (see crbug.com/721144). There may be a
  // race when the policy is needed for worker startup (which happens on a
  // separate worker thread).
  base::AutoLock auto_lock(origin_trial_policy_lock_);
  if (!origin_trial_policy_)
    origin_trial_policy_ =
        std::make_unique<embedder_support::OriginTrialPolicyImpl>();
  return origin_trial_policy_.get();
}

void ContentClientImpl::AddAdditionalSchemes(Schemes* schemes) {
#if BUILDFLAG(IS_ANDROID)
  schemes->standard_schemes.push_back(content::kAndroidAppScheme);
  schemes->referrer_schemes.push_back(content::kAndroidAppScheme);
#endif
}

}  // namespace weblayer
