// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.components.digital_asset_links.OriginVerifier;
import org.chromium.components.digital_asset_links.Relationship;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.embedder_support.util.UrlConstants;

import java.util.List;
import java.util.Locale;

/**
 * WebLayerOriginVerifier performs OriginVerifications for Weblayer.
 * It uses the WebLayerVerificationResultStore to cache validations.
 */
public class WebLayerOriginVerifier extends OriginVerifier {
    private static final String METADATA_SKIP_ORIGIN_VERIFICATION_KEY =
            "org.chromium.weblayer.skipOriginVerification";
    private static final String METADATA_STRICT_LOCALHOST_VERIFICATION_KEY =
            "org.chromium.weblayer.strictLocalhostVerification";
    private final boolean mSkipOriginVerification = getSkipOriginVerificationFromManifest();
    private final boolean mStrictLocalhostVerification =
            getStrictLocalhostVerificationFromManifest();

    /**
     * Main constructor.
     * Use {@link WebLayerOriginVerifier#start}.
     * @param packageName The package for the Android application for verification.
     * @param relationship Digital Asset Links relationship to use during verification.
     * @param verificationResultStore The {@link ChromeVerificationResultStore} for persisting
     *         results.
     *
     */
    public WebLayerOriginVerifier(String packageName, String relationship,
            @Nullable WebLayerVerificationResultStore verificationResultStore) {
        super(packageName, relationship, null, verificationResultStore);
    }

    @Override
    public boolean isAllowlisted(String packageName, Origin origin, String relation) {
        String host = origin.uri().getHost();

        if (UrlConstants.LOCALHOST.equals(host.toLowerCase(Locale.US))) {
            return !mStrictLocalhostVerification;
        }

        return false;
    }

    @Override
    public boolean wasPreviouslyVerified(Origin origin) {
        return wasPreviouslyVerified(mPackageName, mSignatureFingerprints, origin, mRelation);
    }

    /**
     * Returns whether an origin is first-party relative to a given package name.
     *
     * This only returns data from previously cached relations, and does not trigger an asynchronous
     * validation.
     *
     * @param packageName The package name.
     * @param signatureFingerprint The signatures of the package.
     * @param origin The origin to verify.
     * @param relation The Digital Asset Links relation to verify for.
     */
    private static boolean wasPreviouslyVerified(String packageName,
            List<String> signatureFingerprints, Origin origin, String relation) {
        WebLayerVerificationResultStore resultStore = WebLayerVerificationResultStore.getInstance();
        return resultStore.shouldOverride(packageName, origin, relation)
                || resultStore.isRelationshipSaved(
                        new Relationship(packageName, signatureFingerprints, origin, relation));
    }

    // TODO(swestphal): Only for testing during development, remove again eventually.
    boolean skipOriginVerification() {
        return mSkipOriginVerification;
    }

    private boolean getSkipOriginVerificationFromManifest() {
        try {
            Context context = ContextUtils.getApplicationContext();
            Bundle metaData = context.getPackageManager()
                                      .getApplicationInfo(context.getPackageName(),
                                              PackageManager.GET_META_DATA)
                                      .metaData;
            if (metaData != null) {
                return metaData.getBoolean(METADATA_SKIP_ORIGIN_VERIFICATION_KEY);
            }
        } catch (PackageManager.NameNotFoundException e) {
        }
        return false;
    }

    @VisibleForTesting
    boolean getStrictLocalhostVerificationFromManifest() {
        try {
            Context context = ContextUtils.getApplicationContext();
            Bundle metaData = context.getPackageManager()
                                      .getApplicationInfo(context.getPackageName(),
                                              PackageManager.GET_META_DATA)
                                      .metaData;
            if (metaData != null) {
                return metaData.getBoolean(METADATA_STRICT_LOCALHOST_VERIFICATION_KEY);
            }
        } catch (PackageManager.NameNotFoundException e) {
        }
        return false;
    }

    @Override
    public void recordResultMetrics(OriginVerifier.VerifierResult result) {
        // TODO(swestphal): Implement UMA logging.
    }

    @Override
    public void recordVerificationTimeMetrics(long duration, boolean online) {
        // TODO(swestphal): Implement UMA logging.
    }
}