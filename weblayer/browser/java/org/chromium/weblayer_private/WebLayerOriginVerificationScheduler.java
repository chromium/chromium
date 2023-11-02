// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Resources.NotFoundException;
import android.os.Bundle;

import androidx.annotation.Nullable;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.components.digital_asset_links.OriginVerifier;
import org.chromium.components.embedder_support.util.Origin;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * WebLayerOriginVerificationScheduler parses the AndroidManifest to read the statement list, and
 * provides functionality to validate URLs using {@link WebLayerOriginVerifier}.
 *
 * Call {@link WebLayerOriginVerificationScheduler#init} to initialize the statement list and call
 * {@link WebLayerOriginVerificationScheduler#validate} to perform a validation. It is safe to call
 * validate several times, the request for the statement list on the website will only performed at
 * most once.
 */
public class WebLayerOriginVerificationScheduler {
    private static final String TAG = "WLOriginVerification";

    private static final String ASSET_STATEMENTS_METADATA_KEY = "asset_statements";
    private static final String STATEMENT_RELATION_KEY = "relation";
    private static final String STATEMENT_TARGET_KEY = "target";
    private static final String STATEMENT_NAMESPACE_KEY = "namespace";
    private static final String STATEMENT_SITE_KEY = "site";

    private static final String HTTP_SCHEME = "http";
    private static final String HTTPS_SCHEME = "https";

    private static WebLayerOriginVerificationScheduler sInstance;
    private WebLayerOriginVerifier mOriginVerifier;

    /**
     * Origins that we have yet to call OriginVerifier#start or whose validatin is not yet finished.
     */
    @Nullable
    private Set<Origin> mPendingOrigins = Collections.synchronizedSet(new HashSet<>());

    interface OriginVerificationListener {
        void onResult(boolean verified);
    }

    private WebLayerOriginVerificationScheduler(String packageName, Set<Origin> pendingOrigins) {
        mOriginVerifier = new WebLayerOriginVerifier(packageName, OriginVerifier.HANDLE_ALL_URLS,
                WebLayerVerificationResultStore.getInstance());
        mPendingOrigins = pendingOrigins;
    }

    /**
     * Initializes the WebLayerOriginVerificationScheduler.
     * This should be called exactly only once as it parses the AndroidManifest and statement list
     * to initialize the {@code mPendingOrigins}.
     *
     * @param context a context associated with an Activity/Service to load resources.
     */
    static void init(String packageName, Context context) {
        ThreadUtils.assertOnUiThread();

        sInstance = new WebLayerOriginVerificationScheduler(
                packageName, getClaimedOriginsFromManifest(packageName, context));
    }

    static WebLayerOriginVerificationScheduler getInstance() {
        assert sInstance != null : "Call to `init(String packageName, Context context)` missing";

        return sInstance;
    }

    void verify(String url, ProfileImpl profile, OriginVerificationListener listener) {
        ThreadUtils.assertOnUiThread();
        Origin origin = Origin.create(url);
        if (origin == null) {
            listener.onResult(false);
            return;
        }
        if (mOriginVerifier.skipOriginVerification()) {
            listener.onResult(true);
            return;
        }

        String urlScheme = origin.uri().getScheme();
        if (!urlScheme.equals(HTTPS_SCHEME) && !urlScheme.equals(HTTP_SCHEME)) {
            listener.onResult(true);
            return;
        }

        if (mPendingOrigins.contains(origin)) {
            mOriginVerifier.start((packageName, unused, verified, online) -> {
                mPendingOrigins.remove(origin);

                listener.onResult(verified);
            }, profile, origin);
            return;
        }
        listener.onResult(mOriginVerifier.wasPreviouslyVerified(origin));
    }

    private static Set<Origin> getClaimedOriginsFromManifest(String packageName, Context context) {
        try {
            Bundle metaData = context.getPackageManager()
                                      .getApplicationInfo(packageName, PackageManager.GET_META_DATA)
                                      .metaData;
            int statementsStringId;
            if (metaData == null
                    || (statementsStringId = metaData.getInt(ASSET_STATEMENTS_METADATA_KEY)) == 0) {
                return new HashSet<>();
            }

            // Application context cannot access hosting apps resources.
            String assetStatement = context.getResources().getString(statementsStringId);
            JSONArray statements = new JSONArray(assetStatement);

            if (statements == null) {
                return new HashSet<>();
            }

            Set<Origin> parsedOrigins = new HashSet<>();
            for (int i = 0; i < statements.length(); i++) {
                JSONObject statement = statements.getJSONObject(i);
                // TODO(swestphal): Check if lower/upper case is important.

                JSONArray relations = statement.getJSONArray(STATEMENT_RELATION_KEY);
                boolean foundRelation = false;
                for (int j = 0; j < relations.length(); j++) {
                    String relation = relations.getString(j).toString().replace("\\/", "/");
                    if (relation.equals(OriginVerifier.HANDLE_ALL_URLS)) {
                        foundRelation = true;
                        break;
                    }
                }
                if (!foundRelation) continue;

                JSONObject statementTarget = statement.getJSONObject(STATEMENT_TARGET_KEY);
                if (statementTarget.getString(STATEMENT_NAMESPACE_KEY).equals("web")) {
                    parsedOrigins.add(Origin.create(
                            statementTarget.getString(STATEMENT_SITE_KEY).replace("\\/", "/")));
                }
            }
            return parsedOrigins;
        } catch (PackageManager.NameNotFoundException e) {
            Log.w(TAG,
                    "Failed to read claimed origins from Manifest; "
                            + "PackageManager.NameNotFoundException raised");
        } catch (JSONException e) {
            Log.w(TAG, "Failed to read claimed origins from Manifest, failed to parse JSON");
        } catch (NotFoundException e) {
            Log.w(TAG, "Failed to read claimed origins from Manifest, invalid json content");
        }
        return new HashSet<>();
    }
}