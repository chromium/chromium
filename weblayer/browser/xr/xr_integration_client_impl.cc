// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/xr/xr_integration_client_impl.h"

#include <memory>

#include "base/feature_list.h"
#include "components/webxr/android/ar_compositor_delegate_provider.h"
#include "components/webxr/android/arcore_device_provider.h"
#include "content/public/browser/xr_install_helper.h"
#include "content/public/common/content_features.h"
#include "device/vr/public/cpp/vr_device_provider.h"
#include "device/vr/public/mojom/vr_service.mojom-shared.h"
#include "weblayer/browser/java/jni/ArCompositorDelegateProviderImpl_jni.h"
#include "weblayer/browser/java/jni/ArCoreVersionUtils_jni.h"

namespace weblayer {

namespace {

// This install helper simply checks if the necessary package (Google Play
// Services for AR, aka arcore) is installed. It doesn't attempt to initiate an
// install or update.
class ArInstallHelper : public content::XrInstallHelper {
 public:
  explicit ArInstallHelper() = default;
  ~ArInstallHelper() override = default;
  ArInstallHelper(const ArInstallHelper&) = delete;
  ArInstallHelper& operator=(const ArInstallHelper&) = delete;

  // content::XrInstallHelper implementation.
  void EnsureInstalled(
      int render_process_id,
      int render_frame_id,
      base::OnceCallback<void(bool)> install_callback) override {
    std::move(install_callback)
        .Run(Java_ArCoreVersionUtils_isInstalledAndCompatible(
            base::android::AttachCurrentThread()));
  }
};

}  // namespace

bool XrIntegrationClientImpl::IsEnabled() {
  return Java_ArCoreVersionUtils_isEnabled(
      base::android::AttachCurrentThread());
}

std::unique_ptr<content::XrInstallHelper>
XrIntegrationClientImpl::GetInstallHelper(device::mojom::XRDeviceId device_id) {
  if (device_id == device::mojom::XRDeviceId::ARCORE_DEVICE_ID)
    return std::make_unique<ArInstallHelper>();

  return nullptr;
}

content::XRProviderList XrIntegrationClientImpl::GetAdditionalProviders() {
  content::XRProviderList providers;

  if (base::FeatureList::IsEnabled(features::kWebXrArModule)) {
    base::android::ScopedJavaLocalRef<jobject>
        j_ar_compositor_delegate_provider =
            Java_ArCompositorDelegateProviderImpl_Constructor(
                base::android::AttachCurrentThread());

    providers.push_back(std::make_unique<webxr::ArCoreDeviceProvider>(
        webxr::ArCompositorDelegateProvider(
            std::move(j_ar_compositor_delegate_provider))));
  }

  return providers;
}

}  // namespace weblayer
