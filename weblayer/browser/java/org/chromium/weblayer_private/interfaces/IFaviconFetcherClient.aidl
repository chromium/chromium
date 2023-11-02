// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

interface IFaviconFetcherClient {
  void onDestroyed() = 1;
  void onFaviconChanged(in Bitmap bitmap) = 2;
}
