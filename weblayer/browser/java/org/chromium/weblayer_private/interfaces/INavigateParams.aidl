// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IObjectWrapper;

/**
 * Provides parameters for NavigationController.navigate.
 */
interface INavigateParams {
  void replaceCurrentEntry() = 0;
  void disableIntentProcessing() = 1;
  void disableNetworkErrorAutoReload() = 2;
  void enableAutoPlay() = 3;
  void setResponse(in IObjectWrapper response) = 4;
}
