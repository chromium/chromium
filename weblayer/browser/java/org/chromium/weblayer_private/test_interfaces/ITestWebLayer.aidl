// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.test_interfaces;

import android.os.Bundle;
import org.chromium.weblayer_private.interfaces.IBrowser;
import org.chromium.weblayer_private.interfaces.IProfile;
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

  // Creates and shows a test infobar in |tab|, calling |runnable| when the addition (including
  // animations) is complete.
  void addInfoBar(in ITab tab, in IObjectWrapper runnable) = 10;

  // Gets the infobar container view associated with |tab|.
  IObjectWrapper /* View */ getInfoBarContainerView(in ITab tab) = 11;

  void setIgnoreMissingKeyForTranslateManager(in boolean ignore) = 12;
  void forceNetworkConnectivityState(in boolean networkAvailable) = 13;

  boolean canInfoBarContainerScroll(in ITab tab) = 14;

  // Returns the target language of the currently-showing translate infobar, or null if no translate
  // infobar is currently showing.
  String getTranslateInfoBarTargetLanguage(in ITab tab) = 16;

  // Returns true if a fullscreen toast was shown for |tab|.
  boolean didShowFullscreenToast(in ITab tab) = 17;

  // Does setup for MediaRouter tests, mocking out Chromecast devices.
  void initializeMockMediaRouteProvider(
      boolean closeRouteWithErrorOnSend, boolean disableIsSupportsSource,
      in String createRouteErrorMessage, in String joinRouteErrorMessage) = 18;

  // Gets a button from the currently visible media route selection dialog. The button represents a
  // route and contains the text |name|. Returns null if no such dialog or button exists.
  IObjectWrapper /* View */ getMediaRouteButton(String name) = 19;

  // Causes the renderer process in the tab's main frame to crash.
  void crashTab(in ITab tab) = 20;

  boolean isWindowOnSmallDevice(in IBrowser browser) = 21;
  void fetchAccessToken(in IProfile profile, in IObjectWrapper /* Set<String */ scopes, in IObjectWrapper /* ValueCallback<String> */ onTokenFetched) = 23;
  // Add a TestContentCaptureConsumer for the provided |browser|, with a Runnable |onNewEvent| to notify the
  // caller when the events happened, the event ID will be received through |eventsObserved| list.
  void addContentCaptureConsumer(in IBrowser browser,
                                 in IObjectWrapper /* Runnable */ onNewEvent,
                                 in IObjectWrapper /* ArrayList<Integer> */ eventsObserved) = 24;

  // Notifies the caller of autofill-related events that occur in |browser|. The caller is notified
  // via |onNewEvent| when a new event occurs, at which point the list of events that have occurred
  // since notifyOfAutofillEvents() was first invoked will be available via |eventsObserved|.
  // Note: Calling this method results in stubbing out the actual system-level integration with
  // Android Autofill.
  void notifyOfAutofillEvents(in IBrowser browser,
                              in IObjectWrapper /* Runnable */ onNewEvent,
                              in IObjectWrapper /* ArrayList<Integer> */ eventsObserved) = 25;

  // Simulates tapping the download notification with `id`.
  void activateBackgroundFetchNotification(int id) = 26;

  // Speeds up download service initialization.
  void expediteDownloadService() = 27;

  // Mocks the GMSCore Fido calls used by WebAuthn.
  void setMockWebAuthnEnabled(in boolean enabled) = 28;

  // Simulates the implementation-side event of an access token being
  // identified as invalid.
  void fireOnAccessTokenIdentifiedAsInvalid(in IProfile profile, in IObjectWrapper /* Set<String */ scopes, in IObjectWrapper /* String */ token) = 29;

  // Grants `url` location permission.
  void grantLocationPermission(String url) = 30;

  void setTextScaling(in IProfile profile, float value) = 31;
  boolean getForceEnableZoom(in IProfile profile) = 32;
}
