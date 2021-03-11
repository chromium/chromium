// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEBLAYER_IMPL_ANDROID_H_
#define WEBLAYER_BROWSER_WEBLAYER_IMPL_ANDROID_H_

#include <string>

#include "base/strings/string16.h"

namespace weblayer {

// Returns the name of the WebLayer embedder.
base::string16 GetClientApplicationName();

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEBLAYER_IMPL_ANDROID_H_
