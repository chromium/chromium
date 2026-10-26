// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.build.annotations.NullMarked;

import java.io.File;
import java.io.IOException;
import java.util.Arrays;
import java.util.List;

@NullMarked
final class ProcessLlp2Trace implements Action {
    private static final List<String> PAGES =
            Arrays.asList(
                    "amazon_product",
                    "cnn_article",
                    "wikipedia_article",
                    "globo_homepage",
                    "google_search_result");

    private final ActionFactory mFactory;
    private final TraceProcessor mProcessor;
    private final Deadline mDeadline;
    private final String mInput;
    private final String mOutput;
    private final File mQuery;
    private final long mTimeoutMillis;
    private final String mVersion;

    ProcessLlp2Trace(
            ActionFactory factory,
            TraceProcessor processor,
            Deadline deadline,
            String input,
            String output,
            File query,
            long timeoutMillis,
            String version) {
        mFactory = factory;
        mProcessor = processor;
        mDeadline = deadline;
        mInput = input;
        mOutput = output;
        mQuery = query;
        mTimeoutMillis = timeoutMillis;
        mVersion = version;
    }

    @Override
    public void run() throws Exception {
        RunArtifacts.TraceArtifact trace =
                mFactory.requireResult(mInput, RunArtifacts.TraceArtifact.class);
        TraceProcessor.QueryResult rows =
                mProcessor.query(trace, mQuery, mDeadline.limit(mTimeoutMillis));
        JSONObject scores = score(rows, mVersion);
        mFactory.putResult(mOutput, scores);
    }

    static JSONObject score(TraceProcessor.QueryResult result, String version)
            throws IOException, JSONException {
        int nameColumn = result.column("page");
        int startColumn = result.column("story_start");
        int visualColumn = result.column("visual_presentation");
        int interactiveColumn = result.column("interactive_presentation");
        JSONObject pages = new JSONObject();
        for (List<String> row : result.mRows) {
            String name = row.get(nameColumn);
            if (!PAGES.contains(name)) {
                throw new IOException("Unexpected LLP2 page: " + name);
            }
            if (pages.has(name)) {
                throw new IOException("Duplicate LLP2 page: " + name);
            }
            long start = timestamp(row.get(startColumn), "story_start", name);
            long visual = timestamp(row.get(visualColumn), "visual_presentation", name);
            long interactive =
                    timestamp(row.get(interactiveColumn), "interactive_presentation", name);
            if (visual <= start || interactive <= start) {
                throw new IOException("Invalid LLP2 presentation ordering for " + name);
            }
            long visualDuration = Math.subtractExact(visual, start);
            long interactiveDuration = Math.subtractExact(interactive, start);
            double coefficient = 1;
            if (name.equals("globo_homepage")) {
                coefficient = 0.58;
            }
            double visualScore = 60e9 / visualDuration;
            double rawInteractiveScore = 60e9 / interactiveDuration;
            JSONObject pageScores = new JSONObject();
            pageScores.put("story_start_ns", start);
            pageScores.put("visual_presentation_ns", visual);
            pageScores.put("interactive_presentation_ns", interactive);
            pageScores.put("visual_duration_ns", visualDuration);
            pageScores.put("interactive_duration_ns", interactiveDuration);
            pageScores.put("visual_score", visualScore);
            pageScores.put("raw_interactive_score", rawInteractiveScore);
            pageScores.put("interactive_coefficient", coefficient);
            pageScores.put("interactive_score", rawInteractiveScore * coefficient);
            pages.put(name, pageScores);
        }
        if (pages.length() != PAGES.size()) {
            throw new IOException("Incomplete LLP2 results: expected " + PAGES);
        }
        JSONObject ordered = new JSONObject();
        double logarithms = 0;
        for (String name : PAGES) {
            JSONObject page = pages.getJSONObject(name);
            ordered.put(name, page);
            logarithms += Math.log(page.getDouble("visual_score"));
            logarithms += Math.log(page.getDouble("interactive_score"));
        }
        double totalScore = Math.exp(logarithms / (2 * PAGES.size()));
        JSONObject scores = new JSONObject();
        scores.put("benchmark", "loadline2-phone");
        scores.put("version", version);
        scores.put("score_method", "perfetto_presentation");
        scores.put("pages", ordered);
        scores.put("total_score", totalScore);
        return scores;
    }

    private static long timestamp(String value, String column, String page) throws IOException {
        try {
            return Long.parseLong(value);
        } catch (NumberFormatException error) {
            throw new IOException("Missing or invalid " + column + " for " + page, error);
        }
    }
}
