// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import org.chromium.build.annotations.NullMarked;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;

@NullMarked
final class WprReplayController implements AutoCloseable {
    private final NativeTool mTool;
    private final ActionFactory.ReplayConfig mConfig;

    WprReplayController(ActionFactory.ReplayConfig config, RunArtifacts artifacts)
            throws IOException {
        mConfig = config;
        mTool = new NativeTool(config.mCommand, artifacts.directory(), null);
    }

    void awaitReady(Deadline runDeadline) throws IOException, InterruptedException {
        Deadline deadline = runDeadline.limit(mConfig.mReadyTimeoutMillis);
        while (true) {
            mTool.checkRunning();
            try {
                checkListeners(deadline.limit(1000));
                return;
            } catch (IOException notReady) {
                deadline.pause(100);
            }
        }
    }

    private void checkListeners(Deadline deadline) throws IOException {
        try (Socket https = new Socket();
                Socket http = new Socket()) {
            https.connect(
                    new InetSocketAddress(mConfig.mHost, mConfig.mHttpsPort),
                    (int) deadline.remainingMillis());
            http.connect(
                    new InetSocketAddress(mConfig.mHost, mConfig.mHttpPort),
                    (int) deadline.remainingMillis());
            http.setSoTimeout((int) deadline.remainingMillis());
            String request =
                    "GET /web-page-replay-generate-200 HTTP/1.1\r\n"
                            + "Host: localhost\r\nConnection: close\r\n\r\n";
            http.getOutputStream().write(request.getBytes(StandardCharsets.US_ASCII));
            http.getOutputStream().flush();
            InputStreamReader response =
                    new InputStreamReader(http.getInputStream(), StandardCharsets.US_ASCII);
            BufferedReader reader = new BufferedReader(response);
            String status = reader.readLine();
            if (status == null || !status.startsWith("HTTP/1.1 200 ")) {
                throw new IOException("WPR health endpoint not ready");
            }
        }
    }

    @Override
    public void close() throws IOException {
        mTool.close();
    }
}
