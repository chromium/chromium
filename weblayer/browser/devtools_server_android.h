// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_DEVTOOLS_SERVER_ANDROID_H_
#define WEBLAYER_BROWSER_DEVTOOLS_SERVER_ANDROID_H_

namespace weblayer {

class DevToolsServerAndroid {
 public:
  static void SetRemoteDebuggingEnabled(bool enabled);

  static bool GetRemoteDebuggingEnabled();
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_DEVTOOLS_SERVER_ANDROID_H_
