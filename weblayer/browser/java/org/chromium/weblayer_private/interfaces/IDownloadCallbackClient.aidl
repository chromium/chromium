// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import android.content.Intent;
import org.chromium.weblayer_private.interfaces.IClientDownload;
import org.chromium.weblayer_private.interfaces.IDownload;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;

/**
 * Used to forward download requests to the client.
 */
interface IDownloadCallbackClient {
  boolean interceptDownload(in String uriString, in String userAgent, in String contentDisposition, in String mimetype, long contentLength) = 0;
  void allowDownload(in String uriString, in String requestMethod, in String requestInitiatorString, in IObjectWrapper valueCallback) = 1;
  IClientDownload createClientDownload(in IDownload impl) = 2;
  void downloadStarted(IClientDownload download) = 3;
  void downloadProgressChanged(IClientDownload download) = 4;
  void downloadCompleted(IClientDownload download) = 5;
  void downloadFailed(IClientDownload download) = 6;
  // ID 7 was createIntent and was removed in M87.
}
