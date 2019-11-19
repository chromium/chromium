// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import android.os.Bundle;

import org.chromium.weblayer_private.interfaces.IBrowserFragment;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IProfile;
import org.chromium.weblayer_private.interfaces.IRemoteFragmentClient;

interface IWebLayer {
  // Initializes WebLayer and starts loading.
  //
  // It is expected that this method is called before anything else.
  //
  // @param appContext     A Context that refers to the Application using WebLayer.
  // @param loadedCallback A ValueCallback that will be called when load completes.
  void initAndLoadAsync(in IObjectWrapper appContext,
                        in IObjectWrapper loadedCallback) = 1;

  // Blocks until loading has completed.
  void loadSync() = 2;

  // Creates the WebLayer counterpart to a BrowserFragment - a BrowserFragmentImpl
  //
  // @param fragmentClient Representative of the Fragment on the client side through which
  // WebLayer can call methods on Fragment.
  // @param fragmentArgs Bundle of arguments with which the Fragment was created on the client side
  // (see Fragment#setArguments).
  IBrowserFragment createBrowserFragmentImpl(in IRemoteFragmentClient fragmentClient,
      in IObjectWrapper fragmentArgs) = 3;

  // Create or get the profile matching profilePath.
  IProfile getProfile(in String profilePath) = 4;

  // Enable or disable DevTools remote debugging server.
  void setRemoteDebuggingEnabled(boolean enabled) = 5;

  // Returns whether or not the DevTools remote debugging server is enabled.
  boolean isRemoteDebuggingEnabled() = 6;
}
