// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IBrowserClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IProfile;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.IUrlBarController;

import java.util.List;

interface IBrowser {
  IProfile getProfile() = 0;
  void setTopView(in IObjectWrapper view) = 1;

  // |valueCallback| is a wrapped ValueCallback<Boolean> instead. The bool value in |valueCallback|
  // indicates is whether the request was successful. Request might fail if it is subsumed by a
  // following request, or if this object is destroyed.
  void setSupportsEmbedding(in boolean enable, in IObjectWrapper valueCallback) = 2;

  // Sets the active tab, returns false if tab is not attached to this fragment.
  boolean setActiveTab(in ITab tab) = 3;

  int getActiveTabId() = 4;
  List getTabs() = 5;

  void setClient(in IBrowserClient client) = 6;

  void addTab(in ITab tab) = 7;
  void destroyTab(in ITab tab) = 8;
  IUrlBarController getUrlBarController() = 9;

  void setBottomView(in IObjectWrapper view) = 10;

  ITab createTab() = 11;

  void setTopViewAndScrollingBehavior(in IObjectWrapper view, in int minHeight,
                                      in boolean onlyExpandControlsAtPageTop,
                                      in boolean animate) = 12;

  boolean isRestoringPreviousState() = 14;

  // Added in 88.
  void setBrowserControlsOffsetsEnabled(in boolean enable) = 13;

  // Added in 89.
  void setMinimumSurfaceSize(in int width, in int height) = 15;

  // Added in 90.
  void setDarkModeStrategy(in int strategy) = 16;
  void setEmbeddabilityMode(in int mode, in IObjectWrapper valueCallback) = 17;

  // Added in 91.
  void setChangeVisibilityOnNextDetach(in boolean changeVisibility) = 18;

  // Added in 105.
  void setSurfaceControlViewHost(in IObjectWrapper host) = 19;

  // Added in 105
  int[] getTabIds() = 20;

  // Added in 106.
  IObjectWrapper getContentViewRenderView() = 21;
}
