// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.interfaces;

// A FullscreenClient that is passed to the webengine to exit fullscreen
// mode programmatically.
oneway interface IFullscreenClient {
    void exitFullscreen() = 0;
}
