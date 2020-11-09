// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import android.content.Intent;
import android.os.Bundle;

import org.chromium.weblayer_private.interfaces.IBrowserFragment;
import org.chromium.weblayer_private.interfaces.ICrashReporterController;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IProfile;
import org.chromium.weblayer_private.interfaces.IRemoteFragmentClient;
import org.chromium.weblayer_private.interfaces.IMediaRouteDialogFragment;
import org.chromium.weblayer_private.interfaces.ISiteSettingsFragment;
import org.chromium.weblayer_private.interfaces.IWebLayerClient;

interface IWebLayer {
  // ID 1 was loadAsyncV80 and was removed in M86.
  // ID 2 was loadSyncV80 and was removed in M86.

  // Creates the WebLayer counterpart to a BrowserFragment - a BrowserFragmentImpl
  //
  // @param fragmentClient Representative of the Fragment on the client side through which
  // WebLayer can call methods on Fragment.
  // @param fragmentArgs Bundle of arguments with which the Fragment was created on the client side
  // (see Fragment#setArguments).
  IBrowserFragment createBrowserFragmentImpl(in IRemoteFragmentClient fragmentClient,
      in IObjectWrapper fragmentArgs) = 3;

  // Create or get the profile matching profileName.
  IProfile getProfile(in String profileName) = 4;

  // Enable or disable DevTools remote debugging server.
  void setRemoteDebuggingEnabled(boolean enabled) = 5;

  // Returns whether or not the DevTools remote debugging server is enabled.
  boolean isRemoteDebuggingEnabled() = 6;

  // ID 7 was getCrashReporterControllerV80 and was removed in M86.

  // Initializes WebLayer and starts loading.
  //
  // It is expected that either loadAsync or loadSync is called before anything else.
  //
  // @param appContext     A Context that refers to the Application using WebLayer.
  // @param remoteContext  A Context that refers to the WebLayer provider package.
  // @param loadedCallback A ValueCallback that will be called when load completes.
  void loadAsync(in IObjectWrapper appContext,
                 in IObjectWrapper remoteContext,
                 in IObjectWrapper loadedCallback) = 8;

  // Initializes WebLayer, starts loading and blocks until loading has completed.
  //
  // It is expected that either loadAsync or loadSync is called before anything else.
  //
  // @param appContext    A Context that refers to the Application using WebLayer.
  // @param remoteContext A Context that refers to the WebLayer provider package.
  void loadSync(in IObjectWrapper appContext,
                in IObjectWrapper remoteContext) = 9;

  // Returns the singleton crash reporter controller. If WebLayer has not been
  // initialized, does the minimum initialization needed for the crash reporter.
  ICrashReporterController getCrashReporterController(
      in IObjectWrapper appContext,
      in IObjectWrapper remoteContext) = 10;

  // Forwards broadcast from a notification to the implementation.
  void onReceivedBroadcast(in IObjectWrapper appContext, in Intent intent) = 11;

  // Added in Version 82.
  void enumerateAllProfileNames(in IObjectWrapper valueCallback) = 12;

  // Added in Version 83.
  void setClient(in IWebLayerClient client) = 13;

  // Added in Version 84.
  String getUserAgentString() = 14;

  // Added in Version 84.
  void registerExternalExperimentIDs(in String trialName, in int[] experimentIds) = 15;

  // Creates the WebLayer counterpart to a SiteSettingsFragment - a SiteSettingsFragmentImpl
  //
  // @param fragmentClient Representative of the Fragment on the client side through which
  // WebLayer can call methods on Fragment.
  // @param fragmentArgs Bundle of arguments with which the Fragment was created on the client side
  // (see Fragment#setArguments).
  // Added in Version 84.
  ISiteSettingsFragment createSiteSettingsFragmentImpl(
      in IRemoteFragmentClient remoteFragmentClient,
      in IObjectWrapper fragmentArgs) = 16;

  // Added in Version 85.
  void onMediaSessionServiceStarted(in IObjectWrapper sessionService, in Intent intent) = 17;
  void onMediaSessionServiceDestroyed() = 18;

  // Added in Version 86.
  IBinder initializeImageDecoder(in IObjectWrapper appContext,
                                 in IObjectWrapper remoteContext) = 19;

  // Added in Version 87.
  IObjectWrapper getApplicationContext() = 20;
  IMediaRouteDialogFragment createMediaRouteDialogFragmentImpl(
      in IRemoteFragmentClient remoteFragmentClient) = 21;
  IProfile getIncognitoProfile(in String profileName) = 24;
}
