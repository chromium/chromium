// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IObjectWrapper;

// Next value: 15
interface IRemoteFragment {
  // Fragment events.
  void handleOnCreate() = 0;
  void handleOnAttach(in IObjectWrapper context) = 1;
  void handleOnStart() = 2;
  void handleOnResume() = 3;
  void handleOnPause() = 4;
  void handleOnStop() = 5;
  void handleOnDestroyView() = 6;
  void handleOnDetach() = 7;
  void handleOnDestroy() = 8;

  // |data| is an Intent with the result returned from the activity.
  void handleOnActivityResult(int requestCode,
                              int resultCode,
                              in IObjectWrapper data) = 9;
  void handleOnRequestPermissionsResult(int requestCode,
                                        in String[] permissions,
                                        in int[] grantResults) = 10;
  
  // Out of process operations.
  void handleSetSurfaceControlViewHost(in IObjectWrapper /* SurfaceControlViewHost */ host) = 12;

  // In process operations.
  IObjectWrapper /* View */ handleGetContentViewRenderView() = 13;

  // Fragment operations.
  void handleSetMinimumSurfaceSize(in int width, in int height) = 14;
}
