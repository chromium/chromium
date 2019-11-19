// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IObjectWrapper;

/** Interface to forward service calls to the service implementation. */
interface IChildProcessService {
  void onCreate() = 0;

  void onDestroy() = 1;

  IObjectWrapper onBind(IObjectWrapper intent) = 2;
}
