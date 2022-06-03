// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEB_VIEW_COMPATIBILITY_HELPER_IMPL_H_
#define WEBLAYER_BROWSER_WEB_VIEW_COMPATIBILITY_HELPER_IMPL_H_

namespace weblayer {

// Manually registers JNI for WebLayer if necessary.
bool MaybeRegisterNatives();

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEB_VIEW_COMPATIBILITY_HELPER_IMPL_H_
