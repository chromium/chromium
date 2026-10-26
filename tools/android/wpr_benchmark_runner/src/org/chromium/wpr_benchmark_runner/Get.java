// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import org.json.JSONObject;

import org.chromium.build.annotations.NullMarked;

import java.io.IOException;

@NullMarked
final class Get implements Action {
    private final CdpSocket mCdpSocket;
    private final Deadline mDeadline;
    private final String mUrl;
    private final String mTarget;
    private final String mReadyState;
    private final String mUrlContains;
    private final long mTimeoutMillis;

    Get(
            CdpSocket cdp,
            Deadline deadline,
            String url,
            String target,
            String readyState,
            String urlContains,
            long timeoutMillis) {
        mCdpSocket = cdp;
        mDeadline = deadline;
        mUrl = url;
        mTarget = target;
        mReadyState = readyState;
        mUrlContains = urlContains;
        mTimeoutMillis = timeoutMillis;
    }

    @Override
    public void run() throws Exception {
        Deadline deadline = mDeadline.limit(mTimeoutMillis);
        CdpSocket.Session page;
        String loaderId = "";
        if (mTarget.equals("_new_tab")) {
            page = mCdpSocket.createPage(mUrl, deadline);
        } else {
            page = mCdpSocket.page();
            JSONObject navigation =
                    page.send("Page.navigate", new JSONObject().put("url", mUrl), deadline);
            if (navigation.has("errorText")) {
                throw new IOException("Navigation failed: " + navigation.getString("errorText"));
            }
            loaderId = navigation.optString("loaderId");
        }
        if (mReadyState.equals("any")) {
            return;
        }
        String readyCondition;
        if (mReadyState.equals("complete")) {
            readyCondition = "document.readyState === 'complete'";
        } else {
            readyCondition = "document.readyState !== 'loading'";
        }
        String condition =
                "return "
                        + readyCondition
                        + " && String(location.href).includes("
                        + JSONObject.quote(mUrlContains)
                        + ");";
        while (true) {
            boolean currentLoader = true;
            if (!loaderId.isEmpty()) {
                JSONObject frameTree = page.send("Page.getFrameTree", deadline);
                JSONObject frame = frameTree.getJSONObject("frameTree").getJSONObject("frame");
                currentLoader = loaderId.equals(frame.optString("loaderId"));
            }
            if (currentLoader) {
                Object ready =
                        mCdpSocket.evaluate(
                                condition,
                                true,
                                true,
                                deadline.limit(CdpSocket.COMMAND_TIMEOUT_MS));
                if (Boolean.TRUE.equals(ready)) {
                    return;
                }
            }
            deadline.pause(100);
        }
    }
}
