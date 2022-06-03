// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

/**
 * Used to forward find in page results to the client.
 */
interface IFindInPageCallbackClient {
  void onFindResult(in int numberOfMatches, in int activeMatchOrdinal, in boolean finalUpdate) = 0;
  void onFindEnded() = 1;
}
