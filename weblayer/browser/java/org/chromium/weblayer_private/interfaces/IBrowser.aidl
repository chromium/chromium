// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IBrowserClient;
import org.chromium.weblayer_private.interfaces.IBrowserFragment;
import org.chromium.weblayer_private.interfaces.IMediaRouteDialogFragment;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IProfile;
import org.chromium.weblayer_private.interfaces.ITab;

import java.util.List;

interface IBrowser {
  IProfile getProfile() = 0;

  // Sets the active tab, returns false if tab is not attached to this fragment.
  boolean setActiveTab(in ITab tab) = 3;

  int getActiveTabId() = 4;
  List getTabs() = 5;

  void setClient(in IBrowserClient client) = 6;

  void addTab(in ITab tab) = 7;
  void destroyTab(in ITab tab) = 8;

  ITab createTab() = 11;

  boolean isRestoringPreviousState() = 14;

  // Added in 90.
  void setDarkModeStrategy(in int strategy) = 16;

  // Added in 105
  int[] getTabIds() = 20;

  void shutdown() = 22;

  IBrowserFragment getBrowserFragmentImpl() = 23;
  IMediaRouteDialogFragment createMediaRouteDialogFragmentImpl() = 24;

}
