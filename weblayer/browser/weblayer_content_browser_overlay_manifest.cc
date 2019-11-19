// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/weblayer_content_browser_overlay_manifest.h"

#include "base/no_destructor.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "weblayer/browser/webui/weblayer_internals.mojom.h"

namespace weblayer {

const service_manager::Manifest& GetWebLayerContentBrowserOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .ExposeInterfaceFilterCapability_Deprecated(
              "navigation:frame", "renderer",
              service_manager::Manifest::InterfaceList<
                  weblayer_internals::mojom::PageHandler>())
          .Build()};
  return *manifest;
}

}  // namespace weblayer
