// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IObjectWrapper;

/**
 * Used to forward FullscreenCallback calls to the client.
 */
interface IFullscreenCallbackClient {
  // exitFullscreenWrapper is a ValueCallback<Void> that when run exits
  // fullscreen.
  void enterFullscreen(in IObjectWrapper exitFullscreenWrapper) = 0;
  void exitFullscreen() = 1;
}
