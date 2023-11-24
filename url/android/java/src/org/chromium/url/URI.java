// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.url;

import java.net.URISyntaxException;

/**
 * An API shim around GURL that mostly matches the java.net.URI API.
 *
 * @deprecated Please use GURL directly in new code.
 */
@Deprecated
public class URI extends GURL {
    /** Create a new GURL with a java.net.URI API shim. */
    public URI(String uri) throws URISyntaxException {
        super(uri);
        if (!isValid()) {
            throw new URISyntaxException(uri, "Uri could not be parsed as a valid GURL");
        }
    }

    private URI() {}

    /**
     * This function is a convenience wrapper around {@link URI#URI(String)}, that wraps the thrown
     * thrown URISyntaxException in an IllegalArgumentException and throws that instead.
     */
    public static URI create(String str) {
        try {
            return new URI(str);
        } catch (URISyntaxException e) {
            throw new IllegalArgumentException(e);
        }
    }

    @Override
    public URI getOrigin() {
        URI target = new URI();
        getOriginInternal(target);
        return target;
    }

    /** See {@link GURL#getRef()} */
    public String getFragment() {
        return getRef();
    }

    /** See {@link java.net.URI#isAbsolute()} */
    public boolean isAbsolute() {
        return !getScheme().isEmpty();
    }

    @Override
    public String toString() {
        return getPossiblyInvalidSpec();
    }
}
