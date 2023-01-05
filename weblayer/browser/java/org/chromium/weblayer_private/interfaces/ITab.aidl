// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import java.util.List;

import org.chromium.weblayer_private.interfaces.IContextMenuParams;
import org.chromium.weblayer_private.interfaces.IDownloadCallbackClient;
import org.chromium.weblayer_private.interfaces.IErrorPageCallbackClient;
import org.chromium.weblayer_private.interfaces.IExternalIntentInIncognitoCallbackClient;
import org.chromium.weblayer_private.interfaces.IFaviconFetcher;
import org.chromium.weblayer_private.interfaces.IFaviconFetcherClient;
import org.chromium.weblayer_private.interfaces.IFindInPageCallbackClient;
import org.chromium.weblayer_private.interfaces.IFullscreenCallbackClient;
import org.chromium.weblayer_private.interfaces.IGoogleAccountsCallbackClient;
import org.chromium.weblayer_private.interfaces.IMediaCaptureCallbackClient;
import org.chromium.weblayer_private.interfaces.INavigationController;
import org.chromium.weblayer_private.interfaces.INavigationControllerClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IStringCallback;
import org.chromium.weblayer_private.interfaces.ITabClient;
import org.chromium.weblayer_private.interfaces.IWebMessageCallbackClient;

interface ITab {
  void setClient(in ITabClient client) = 0;

  INavigationController createNavigationController(in INavigationControllerClient client) = 1;

  // ID 2 was setDownloadCallbackClient and was removed in M89.

  void setErrorPageCallbackClient(IErrorPageCallbackClient client) = 3;

  void setFullscreenCallbackClient(in IFullscreenCallbackClient client) = 4;

  void executeScript(in String script, boolean useSeparateIsolate, in IStringCallback callback) = 5;

  void setNewTabsEnabled(in boolean enabled) = 6;

  // Returns a unique identifier for this Tab. The id is *not* unique across
  // restores. The id is intended for the client library to avoid creating duplicate client objects
  // for the same ITab.
  int getId() = 7;

  boolean setFindInPageCallbackClient(IFindInPageCallbackClient client) = 8;
  void findInPage(in String searchText, boolean forward) = 9;

  // And and removed in 82; superseded by dismissTransientUi().
  // void dismissTabModalOverlay() = 10;
  void dispatchBeforeUnloadAndClose() = 11;

  boolean dismissTransientUi() = 12;

  String getGuid() = 13;

  void setMediaCaptureCallbackClient(in IMediaCaptureCallbackClient client) = 14;
  void stopMediaCapturing() = 15;

  void captureScreenShot(in float scale, in IObjectWrapper resultCallback) = 16;

  boolean setData(in Map data) = 17;
  Map getData() = 18;
  void registerWebMessageCallback(in String jsObjectName,
                                  in List<String> allowedOrigins,
                                  in IWebMessageCallbackClient client) = 19;
  void unregisterWebMessageCallback(in String jsObjectName) = 20;
  boolean canTranslate() = 21;
  void showTranslateUi() = 22;

  void setGoogleAccountsCallbackClient(IGoogleAccountsCallbackClient client) = 23;
  IFaviconFetcher createFaviconFetcher(IFaviconFetcherClient client) = 24;
  void setTranslateTargetLanguage(in String targetLanguage) = 25;

  void setScrollOffsetsEnabled(in boolean enabled) = 26;

  // Added in 88
  void setFloatingActionModeOverride(in int actionModeItemTypes) = 27;
  boolean willAutomaticallyReloadAfterCrash() = 28;
  void setDesktopUserAgentEnabled(in boolean enable) = 29;
  boolean isDesktopUserAgentEnabled() = 30;
  void download(in IContextMenuParams contextMenuParams) = 31;

  // Added in 90
  void addToHomescreen() = 32;

  // Added in 93
  void setExternalIntentInIncognitoCallbackClient(IExternalIntentInIncognitoCallbackClient client) = 33;

  String getUri() = 34;
}
