// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

interface INavigationController {
  void navigate(in String uri) = 0;

  void goBack() = 1;

  void goForward() = 2;

  void reload() = 3;

  void stop() = 4;

  int getNavigationListSize() = 5;

  int getNavigationListCurrentIndex() = 6;

  String getNavigationEntryDisplayUri(in int index) = 7;

  boolean canGoBack() = 8;

  boolean canGoForward() = 9;
}
