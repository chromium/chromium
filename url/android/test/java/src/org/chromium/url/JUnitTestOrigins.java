// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.url;

/**
 * Factories for {@link Origin}s usable from Robolectric tests.
 *
 * <p>{@link Origin#create(GURL)} and {@link Origin#createOpaqueOrigin()} both require native, so
 * unit tests have to build origins from the Mojo representation instead. That representation is not
 * meant to be used directly (see crbug.com/1156866), so it is wrapped here rather than duplicated
 * in every test.
 */
public class JUnitTestOrigins {
    private JUnitTestOrigins() {}

    /** Returns the tuple origin (scheme, host, port), e.g. ("https", "example.com", 443). */
    public static Origin createTuple(String scheme, String host, int port) {
        org.chromium.url.internal.mojom.Origin mojoOrigin =
                new org.chromium.url.internal.mojom.Origin();
        mojoOrigin.scheme = scheme;
        mojoOrigin.host = host;
        mojoOrigin.port = (short) port;
        // nonceIfOpaque is left at its default of null, i.e. a tuple (non-opaque) origin.
        return new Origin(mojoOrigin);
    }
}
