// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

/**
 * Used to forward download requests to the client.
 */
interface IDownloadCallbackClient {
  void downloadRequested(in String url, in String userAgent, in String contentDisposition, in String mimetype, long contentLength) = 0;
}
