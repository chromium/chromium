// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IClientPage;

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

  boolean isPageInitiated() = 11;
  boolean isReload() = 12;

  // @since 89
  boolean wasIntentLaunched() = 13;
  boolean isUserDecidingIntentLaunch() = 14;
  boolean isKnownProtocol() = 15;
  boolean isServedFromBackForwardCache() = 16;
  boolean isFormSubmission() = 19;
  String getReferrer() = 20;

  // @since 88
  void disableNetworkErrorAutoReload() = 17;

  // @since 90
  IClientPage getPage() = 18;

  // @since 91
  List<String> getResponseHeaders() = 21;

  // @since 92
  int getNavigationEntryOffset() = 22;

  // @since 97
  void disableIntentProcessing() = 23;

  // @since 102
  boolean wasFetchedFromCache() = 24;
}
