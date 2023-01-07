// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import android.os.Bundle;

import org.chromium.weblayer_private.interfaces.IWebLayer;

// Factory for creating IWebLayer as well as determining if a particular version
// of a client is supported.
interface IWebLayerFactory {
  // Returns true if a client with the specified version is supported.
  boolean isClientSupported() = 0;

  // Creates a new IWebLayer. It is expected that a client has a single
  // IWebLayer. Further, at this time, only a single client is supported.
  IWebLayer createWebLayer() = 1;

  // Returns the full version string of the implementation.
  String getImplementationVersion() = 2;

  // Returns the major version of the implementation.
  int getImplementationMajorVersion() = 3;
}
