// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.test_interfaces;

import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ITab;

interface ITestWebLayer {
  // Force network connectivity state.
  boolean isNetworkChangeAutoDetectOn() = 1;
  // set mock location provider
  void setMockLocationProvider(in boolean enable) = 2;
  boolean isMockLocationProviderRunning() = 3;

  // Whether or not a permission dialog is currently showing.
  boolean isPermissionDialogShown() = 4;

  // Clicks a button on the permission dialog.
  void clickPermissionDialogButton(boolean allow) = 5;

  // Forces the system location setting to enabled.
  void setSystemLocationSettingEnabled(boolean enabled) = 6;

  // See comments in TestWebLayer for details.
  void waitForBrowserControlsMetadataState(in ITab tab,
                                           in int top,
                                           in int bottom,
                                           in IObjectWrapper runnable) = 7;

  void setAccessibilityEnabled(in boolean enabled) = 8;

  boolean canBrowserControlsScroll(in ITab tab) = 9;

  // Creates and shows a test infobar in |tab|, calling |runnable| when the addition (including
  // animations) is complete.
  void addInfoBar(in ITab tab, in IObjectWrapper runnable) = 10;

  // Gets the infobar container view associated with |tab|.
  IObjectWrapper getInfoBarContainerView(in ITab tab) = 11;

  void setIgnoreMissingKeyForTranslateManager(in boolean ignore) = 12;
  void forceNetworkConnectivityState(in boolean networkAvailable) = 13;

  boolean canInfoBarContainerScroll(in ITab tab) = 14;

  String getDisplayedUrl(IObjectWrapper /* View */ urlBarView) = 15;

  // Returns the target language of the currently-showing translate infobar, or null if no translate
  // infobar is currently showing.
  String getTranslateInfoBarTargetLanguage(in ITab tab) = 16;

  // Returns true if a fullscreen toast was shown for |tab|.
  boolean didShowFullscreenToast(in ITab tab) = 17;
}
