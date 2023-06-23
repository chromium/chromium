// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.components.content_relationship_verification.OriginVerificationScheduler;
import org.chromium.components.content_relationship_verification.OriginVerifier;
import org.chromium.components.content_relationship_verification.OriginVerifierHelper;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.util.Set;

/**
 * Singleton.
 * WebLayerOriginVerificationScheduler provides a WebLayer specific implementation of {@link
 * OriginVerificationScheduler}.
 *
 * Call {@link WebLayerOriginVerificationScheduler#init} to initialize the statement list and call
 * {@link WebLayerOriginVerificationScheduler#verify} to perform an origin validation.
 */
public class WebLayerOriginVerificationScheduler extends OriginVerificationScheduler {
    private static final String TAG = "WLOriginVerification";

    private static WebLayerOriginVerificationScheduler sInstance;

    private WebLayerOriginVerifier mOriginVerifier;

    private WebLayerOriginVerificationScheduler(
            WebLayerOriginVerifier originVerifier, Set<Origin> pendingOrigins) {
        super(originVerifier, pendingOrigins);
        mOriginVerifier = originVerifier;
    }

    /**
     * Initializes the WebLayerOriginVerificationScheduler.
     * This should be called exactly only once as it parses the AndroidManifest and statement list.
     *
     * @param packageName the package name of the host application.
     * @param profile the profile to use for the simpleUrlLoader to download the asset links file.
     * @param context a context associated with an Activity/Service to load resources.
     */
    static void init(String packageName, BrowserContextHandle profile, Context context) {
        ThreadUtils.assertOnUiThread();
        assert sInstance
                == null : "`init(String packageName, Context context)` must only be called once";

        sInstance = new WebLayerOriginVerificationScheduler(
                new WebLayerOriginVerifier(packageName, OriginVerifier.HANDLE_ALL_URLS, profile,
                        WebLayerVerificationResultStore.getInstance()),
                OriginVerifierHelper.getClaimedOriginsFromManifest(packageName, context));
    }

    static WebLayerOriginVerificationScheduler getInstance() {
        assert sInstance != null : "Call to `init(String packageName, Context context)` missing";

        return sInstance;
    }

    @Override
    public void verify(String url, Callback<Boolean> callback) {
        if (mOriginVerifier.skipOriginVerification()) {
            callback.onResult(true);
            return;
        }
        super.verify(url, callback);
    }
}
