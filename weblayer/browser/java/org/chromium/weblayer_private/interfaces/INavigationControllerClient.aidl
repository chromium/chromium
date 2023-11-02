// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IClientNavigation;
import org.chromium.weblayer_private.interfaces.IClientPage;
import org.chromium.weblayer_private.interfaces.INavigation;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;

/**
 * Interface used by NavigationController to inform the client of changes. This largely duplicates
 * the NavigationCallback interface, but is a singleton to avoid unnecessary IPC.
 */
interface INavigationControllerClient {
  IClientNavigation createClientNavigation(in INavigation impl) = 0;

  void navigationStarted(IClientNavigation navigation) = 1;

  void navigationRedirected(IClientNavigation navigation) = 2;

  void readyToCommitNavigation(IClientNavigation navigation) = 3;

  void navigationCompleted(IClientNavigation navigation) = 4;

  void navigationFailed(IClientNavigation navigation) = 5;

  void loadStateChanged(boolean isLoading, boolean shouldShowLoadingUi) = 6;

  void loadProgressChanged(double progress) = 7;

  void onFirstContentfulPaint() = 8;

  void onOldPageNoLongerRendered(in String uri) = 9;

  // Added in M88.
  void onFirstContentfulPaint2(long navigationStartMs, long firstContentfulPaintDurationMs) = 10;
  void onLargestContentfulPaint(long navigationStartMs, long largestContentfulPaintDurationMs) = 11;

  // Added in M90.
  IClientPage createClientPage() = 12;
  void onPageDestroyed(IClientPage page) = 13;

  // Added in M93.
  void onPageLanguageDetermined(IClientPage page, in String language) = 14;
}
