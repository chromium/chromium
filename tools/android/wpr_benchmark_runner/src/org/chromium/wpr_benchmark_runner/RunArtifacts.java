// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import android.content.Context;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Comparator;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.stream.Stream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

@NullMarked
final class RunArtifacts implements AutoCloseable {
    private static final String TAG = "WprBenchmark";

    private final Context mContext;
    private final Path mDirectory;
    private final @Nullable Path mResultsDirectory;
    private final Map<String, File> mFiles = new LinkedHashMap<>();

    RunArtifacts(Context context, @Nullable String resultsDirectory) throws IOException {
        mContext = context;
        if (resultsDirectory == null) {
            mResultsDirectory = null;
        } else {
            mResultsDirectory = new File(resultsDirectory).toPath().toAbsolutePath().normalize();
        }
        Path parent = new File(context.getFilesDir(), "wpr_runs").toPath();
        Files.createDirectories(parent);
        mDirectory = Files.createTempDirectory(parent, "run-");
    }

    JSONObject prepareDependencies(Deadline deadline) throws IOException, JSONException {
        try (ZipInputStream input =
                new ZipInputStream(mContext.getAssets().open("wpr_benchmark.zip"))) {
            ZipEntry entry;
            while ((entry = input.getNextEntry()) != null) {
                deadline.remainingMillis();
                if (entry.isDirectory()) {
                    continue;
                }
                Path file = mDirectory.resolve(entry.getName()).normalize();
                if (!file.startsWith(mDirectory)) {
                    throw new IOException("Unsafe asset path");
                }
                Files.createDirectories(file.getParent());
                try (OutputStream output = new FileOutputStream(file.toFile())) {
                    copy(input, output, deadline);
                }
            }
        }
        JSONObject definition = readObject(mDirectory.resolve("config.json").toFile());
        JSONObject bindings = definition.getJSONObject("files");
        Iterator<String> aliases = bindings.keys();
        while (aliases.hasNext()) {
            String name = aliases.next();
            JSONObject binding = bindings.getJSONObject(name);
            File file;
            if (binding.getBoolean("executable")) {
                file =
                        new File(
                                mContext.getApplicationInfo().nativeLibraryDir,
                                binding.getString("native_library"));
            } else {
                file = mDirectory.resolve(binding.getString("relative_path")).toFile();
            }
            mFiles.put(name, file);
        }
        return definition;
    }

    Map<String, File> files() {
        return mFiles;
    }

    File directory() {
        return mDirectory.toFile();
    }

    File cdpRelay() {
        return new File(mContext.getApplicationInfo().nativeLibraryDir, "libcdp_relay.so");
    }

    File traceProcessor() {
        return new File(
                mContext.getApplicationInfo().nativeLibraryDir, "libtrace_processor_shell.so");
    }

    File temporaryFile(String prefix, String suffix) throws IOException {
        Path scratch = mDirectory.resolve("scratch");
        Files.createDirectories(scratch);
        return Files.createTempFile(scratch, prefix, suffix).toFile();
    }

    boolean hasResultsDirectory() {
        return mResultsDirectory != null;
    }

    void writeOutput(String filename, Object value, Deadline deadline)
            throws IOException, JSONException {
        Path directory = mResultsDirectory;
        if (directory == null) {
            throw new IllegalStateException("File output is not enabled");
        }
        Path destination = directory.resolve(filename).normalize();
        if (!destination.startsWith(directory) || destination.equals(directory)) {
            throw new IllegalArgumentException("Output must be relative to results-dir");
        }
        Files.createDirectories(destination.getParent());
        try (FileOutputStream output = new FileOutputStream(destination.toFile())) {
            if (value instanceof TraceArtifact) {
                try (InputStream input = new FileInputStream(((TraceArtifact) value).mFile)) {
                    copy(input, output, deadline);
                }
            } else {
                deadline.remainingMillis();
                output.write((json(value) + "\n").getBytes(StandardCharsets.UTF_8));
            }
        }
    }

    static void writeJson(File file, Object value) throws IOException, JSONException {
        try (FileOutputStream output = new FileOutputStream(file)) {
            output.write((json(value) + "\n").getBytes(StandardCharsets.UTF_8));
        }
    }

    static JSONObject readObject(File file) throws IOException, JSONException {
        try (InputStream input = new FileInputStream(file)) {
            return new JSONObject(readText(input));
        }
    }

    static String readText(InputStream input) throws IOException {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        byte[] buffer = new byte[8192];
        int size;
        while ((size = input.read(buffer)) != -1) {
            output.write(buffer, 0, size);
        }
        return output.toString(StandardCharsets.UTF_8.name());
    }

    private static void copy(InputStream input, OutputStream output, Deadline deadline)
            throws IOException {
        byte[] buffer = new byte[64 * 1024];
        int size;
        while ((size = input.read(buffer)) != -1) {
            deadline.remainingMillis();
            output.write(buffer, 0, size);
        }
    }

    private static String json(Object value) throws JSONException {
        if (value instanceof TraceProcessor.QueryResult) {
            value = ((TraceProcessor.QueryResult) value).toJson();
        } else if (value instanceof TraceArtifact) {
            TraceArtifact trace = (TraceArtifact) value;
            JSONObject metadata = new JSONObject();
            metadata.put("path", trace.mFile.getAbsolutePath());
            metadata.put("format", "perfetto_protobuf");
            value = metadata;
        }
        String wrappedJson = new JSONArray().put(value).toString();
        return wrappedJson.substring(1, wrappedJson.length() - 1);
    }

    static void logJson(String resultId, Object value, Deadline deadline)
            throws IOException, JSONException {
        String json = json(value);
        int part = 0;
        for (int start = 0; start < json.length(); ) {
            deadline.remainingMillis();
            int end = Math.min(start + 1000, json.length());
            if (end < json.length() && Character.isHighSurrogate(json.charAt(end - 1))) {
                end--;
            }
            boolean finalPart = end == json.length();
            Log.i(
                    TAG,
                    "WPR_RESULT id="
                            + resultId
                            + " part="
                            + part
                            + " final="
                            + finalPart
                            + " "
                            + json.substring(start, end));
            part++;
            start = end;
        }
    }

    @Override
    public void close() throws IOException {
        try (Stream<Path> paths = Files.walk(mDirectory)) {
            Iterator<Path> files = paths.sorted(Comparator.reverseOrder()).iterator();
            while (files.hasNext()) {
                Files.delete(files.next());
            }
        }
    }

    static final class TraceArtifact {
        final File mFile;

        TraceArtifact(File file) {
            mFile = file;
        }
    }
}
