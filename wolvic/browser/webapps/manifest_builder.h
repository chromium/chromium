// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_MANIFEST_BUILDER_H_
#define WOLVIC_BROWSER_MANIFEST_BUILDER_H_

#include <string>

#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

namespace wolvic {

class ManifestBuilder {
 public:
  static std::string FromMojoToJson(const blink::mojom::Manifest& manifest);
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_MANIFEST_BUILDER_H_
