// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IObjectWrapper;

interface IRemoteFragment {
  // Using IObjectWrapper instead of Bundle to pass by reference instead of by value.
  void handleOnCreate(in IObjectWrapper savedInstanceState) = 0;
  void handleOnAttach(in IObjectWrapper context) = 1;
  void handleOnActivityCreated(in IObjectWrapper savedInstanceState) = 2;
  IObjectWrapper handleOnCreateView() = 3;
  void handleOnStart() = 4;
  void handleOnResume() = 5;
  void handleOnPause() = 6;
  void handleOnStop() = 7;
  void handleOnDestroyView() = 8;
  void handleOnDetach() = 9;
  void handleOnDestroy() = 10;
  void handleOnSaveInstanceState(in IObjectWrapper outState) = 11;
  // |data| is an Intent with the result returned from the activity.
  void handleOnActivityResult(int requestCode,
                              int resultCode,
                              in IObjectWrapper data) = 12;
  void handleOnRequestPermissionsResult(int requestCode,
                                        in String[] permissions,
                                        in int[] grantResults) = 13;
}
