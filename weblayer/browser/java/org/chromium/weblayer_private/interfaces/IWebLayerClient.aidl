// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import android.content.Intent;

interface IWebLayerClient {
  Intent createIntent() = 0;
  Intent createMediaSessionServiceIntent() = 1;
  int getMediaSessionNotificationId() = 2;
  Intent createImageDecoderServiceIntent() = 3;

  // Since Version 88.
  Intent createRemoteMediaServiceIntent() = 7;
  int getPresentationApiNotificationId() = 8;
  int getRemotePlaybackApiNotificationId() = 9;

  // Added in Version 98.
  int getMaxNavigationsPerTabForInstanceState() = 10;
}
