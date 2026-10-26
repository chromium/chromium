// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import android.Manifest;
import android.app.UiAutomation;
import android.content.Context;
import android.content.Intent;
import android.os.Process;
import android.provider.Settings;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

@NullMarked
final class BrowserController implements AutoCloseable {
    private static final String FLAGS_FILE = "/data/local/tmp/chrome-command-line";
    private static final long SHELL_TIMEOUT_MS = 10_000;
    private static final long CLEANUP_TIMEOUT_MS = 15_000;
    private static final int UIDS_PER_USER = 100_000;
    private static final String[] BROWSER_PERMISSIONS = {
        Manifest.permission.POST_NOTIFICATIONS,
        Manifest.permission.CAMERA,
        Manifest.permission.RECORD_AUDIO,
    };

    private final Context mContext;
    private final UiAutomation mAutomation;
    private final String mPackage;
    private final String mCommandLine;
    private final String mBackupDirectory = "/data/local/tmp/wpr-browser-" + UUID.randomUUID();
    private @Nullable String mPreviousDebugApp;
    private @Nullable String mPreviousWaitForDebugger;
    private boolean mBackupCreated;
    private boolean mStateSaved;

    BrowserController(Context context, UiAutomation automation, JSONObject browser)
            throws JSONException {
        mContext = context;
        mAutomation = automation;
        mPackage = browser.getString("package");
        JSONArray flags = browser.getJSONArray("flags");
        StringBuilder commandLine = new StringBuilder("chrome");
        for (int index = 0; index < flags.length(); index++) {
            commandLine.append(' ').append(quoteFlag(flags.getString(index)));
        }
        String host = browser.getString("replay_host");
        String routing =
                "--host-resolver-rules=MAP *:80 "
                        + host
                        + ":"
                        + browser.getInt("http_port")
                        + ",MAP *:443 "
                        + host
                        + ":"
                        + browser.getInt("https_port")
                        + ",EXCLUDE localhost";
        mCommandLine = commandLine.append(' ').append(quoteFlag(routing)).toString();
    }

    void start(Deadline deadline) throws Exception {
        Intent launch = mContext.getPackageManager().getLaunchIntentForPackage(mPackage);
        if (launch == null) {
            throw new IllegalStateException("Browser is not installed: " + mPackage);
        }
        mPreviousDebugApp =
                Settings.Global.getString(mContext.getContentResolver(), Settings.Global.DEBUG_APP);
        mPreviousWaitForDebugger =
                Settings.Global.getString(
                        mContext.getContentResolver(), Settings.Global.WAIT_FOR_DEBUGGER);
        shell("mkdir -m 700 " + quote(mBackupDirectory), deadline);
        mBackupCreated = true;
        String original = quote(mBackupDirectory + "/original-flags");
        shell(
                "if [ -e "
                        + FLAGS_FILE
                        + " ] || [ -L "
                        + FLAGS_FILE
                        + " ]; then cp -pP "
                        + FLAGS_FILE
                        + " "
                        + original
                        + "; fi",
                deadline);
        mStateSaved = true;
        shell("am force-stop " + quote(mPackage), deadline);
        int user = Process.myUid() / UIDS_PER_USER;
        shell("pm clear --user " + user + " " + quote(mPackage), deadline);
        for (String permission : BROWSER_PERMISSIONS) {
            shell("pm grant --user " + user + " " + quote(mPackage) + " " + permission, deadline);
        }
        String temporary = quote(mBackupDirectory + "/benchmark-flags");
        shell(
                "printf '%s\\n' "
                        + quote(mCommandLine)
                        + " > "
                        + temporary
                        + " && chmod 644 "
                        + temporary
                        + " && mv "
                        + temporary
                        + " "
                        + FLAGS_FILE
                        + " && am set-debug-app --persistent "
                        + quote(mPackage),
                deadline);
        launch.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(launch);
    }

    private void shell(String command, Deadline runDeadline) throws Exception {
        Deadline deadline = runDeadline.limit(SHELL_TIMEOUT_MS);
        try (ShellProcess process = new ShellProcess(mAutomation, "sh")) {
            String script = "(\n" + command + "\n) 2>&1\nprintf '\\n%s' \"$?\"\n";
            process.write(script.getBytes(StandardCharsets.UTF_8), deadline);
            process.finishInput();
            ByteArrayOutputStream output = new ByteArrayOutputStream();
            byte[] buffer = new byte[4096];
            int size;
            while ((size = process.read(buffer, 0, buffer.length, deadline)) != -1) {
                output.write(buffer, 0, size);
            }
            String result = output.toString(StandardCharsets.UTF_8.name());
            int separator = result.lastIndexOf('\n');
            if (separator == -1 || !result.substring(separator + 1).equals("0")) {
                throw new IOException("Shell command failed: " + command + "\n" + result);
            }
        }
    }

    private static String quote(String value) {
        return "'" + value.replace("'", "'\\''") + "'";
    }

    private static String quoteFlag(String flag) {
        int end = flag.length();
        while (end > 0 && flag.charAt(end - 1) == '\\') {
            end--;
        }
        return "\"" + flag.substring(0, end).replace("\"", "\\\"") + "\"" + flag.substring(end);
    }

    private static String restoreSetting(String name, @Nullable String value) {
        if (value == null) {
            return "settings delete global " + name;
        }
        return "settings put global " + name + " " + quote(value);
    }

    @Override
    public void close() throws Exception {
        if (!mBackupCreated) {
            return;
        }
        Deadline deadline = Deadline.after(CLEANUP_TIMEOUT_MS);
        List<String> commands = new ArrayList<>();
        String original = quote(mBackupDirectory + "/original-flags");
        if (mStateSaved) {
            commands.add("am force-stop " + quote(mPackage));
            commands.add(
                    "if [ -e "
                            + original
                            + " ] || [ -L "
                            + original
                            + " ]; then mv "
                            + original
                            + " "
                            + FLAGS_FILE
                            + "; else rm -f "
                            + FLAGS_FILE
                            + "; fi");
            String previous = mPreviousDebugApp;
            if (previous == null || previous.isEmpty()) {
                commands.add("am clear-debug-app");
            } else {
                String wait = "1".equals(mPreviousWaitForDebugger) ? "-w " : "";
                commands.add("am set-debug-app --persistent " + wait + quote(previous));
            }
            commands.add(restoreSetting(Settings.Global.DEBUG_APP, mPreviousDebugApp));
            commands.add(
                    restoreSetting(Settings.Global.WAIT_FOR_DEBUGGER, mPreviousWaitForDebugger));
        }
        @Nullable Exception failure = null;
        for (String command : commands) {
            try {
                shell(command, deadline);
            } catch (Exception error) {
                if (failure == null) {
                    failure = error;
                } else {
                    failure.addSuppressed(error);
                }
            }
        }
        if (failure != null) {
            throw failure;
        }
        shell(
                "rm -f "
                        + original
                        + " "
                        + quote(mBackupDirectory + "/benchmark-flags")
                        + " && rmdir "
                        + quote(mBackupDirectory),
                deadline);
        mBackupCreated = false;
    }
}
