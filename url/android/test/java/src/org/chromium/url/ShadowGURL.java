// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.url;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.url.GURL.Natives;

/**
 * Shadow of {@link GURL}. Lets Robolectric tests use {@code GURL} without the native libraries
 * loaded.
 *
 * <p>This shadow can create only GURLs listed in {@link JUnitTestGURLs}.
 */
@Implements(GURL.class)
public class ShadowGURL {
    /**
     * The {@link GURL.Natives} implementation used by a shadowed {@link GURL}.
     */
    private static class NativesImpl implements GURL.Natives {
        @Override
        public void init(String url, GURL target) {
            target.initForTesting(JUnitTestGURLs.getGURL(url));
        }

        @Override
        public void getOrigin(String spec, boolean isValid, long nativeParsed, GURL target) {
            throw new UnsupportedOperationException(
                    "ShadowGURL.NativesImpl#getOrigin is not implemented");
        }

        @Override
        public boolean domainIs(String spec, boolean isValid, long nativeParsed, String domain) {
            throw new UnsupportedOperationException(
                    "ShadowGURL.NativesImpl#domainIs is not implemented");
        }

        @Override
        public long createNative(String spec, boolean isValid, long nativeParsed) {
            throw new UnsupportedOperationException(
                    "ShadowGURL.NativesImpl#createNative is not implemented");
        }
    }
    private static final NativesImpl sNativesInstance = new NativesImpl();

    /**
     * We could instead shadow {@code GURLJni#get}, but that would require tests using this to load
     * both shadows.
     */
    @Implementation
    protected static Natives getNatives() {
        return sNativesInstance;
    }

    @Implementation
    protected static void ensureNativeInitializedForGURL() {
        // Skip native initialization.
    }
}
