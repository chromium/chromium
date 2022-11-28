// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.rules.TestWatcher;
import org.junit.runner.Description;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageUtils;
import org.chromium.net.test.util.TestWebServer;

import java.util.List;

/**
 * Gives an option to make DAL verification pass for the tests.
 * Also exposes the TestWebServer instance for more customization.
 */
public class DigitalAssetLinksServerRule extends TestWatcher {
    private static final String ASSETLINKS_PATH = "/.well-known/assetlinks.json";

    // TODO(crbug.com/1376522): Figure out how to not hardcode a port number.
    private static final int PORT = 8888;
    private TestWebServer mServer;

    @Override
    protected void starting(Description desc) {
        super.starting(desc);
        try {
            mServer = TestWebServer.start(PORT);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
        // By default, the asset links are not set up.
        mServer.setResponseWithNotFoundStatus(ASSETLINKS_PATH, null);
    }

    @Override
    protected void finished(Description desc) {
        super.finished(desc);
        mServer.shutdown();
    }

    // Returns the TestServer in case you want to add additional handlers.
    public TestWebServer getServer() {
        return mServer;
    }

    // Makes the DAL verification succeed.
    public void setUpDigitalAssetLinks() {
        String packageName = ContextUtils.getApplicationContext().getPackageName();
        List<String> signatureFingerprints =
                PackageUtils.getCertificateSHA256FingerprintForPackage(packageName);
        mServer.setResponse(
                ASSETLINKS_PATH, makeAssetFile(packageName, signatureFingerprints.get(0)), null);
    }

    private static String makeAssetFile(String packageName, String fingerprint) {
        try {
            return (new JSONArray().put(
                            new JSONObject()
                                    .put("relation",
                                            new JSONArray().put(
                                                    "delegate_permission/common.handle_all_urls"))
                                    .put("target",
                                            new JSONObject()
                                                    .put("namespace", "android_app")
                                                    .put("package_name", packageName)
                                                    .put("sha256_cert_fingerprints",
                                                            new JSONArray().put(fingerprint)))))
                    .toString();
        } catch (JSONException e) {
        }
        return "";
    }
}
