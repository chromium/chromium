// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEBLAYER_FACTORY_IMPL_ANDROID_H_
#define WEBLAYER_BROWSER_WEBLAYER_FACTORY_IMPL_ANDROID_H_

namespace weblayer {

// Exposes functionality from WebLayerFactoryImpl.java to C++.
class WebLayerFactoryImplAndroid {
 public:
  static int GetClientMajorVersion();
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEBLAYER_FACTORY_IMPL_ANDROID_H_
