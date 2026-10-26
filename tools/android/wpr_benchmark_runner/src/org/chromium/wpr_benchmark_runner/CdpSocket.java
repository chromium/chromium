// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import android.app.UiAutomation;
import android.os.Process;
import android.util.Base64;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.EOFException;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

@NullMarked
final class CdpSocket implements AutoCloseable {
    private static final String TAG = "WprBenchmark";

    static final long COMMAND_TIMEOUT_MS = 5000;
    private static final long CONNECT_TIMEOUT_MS = 10_000;
    private static final long CLEANUP_TIMEOUT_MS = 15_000;
    private static final int MAX_MESSAGE_BYTES = 16 * 1024 * 1024;
    private static final SecureRandom RANDOM = new SecureRandom();
    private static final int UIDS_PER_USER = 100_000;

    private final UiAutomation mAutomation;
    private final String mBrowserPackage;
    private final File mRelayExecutable;
    private final String mSocketName;
    private final List<String> mOwnedTargets = new ArrayList<>();
    private @Nullable Session mBrowser;
    private @Nullable Session mPage;
    private @Nullable Session mTrace;
    private boolean mTracing;

    CdpSocket(
            UiAutomation automation,
            String browserPackage,
            File relayExecutable,
            String socketName) {
        mAutomation = automation;
        mBrowserPackage = browserPackage;
        mRelayExecutable = relayExecutable;
        mSocketName = socketName;
    }

    Session browser(Deadline deadline) throws Exception {
        if (mBrowser == null) {
            mBrowser = connect("/devtools/browser", deadline);
        }
        return mBrowser;
    }

    Session page() {
        if (mPage == null) {
            throw new IllegalStateException(
                    "Create a tab with target '_new_tab' before running page actions");
        }
        return mPage;
    }

    Session createPage(String url, Deadline deadline) throws Exception {
        Session browser = browser(deadline);
        JSONObject params = new JSONObject();
        params.put("url", url);
        params.put("background", false);
        JSONObject result = browser.send("Target.createTarget", params, deadline);
        String targetId = result.getString("targetId");
        mOwnedTargets.add(targetId);
        return selectPage(targetId, deadline);
    }

    private Session selectPage(String target, Deadline deadline) throws Exception {
        if (mPage != null) {
            mPage.close();
        }
        mPage = null;
        Session page = connect("/devtools/page/" + target, deadline);
        mPage = page;
        page.send("Page.enable", deadline);
        page.send("Page.bringToFront", deadline);
        return page;
    }

    Object evaluate(String body, boolean requireValue, boolean readOnly, Deadline deadline)
            throws Exception {
        JSONObject params = new JSONObject();
        params.put("expression", "(function(){" + body + "\n})()");
        params.put("returnByValue", true);
        params.put("awaitPromise", true);
        params.put("timeout", deadline.remainingMillis());
        JSONObject reply = page().request("Runtime.evaluate", params, deadline);
        JSONObject error = reply.optJSONObject("error");
        if (error != null) {
            String message = error.getString("message");
            if (readOnly && isContextLoss(message)) {
                return JSONObject.NULL;
            }
            throw new IOException("CDP Runtime.evaluate failed: " + error);
        }
        JSONObject evaluation = reply.getJSONObject("result");
        JSONObject exception = evaluation.optJSONObject("exceptionDetails");
        if (exception != null) {
            throw new IOException("JavaScript failed: " + exception);
        }
        JSONObject result = evaluation.getJSONObject("result");
        if (!requireValue) {
            return JSONObject.NULL;
        }
        if (!result.has("value")) {
            throw new IOException("JavaScript must return a JSON-compatible value: " + result);
        }
        return result.get("value");
    }

    private static boolean isContextLoss(String message) {
        return message.equals("Cannot find default execution context")
                || message.equals("Cannot find context with specified id")
                || message.equals("Execution context was destroyed.");
    }

    void startTrace(JSONObject traceConfig, Deadline deadline) throws Exception {
        if (mTracing) {
            throw new IllegalStateException("Trace already started");
        }
        if (mTrace == null) {
            mTrace = connect("/devtools/browser", deadline);
        }
        JSONObject params = new JSONObject();
        params.put("transferMode", "ReturnAsStream");
        params.put("streamFormat", "proto");
        params.put("tracingBackend", "chrome");
        params.put("traceConfig", traceConfig);
        mTrace.send("Tracing.start", params, deadline);
        mTracing = true;
    }

