// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEBLAYER_CONTENT_BROWSER_OVERLAY_MANIFEST_H_
#define WEBLAYER_BROWSER_WEBLAYER_CONTENT_BROWSER_OVERLAY_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace weblayer {

// Returns the manifest WebLayer amends to Content's content_browser service
// manifest. This allows WebLayer to extend the capabilities exposed and/or
// required by content_browser service instances, as well as declaring any
// additional in- and out-of-process per-profile packaged services.
const service_manager::Manifest& GetWebLayerContentBrowserOverlayManifest();

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEBLAYER_CONTENT_BROWSER_OVERLAY_MANIFEST_H_
