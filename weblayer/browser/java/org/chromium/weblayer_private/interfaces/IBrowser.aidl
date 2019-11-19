// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IBrowserClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.IProfile;

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
}