    RunArtifacts.TraceArtifact stopTrace(File output, Deadline deadline) throws Exception {
        Session trace = mTrace;
        if (trace == null || !mTracing) {
            throw new IllegalStateException("Trace not started");
        }
        JSONObject complete = endTrace(trace, deadline);
        String handle = complete.getString("stream");
        try {
            if (complete.optBoolean("dataLossOccurred")) {
                throw new IOException("Chrome reported trace data loss");
            }
            try (FileOutputStream file = new FileOutputStream(output)) {
                while (true) {
                    JSONObject chunk =
                            trace.send(
                                    "IO.read",
                                    new JSONObject().put("handle", handle).put("size", 256 * 1024),
                                    deadline);
                    String data = chunk.getString("data");
                    byte[] bytes;
                    if (chunk.optBoolean("base64Encoded")) {
                        bytes = Base64.decode(data, Base64.DEFAULT);
                    } else {
                        bytes = data.getBytes(StandardCharsets.UTF_8);
                    }
                    file.write(bytes);
                    if (chunk.getBoolean("eof")) {
                        break;
                    }
                }
                file.getFD().sync();
            }
        } catch (Exception failure) {
            try {
                trace.send(
                        "IO.close",
                        new JSONObject().put("handle", handle),
                        Deadline.after(CLEANUP_TIMEOUT_MS));
            } catch (Exception cleanup) {
                failure.addSuppressed(cleanup);
            }
            throw failure;
        }
        trace.send("IO.close", new JSONObject().put("handle", handle), deadline);
        if (output.length() == 0) {
            throw new IOException("Chrome returned an empty trace");
        }
        return new RunArtifacts.TraceArtifact(output);
    }

    private JSONObject endTrace(Session trace, Deadline deadline) throws Exception {
        trace.send("Tracing.end", deadline);
        JSONObject complete = trace.awaitEvent("Tracing.tracingComplete", deadline);
        mTracing = false;
        return complete;
    }

    private Session connect(String path, Deadline deadline) throws Exception {
        Deadline connection = deadline.limit(CONNECT_TIMEOUT_MS);
        while (true) {
            ShellProcess relay =
                    new ShellProcess(
                            mAutomation,
                            "run-as "
                                    + mBrowserPackage
                                    + " --user "
                                    + Process.myUid() / UIDS_PER_USER
                                    + " "
                                    + mRelayExecutable.getAbsolutePath()
                                    + " "
                                    + mSocketName);
            try {
                Session session = new Session(relay);
                session.upgrade(path, connection);
                return session;
            } catch (Exception error) {
                try {
                    relay.close();
                } catch (IOException cleanup) {
                    error.addSuppressed(cleanup);
                }
                if (!(error instanceof IOException)) {
                    throw error;
                }
                try {
                    connection.pause(100);
                } catch (IOException timeout) {
                    timeout.addSuppressed(error);
                    throw timeout;
                }
            }
        }
    }

    @Override
    public void close() {
        Deadline cleanup = Deadline.after(CLEANUP_TIMEOUT_MS);
        Session trace = mTrace;
        if (trace != null && mTracing) {
            try {
                JSONObject complete = endTrace(trace, cleanup);
                trace.send(
                        "IO.close",
                        new JSONObject().put("handle", complete.getString("stream")),
                        cleanup);
            } catch (Exception error) {
                Log.w(TAG, "Trace cleanup failed", error);
            }
        }
        Session browser = mBrowser;
        if (browser != null) {
            for (String target : mOwnedTargets) {
                try {
                    browser.send(
                            "Target.closeTarget",
                            new JSONObject().put("targetId", target),
                            cleanup);
                } catch (Exception error) {
                    Log.w(TAG, "Tab cleanup failed", error);
                    break;
                }
            }
        }
        for (Session session : new Session[] {mPage, mTrace, mBrowser}) {
            if (session == null) {
                continue;
            }
            try {
                session.close();
            } catch (IOException error) {
                Log.w(TAG, "CDP socket cleanup failed", error);
            }
        }
        mPage = null;
        mTrace = null;
        mBrowser = null;
        mTracing = false;
        mOwnedTargets.clear();
    }

