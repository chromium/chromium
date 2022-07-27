// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment.interfaces;

interface ITabProxy {
  // TODO(swestphal): Move this to the navigationController when we expose one.
  void navigate(in String url) = 1;
}
