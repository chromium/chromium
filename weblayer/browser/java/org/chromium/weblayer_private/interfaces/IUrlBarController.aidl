// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IObjectWrapper;

interface IUrlBarController {

  // ID 0 was deprecatedCreateUrlBarView and was removed in M89.

  IObjectWrapper /* View */ createUrlBarView(
      in Bundle options,
      in IObjectWrapper /* View.OnClickListener */ textClickListener,
      in IObjectWrapper /* View.OnLongClickListener */ textLongClickListener) = 1;

  // Added in 95.
  void showPageInfo(in Bundle options) = 2;
}
