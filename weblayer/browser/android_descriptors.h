// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_ANDROID_DESCRIPTORS_H_
#define WEBLAYER_BROWSER_ANDROID_DESCRIPTORS_H_

#include "content/public/common/content_descriptors.h"

namespace weblayer {

// This is a list of global descriptor keys to be used with the
// base::GlobalDescriptors object (see base/posix/global_descriptors.h)
enum {
  kWebLayerLocalePakDescriptor = kContentIPCDescriptorMax + 1,
  kWebLayerMainPakDescriptor,
  kWebLayer100PercentPakDescriptor,
  kWebLayerSecondaryLocalePakDescriptor,
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_ANDROID_DESCRIPTORS_H_