    static final class Session implements AutoCloseable {
        private static final int CONTINUATION_FRAME = 0;
        private static final int TEXT_FRAME = 1;
        private static final int CLOSE_FRAME = 8;
        private static final int PING_FRAME = 9;
        private static final int PONG_FRAME = 10;

        private final ShellProcess mRelay;
        private final ArrayDeque<JSONObject> mEvents = new ArrayDeque<>();
        private int mNextId;

        Session(ShellProcess relay) {
            mRelay = relay;
        }

        JSONObject send(String method, Deadline deadline) throws Exception {
            return send(method, null, deadline);
        }

        JSONObject send(String method, @Nullable JSONObject params, Deadline deadline)
                throws Exception {
            JSONObject reply = request(method, params, deadline.limit(COMMAND_TIMEOUT_MS));
            if (reply.has("error")) {
                throw new IOException("CDP " + method + ": " + reply.get("error"));
            }
            return reply.getJSONObject("result");
        }

        JSONObject request(String method, @Nullable JSONObject params, Deadline deadline)
                throws IOException, JSONException {
            int id = ++mNextId;
            JSONObject command = new JSONObject().put("id", id).put("method", method);
            if (params != null) {
                command.put("params", params);
            }
            byte[] payload = command.toString().getBytes(StandardCharsets.UTF_8);
            writeFrame(TEXT_FRAME, payload, deadline);
            while (true) {
                JSONObject message = new JSONObject(readMessage(deadline));
                if (message.has("id")) {
                    if (message.getInt("id") != id) {
                        throw new IOException("Unexpected CDP response ID");
                    }
                    return message;
                }
                String event = message.optString("method");
                if ("Tracing.tracingComplete".equals(event)) {
                    if (mEvents.size() == 16) {
                        throw new IOException("CDP event queue overflow");
                    }
                    mEvents.add(message);
                }
            }
        }

        JSONObject awaitEvent(String method, Deadline deadline) throws IOException, JSONException {
            while (true) {
                JSONObject event;
                if (mEvents.isEmpty()) {
                    event = new JSONObject(readMessage(deadline));
                } else {
                    event = mEvents.removeFirst();
                }
                if (method.equals(event.optString("method"))) {
                    return event.getJSONObject("params");
                }
                if (event.has("id")) {
                    throw new IOException("Unexpected CDP response while waiting");
                }
            }
        }

