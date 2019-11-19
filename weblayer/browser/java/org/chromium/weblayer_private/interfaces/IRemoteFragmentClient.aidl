// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IObjectWrapper;

interface IRemoteFragmentClient {
  void superOnCreate(in IObjectWrapper savedInstanceState) = 0;
  void superOnAttach(in IObjectWrapper context) = 1;
  void superOnActivityCreated(in IObjectWrapper savedInstanceState) = 2;
  void superOnStart() = 3;
  void superOnResume() = 4;
  void superOnPause() = 5;
  void superOnStop() = 6;
  void superOnDestroyView() = 7;
  void superOnDetach() = 8;
  void superOnDestroy() = 9;
  void superOnSaveInstanceState(in IObjectWrapper outState) = 10;

  IObjectWrapper getActivity() = 11;
  boolean startActivityForResult(in IObjectWrapper intent,
                                 int requestCode,
                                 in IObjectWrapper options) = 12;
  boolean startIntentSenderForResult(in IObjectWrapper intent,
                                     int requestCode,
                                     in IObjectWrapper fillInIntent,
                                     int flagsMask,
                                     int flagsValues,
                                     int extraFlags,
                                     in IObjectWrapper options) = 13;
  boolean shouldShowRequestPermissionRationale(String permission) = 14;
  void requestPermissions(in String[] permissions, int requestCode) = 15;
}
