// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.url;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.nio.ByteBuffer;

/** An origin is either a (scheme, host, port) tuple or is opaque. */
@JNINamespace("url")
public class Origin {
    // The internal representation of the origin that should never be used directly.
    private final org.chromium.url.internal.mojom.Origin mInternal;

    /** @return The scheme of the origin. Returns an empty string for an opaque origin. */
    public String getScheme() {
        return !isOpaque() ? mInternal.scheme : "";
    }

    /** @return The host of the origin. Returns an empty string for an opaque origin. */
    public String getHost() {
        return !isOpaque() ? mInternal.host : "";
    }

    /** @return The port of the origin. Returns 0 for an opaque origin. */
    public int getPort() {
        return !isOpaque() ? mInternal.port : 0;
    }

    /** @return Whether the origin is opaque. */
    public boolean isOpaque() {
        return mInternal.nonceIfOpaque != null;
    }

    @CalledByNative
    private static ByteBuffer serialize(Origin origin) {
        return origin.mInternal.serialize();
    }

    @CalledByNative
    private Origin(ByteBuffer byteBuffer) {
        mInternal = org.chromium.url.internal.mojom.Origin.deserialize(byteBuffer);
    }
}