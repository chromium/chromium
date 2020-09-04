// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import android.content.Intent;

interface IWebLayerClient {
  Intent createIntent() = 0;
  Intent createMediaSessionServiceIntent() = 1;
  int getMediaSessionNotificationId() = 2;

  // Since Version 86.
  Intent createImageDecoderServiceIntent() = 3;
}
