// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

interface IWebMessageReplyProxy {
  void postMessage(in String message) = 0;

  // @since 90
  boolean isActive() = 1;
}
