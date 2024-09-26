// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.url;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import java.util.Objects;

/** An origin is either a (scheme, host, port) tuple or is opaque. */
@JNINamespace("url")
public class Origin {
    private final String mScheme;
    private final String mHost;
    private final short mPort;

    private final boolean mIsOpaque;

    // Serialization of the Unguessable Token. Do not use directly.
    private final long mTokenHighBits;
    private final long mTokenLowBits;

    /** Constructs an opaque origin. */
    public static Origin createOpaqueOrigin() {
        return OriginJni.get().createOpaque();
    }

    /**
     * See origin.h for many warnings about this method.
     *
     * Constructs an Origin from a GURL.
     */
    public static Origin create(GURL gurl) {
        return OriginJni.get().createFromGURL(gurl);
    }

    /**
     * Parses a mojo Origin into a Java analogue of the c++ Origin class.
     *
     * `org.chromium.url.internal.mojom.Origin`s, are provided by Mojo-generated code but not
     * intended for direct use (see crbug.com/1156866).
     *
     * @return A Java equivalent of the c++ Origin represented by the provided mojo Origin.
     */
    public Origin(org.chromium.url.internal.mojom.Origin mojoOrigin) {
        mScheme = mojoOrigin.scheme;
        mHost = mojoOrigin.host;
        mPort = mojoOrigin.port;
        if (mojoOrigin.nonceIfOpaque != null) {
            mIsOpaque = true;
            mTokenHighBits = mojoOrigin.nonceIfOpaque.high;
            mTokenLowBits = mojoOrigin.nonceIfOpaque.low;
        } else {
            mIsOpaque = false;
            mTokenHighBits = 0;
            mTokenLowBits = 0;
        }
    }

    @CalledByNative
    private Origin(
            @JniType("std::string") String scheme,
            @JniType("std::string") String host,
            short port,
            boolean isOpaque,
            long tokenHighBits,
            long tokenLowBits) {
        mScheme = scheme;
        mHost = host;
        mPort = port;
        mIsOpaque = isOpaque;
        mTokenHighBits = tokenHighBits;
        mTokenLowBits = tokenLowBits;
    }

    /** @return The scheme of the origin. Returns an empty string for an opaque origin. */
    public String getScheme() {
        return !isOpaque() ? mScheme : "";
    }

    /** @return The host of the origin. Returns an empty string for an opaque origin. */
    public String getHost() {
        return !isOpaque() ? mHost : "";
    }

    /** @return The port of the origin. Returns 0 for an opaque origin. */
    public int getPort() {
        return !isOpaque() ? Short.toUnsignedInt(mPort) : 0;
    }

    /** @return Whether the origin is opaque. */
    public boolean isOpaque() {
        return mIsOpaque;
    }

    @Override
    public final int hashCode() {
        return Objects.hash(mScheme, mHost, mPort, mIsOpaque, mTokenHighBits, mTokenLowBits);
    }

    @Override
    public final boolean equals(Object other) {
        if (other == this) return true;
        if (!(other instanceof Origin)) return false;

        Origin that = (Origin) other;

        return mScheme.equals(that.mScheme)
                && mHost.equals(that.mHost)
                && mPort == that.mPort
                && mIsOpaque == that.mIsOpaque
                && mTokenHighBits == that.mTokenHighBits
                && mTokenLowBits == that.mTokenLowBits;
    }

    /**
     * Returns a String representing the Origin in structure of scheme://host:port or the string
     * "null" if it's opaque.
     */
    @Override
    public String toString() {
        return isOpaque() ? "null" : String.format("%s://%s:%s", mScheme, mHost, mPort);
    }

    @CalledByNative
    private void assignNativeOrigin(long nativeOrigin) {
        OriginJni.get()
                .assignNativeOrigin(
                        mScheme,
                        mHost,
                        mPort,
                        mIsOpaque,
                        mTokenHighBits,
                        mTokenLowBits,
                        nativeOrigin);
    }

    @NativeMethods
    interface Natives {
        /** Constructs a new Opaque origin. */
        Origin createOpaque();

        /** Constructs an Origin from a GURL. */
        Origin createFromGURL(GURL gurl);

        /** Initialize nativeOrigin. */
        void assignNativeOrigin(
                @JniType("std::string") String scheme,
                @JniType("std::string") String host,
                short port,
                boolean isOpaque,
                long tokenHighBits,
                long tokenLowBits,
                long nativeOrigin);
    }
}
