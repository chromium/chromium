// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Bridge for setup of AccessTokenFetch browsertests from native.
 */
@JNINamespace("weblayer")
public class AccessTokenFetchTestBridge {
    AccessTokenFetchTestBridge() {}

    // Installs a GoogleAccountAccessTokenFetcherTestStub on |profile|. Returns the instance for
    // use by the test invoking this method.
    @CalledByNative
    private static GoogleAccountAccessTokenFetcherTestStub
    installGoogleAccountAccessTokenFetcherTestStub(ProfileImpl profile) {
        GoogleAccountAccessTokenFetcherTestStub testClient =
                new GoogleAccountAccessTokenFetcherTestStub();
        profile.setGoogleAccountAccessTokenFetcherClient(testClient);

        return testClient;
    }
}
