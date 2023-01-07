// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.INavigateParams;
import org.chromium.weblayer_private.interfaces.NavigateParams;

interface INavigationController {
  // Deprecated in M89.
  void navigate(in String uri, in NavigateParams params) = 0;

  void goBack() = 1;

  void goForward() = 2;

  void reload() = 3;

  void stop() = 4;

  int getNavigationListSize() = 5;

  int getNavigationListCurrentIndex() = 6;

  String getNavigationEntryDisplayUri(in int index) = 7;

  boolean canGoBack() = 8;

  boolean canGoForward() = 9;

  void goToIndex(in int index) = 10;

  String getNavigationEntryTitle(in int index) = 11;

  // ID 12 was replace and was removed in M83.

  boolean isNavigationEntrySkippable(int index) = 13;

  // Deprecated in M89.
  void navigate2(in String uri,
                 in boolean shouldReplaceEntry,
                 in boolean disableIntentProcessing,
                 in boolean disableNetworkErrorAutoReload,
                 in boolean enableAutoPlay) = 14;

  INavigateParams createNavigateParams() = 15;
  void navigate3(in String uri,
                 in INavigateParams params) = 16;
}
