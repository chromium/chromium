// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment.interfaces;

import org.chromium.browserfragment.interfaces.IStringCallback;

oneway interface ITabProxy {
  void setActive() = 1;
  void close() = 2;
  void executeScript(in String script, in boolean useSeparateIsolate, in IStringCallback callback) = 3;
}
