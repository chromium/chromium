// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;

/**
 * Used when sending and receiving messages to a page.
 */
class WebMessage {
    private final String mContents;

    /**
     * Creates a message with the specified contents.
     *
     * @param message Contents of the message.
     */
    public WebMessage(@NonNull String message) {
        mContents = message;
    }

    /**
     * Returns the contents of the message.
     *
     * @return The contents of the message.
     */
    public @NonNull String getContents() {
        return mContents;
    }
}
