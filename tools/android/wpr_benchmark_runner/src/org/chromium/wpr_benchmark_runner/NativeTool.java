// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

@NullMarked
final class NativeTool implements AutoCloseable {
    private static final String TAG = "WprNativeTool";

    private static final int MAX_DIAGNOSTIC_CHARS = 16 * 1024;
    private final Process mProcess;
    private final List<Thread> mReaders = new ArrayList<>();
    private final StringBuilder mDiagnostics = new StringBuilder();
    private volatile @Nullable IOException mReadFailure;

    NativeTool(List<String> command, File directory, @Nullable File stdoutFile) throws IOException {
        ProcessBuilder builder = new ProcessBuilder(command).directory(directory);
        if (stdoutFile != null) {
            builder.redirectOutput(stdoutFile);
        }
        mProcess = builder.start();
        try {
            mProcess.getOutputStream().close();
            if (stdoutFile == null) {
                drain(mProcess.getInputStream());
            }
            drain(mProcess.getErrorStream());
        } catch (IOException | RuntimeException error) {
            try {
                close();
            } catch (IOException cleanup) {
                error.addSuppressed(cleanup);
            }
            throw error;
        }
    }

    private void drain(InputStream stream) {
        Thread reader = new Thread(() -> readOutput(stream), "WprNativeTool-output");
        reader.setDaemon(true);
        mReaders.add(reader);
        reader.start();
    }

    private void readOutput(InputStream stream) {
        try (InputStreamReader input = new InputStreamReader(stream, StandardCharsets.UTF_8)) {
            char[] buffer = new char[4096];
            int size;
            while ((size = input.read(buffer)) != -1) {
                String text = new String(buffer, 0, size);
                synchronized (mDiagnostics) {
                    mDiagnostics.append(text);
                    int excess = mDiagnostics.length() - MAX_DIAGNOSTIC_CHARS;
                    if (excess > 0) {
                        mDiagnostics.delete(0, excess);
                    }
                }
                Log.i(TAG, text);
            }
        } catch (IOException error) {
            mReadFailure = error;
        }
    }

    void checkRunning() throws IOException {
        IOException readFailure = mReadFailure;
        if (readFailure != null) {
            throw readFailure;
        }
        if (!mProcess.isAlive()) {
            throw new IOException(
                    "Native process exited with " + mProcess.exitValue() + ": " + diagnostics());
        }
    }

    void awaitExit(Deadline deadline) throws IOException, InterruptedException {
        if (!mProcess.waitFor(deadline.remainingMillis(), TimeUnit.MILLISECONDS)) {
            throw new IOException("Native process timed out");
        }
        for (Thread reader : mReaders) {
            reader.join(deadline.remainingMillis());
            if (reader.isAlive()) {
                throw new IOException("Native output drain timed out");
            }
        }
        IOException readFailure = mReadFailure;
        if (readFailure != null) {
            throw readFailure;
        }
        if (mProcess.exitValue() != 0) {
            throw new IOException(
                    "Native process exited with " + mProcess.exitValue() + ": " + diagnostics());
        }
    }

    private String diagnostics() {
        synchronized (mDiagnostics) {
            return mDiagnostics.toString();
        }
    }

    @Override
    public void close() throws IOException {
        try {
            if (mProcess.isAlive()) {
                mProcess.destroy();
                if (!mProcess.waitFor(500, TimeUnit.MILLISECONDS)) {
                    mProcess.destroyForcibly();
                    if (!mProcess.waitFor(1500, TimeUnit.MILLISECONDS)) {
                        throw new IOException("Native process did not terminate");
                    }
                }
            }
            mProcess.getInputStream().close();
            mProcess.getErrorStream().close();
            for (Thread reader : mReaders) {
                reader.join(500);
            }
        } catch (InterruptedException error) {
            mProcess.destroyForcibly();
            Thread.currentThread().interrupt();
            throw new IOException("Interrupted during native process cleanup", error);
        }
    }
}
