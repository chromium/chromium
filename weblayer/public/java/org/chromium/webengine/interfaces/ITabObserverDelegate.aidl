// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.interfaces;

import android.graphics.Bitmap;

import org.chromium.webengine.interfaces.ITabParams;

oneway interface ITabObserverDelegate {
    void notifyTitleUpdated(in String title) = 1;
    void notifyVisibleUriChanged(in String uri) = 2;
    void notifyRenderProcessGone() = 3;
    void notifyFaviconChanged(in Bitmap favicon) = 4;
}