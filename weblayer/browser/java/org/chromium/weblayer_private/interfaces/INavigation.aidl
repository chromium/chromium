// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

/**
 * Provides information about a navigation.
 */
interface INavigation {
  int getState() = 0;

  String getUri() = 1;

  List<String> getRedirectChain() = 2;

  int getHttpStatusCode() = 3;

  boolean isSameDocument() = 4;

  boolean isErrorPage() = 5;

  int getLoadError() = 6;

  void setRequestHeader(in String name, in String value) = 7;

  void setUserAgentString(in String value) = 8;

  boolean isDownload() = 9;

  boolean wasStopCalled() = 10;

  // @since 86
  boolean isPageInitiated() = 11;
  boolean isReload() = 12;
}
