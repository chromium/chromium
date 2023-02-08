// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

/**
 * An interface for listening to postMessages from the web content.
 */
public interface MessageEventListener {
    public abstract void onMessage(Tab source, String message);
}
