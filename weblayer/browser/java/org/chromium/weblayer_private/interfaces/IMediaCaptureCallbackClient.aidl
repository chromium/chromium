// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IObjectWrapper;

interface IMediaCaptureCallbackClient {
  void onMediaCaptureRequested(boolean audio, boolean video, in IObjectWrapper requestResult) = 0;
  void onMediaCaptureStateChanged(boolean audio, boolean video) = 1;
}
