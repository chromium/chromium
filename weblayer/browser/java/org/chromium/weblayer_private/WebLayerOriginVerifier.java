// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import androidx.annotation.Nullable;

import org.chromium.components.digital_asset_links.OriginVerifier;
import org.chromium.components.digital_asset_links.Relationship;
import org.chromium.components.embedder_support.util.Origin;

import java.util.List;

/**
 * WebLayerOriginVerifier performs OriginVerifications for Weblayer.
 * It uses the WebLayerVerificationResultStore to cache validations.
 */
public class WebLayerOriginVerifier extends OriginVerifier {
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

    @Override
    public void recordResultMetrics(OriginVerifier.VerifierResult result) {
        // TODO(swestphal): Implement UMA logging.
    }

    @Override
    public void recordVerificationTimeMetrics(long duration, boolean online) {
        // TODO(swestphal): Implement UMA logging.
    }
}