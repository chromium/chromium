// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.components.digital_asset_links.VerificationResultStore;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * WebLayerVerificationResultStore stores relationships in a local variable.
 */
public class WebLayerVerificationResultStore extends VerificationResultStore {
    private static final WebLayerVerificationResultStore sInstance =
            new WebLayerVerificationResultStore();

    private Set<String> mVerifiedOrigins = Collections.synchronizedSet(new HashSet<>());

    private WebLayerVerificationResultStore() {}

    public static WebLayerVerificationResultStore getInstance() {
        return sInstance;
    }

    @Override
    protected Set<String> getRelationships() {
        return mVerifiedOrigins;
    }

    @Override
    protected void setRelationships(Set<String> relationships) {
        mVerifiedOrigins = relationships;
    }
}