        void upgrade(String path, Deadline deadline) throws IOException {
            byte[] nonce = new byte[16];
            RANDOM.nextBytes(nonce);
            String key = Base64.encodeToString(nonce, Base64.NO_WRAP);
            String request =
                    "GET "
                            + path
                            + " HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\n"
                            + "Connection: Upgrade\r\nSec-WebSocket-Key: "
                            + key
                            + "\r\nSec-WebSocket-Version: 13\r\n\r\n";
            mRelay.write(request.getBytes(StandardCharsets.US_ASCII), deadline);
            ByteArrayOutputStream header = new ByteArrayOutputStream();
            int ending = 0;
            while (ending != 0x0d0a0d0a) {
                if (header.size() == 32 * 1024) {
                    throw new IOException("WebSocket header too large");
                }
                int value = readByte(deadline);
                header.write(value);
                ending = (ending << 8) | value;
            }
            String headers = header.toString(StandardCharsets.US_ASCII.name());
            if (!headers.startsWith("HTTP/1.1 101 ")) {
                throw new IOException("WebSocket upgrade failed: " + headers.split("\r\n", 2)[0]);
            }
            String accept;
            try {
                accept =
                        Base64.encodeToString(
                                MessageDigest.getInstance("SHA-1")
                                        .digest(
                                                (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11")
                                                        .getBytes(StandardCharsets.US_ASCII)),
                                Base64.NO_WRAP);
            } catch (NoSuchAlgorithmException error) {
                throw new IOException(error);
            }
            boolean accepted = false;
            for (String line : headers.split("\r\n")) {
                if (line.toLowerCase(Locale.ROOT).startsWith("sec-websocket-accept:")) {
                    accepted = accept.equals(line.substring(line.indexOf(':') + 1).trim());
                }
            }
            if (!accepted) {
                throw new IOException("Invalid WebSocket accept header");
            }
        }

        private String readMessage(Deadline deadline) throws IOException {
            ByteArrayOutputStream text = new ByteArrayOutputStream();
            boolean fragmented = false;
            try {
                while (true) {
                    int firstByte = readByte(deadline);
                    int secondByte = readByte(deadline);
                    int opcode = firstByte & 15;
                    boolean complete = (firstByte & 128) != 0;
                    long length = secondByte & 127;
                    if ((firstByte & 112) != 0 || (secondByte & 128) != 0) {
                        throw new IOException("Unsupported WebSocket frame");
                    }
                    if (length == 126) {
                        length = readUnsigned(2, deadline);
                    } else if (length == 127) {
                        length = readUnsigned(8, deadline);
                    }
                    if (length < 0 || length > MAX_MESSAGE_BYTES) {
                        throw new IOException("WebSocket frame too large");
                    }
                    byte[] payload = readFully((int) length, deadline);
                    if (opcode >= CLOSE_FRAME) {
                        if (!complete || length > 125) {
                            throw new IOException("Invalid control frame");
                        }
                        if (opcode == CLOSE_FRAME) {
                            throw new EOFException("DevTools socket closed");
                        }
                        if (opcode == PING_FRAME) {
                            writeFrame(PONG_FRAME, payload, deadline);
                        } else if (opcode != PONG_FRAME) {
                            throw new IOException("Unknown WebSocket control frame");
                        }
                        continue;
                    }
                    if (opcode == TEXT_FRAME && !fragmented) {
                        fragmented = !complete;
                    } else if (opcode != CONTINUATION_FRAME || !fragmented) {
                        throw new IOException("Unexpected WebSocket continuation");
                    }
                    if ((long) text.size() + payload.length > MAX_MESSAGE_BYTES) {
                        throw new IOException("WebSocket message too large");
                    }
                    text.write(payload);
                    if (complete) {
                        return text.toString(StandardCharsets.UTF_8.name());
                    }
                }
            } catch (IOException error) {
                try {
                    close();
                } catch (IOException cleanup) {
                    error.addSuppressed(cleanup);
                }
                throw error;
            }
        }

        private void writeFrame(int opcode, byte[] payload, Deadline deadline) throws IOException {
            ByteArrayOutputStream header = new ByteArrayOutputStream();
            DataOutputStream output = new DataOutputStream(header);
            output.writeByte(128 | opcode);
            if (payload.length <= 125) {
                output.writeByte(128 | payload.length);
            } else if (payload.length <= 65535) {
                output.writeByte(128 | 126);
                output.writeShort(payload.length);
            } else {
                output.writeByte(128 | 127);
                output.writeLong(payload.length);
            }
            byte[] mask = new byte[4];
            RANDOM.nextBytes(mask);
            output.write(mask);
            mRelay.write(header.toByteArray(), deadline);
            byte[] masked = new byte[payload.length];
            for (int index = 0; index < payload.length; index++) {
                masked[index] = (byte) (payload[index] ^ mask[index % 4]);
            }
            mRelay.write(masked, deadline);
        }

        private int readByte(Deadline deadline) throws IOException {
            return mRelay.readByte(deadline);
        }

        private long readUnsigned(int bytes, Deadline deadline) throws IOException {
            long value = 0;
            for (int index = 0; index < bytes; index++) {
                int nextByte = readByte(deadline);
                value = (value << 8) | nextByte;
            }
            return value;
        }

        private byte[] readFully(int length, Deadline deadline) throws IOException {
            byte[] bytes = new byte[length];
            for (int offset = 0; offset < length; ) {
                int size = mRelay.read(bytes, offset, length - offset, deadline);
                if (size == -1) {
                    throw new EOFException("Incomplete DevTools frame");
                }
                offset += size;
            }
            return bytes;
        }

        @Override
        public void close() throws IOException {
            mRelay.close();
        }
    }
}
