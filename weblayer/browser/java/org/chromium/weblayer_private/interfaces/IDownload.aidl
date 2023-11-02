// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

/**
 * Contains information about a single download that's in progress.
 */
interface IDownload {
  int getState() = 0;
  long getTotalBytes() = 1;
  long getReceivedBytes() = 2;
  void pause() = 3;
  void resume() = 4;
  void cancel() = 5;
  String getLocation() = 6;
  int getError() = 7;
  String getMimeType() = 8;
  void disableNotification() = 9;
  String getFileNameToReportToUser() = 10;
}
