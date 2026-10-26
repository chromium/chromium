// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.build.annotations.NullMarked;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

@NullMarked
final class TraceProcessor {
    private final RunArtifacts mArtifacts;

    TraceProcessor(RunArtifacts artifacts) {
        mArtifacts = artifacts;
    }

    QueryResult query(RunArtifacts.TraceArtifact trace, File query, Deadline deadline)
            throws IOException, InterruptedException {
        File csv = mArtifacts.temporaryFile("query-", ".csv");
        try (NativeTool tool =
                new NativeTool(
                        Arrays.asList(
                                mArtifacts.traceProcessor().getAbsolutePath(),
                                "query",
                                "--query-file",
                                query.getAbsolutePath(),
                                trace.mFile.getAbsolutePath()),
                        mArtifacts.directory(),
                        csv)) {
            tool.awaitExit(deadline);
        }
        try (FileInputStream input = new FileInputStream(csv)) {
            return parseCsv(RunArtifacts.readText(input));
        }
    }

    static QueryResult parseCsv(String text) throws IOException {
        List<List<String>> records = new ArrayList<>();
        List<String> row = new ArrayList<>();
        StringBuilder field = new StringBuilder();
        boolean quoted = false;
        boolean closedQuote = false;
        for (int index = 0; index < text.length(); index++) {
            char value = text.charAt(index);
            if (quoted) {
                if (value != '"') {
                    field.append(value);
                } else if (index + 1 < text.length() && text.charAt(index + 1) == '"') {
                    field.append('"');
                    index++;
                } else {
                    quoted = false;
                    closedQuote = true;
                }
                continue;
            }
            if (value == '"' && field.length() == 0 && !closedQuote) {
                quoted = true;
            } else if (value == ',' || value == '\n' || value == '\r') {
                boolean blank =
                        row.isEmpty() && field.length() == 0 && !closedQuote && value != ',';
                row.add(field.toString());
                field.setLength(0);
                closedQuote = false;
                if (value != ',') {
                    if (blank) {
                        records.add(Collections.emptyList());
                    } else {
                        records.add(row);
                    }
                    row = new ArrayList<>();
                    if (value == '\r'
                            && index + 1 < text.length()
                            && text.charAt(index + 1) == '\n') {
                        index++;
                    }
                }
            } else {
                if (closedQuote || value == '"') {
                    throw new IOException("Malformed query CSV");
                }
                field.append(value);
            }
        }
        if (quoted) {
            throw new IOException("Incomplete quoted query CSV field");
        }
        if (!row.isEmpty() || field.length() > 0 || closedQuote) {
            row.add(field.toString());
            records.add(row);
        }
        List<String> columns = Collections.emptyList();
        List<List<String>> rows = new ArrayList<>();
        boolean ended = false;
        for (List<String> record : records) {
            if (record.isEmpty()) {
                if (!columns.isEmpty()) {
                    ended = true;
                }
                continue;
            }
            if (ended) {
                throw new IOException("Query must produce one result table");
            }
            if (columns.isEmpty()) {
                columns = record;
            } else {
                if (record.size() != columns.size()) {
                    throw new IOException("Query CSV width mismatch");
                }
                rows.add(record);
            }
        }
        if (columns.isEmpty()) {
            throw new IOException("Query produced no table");
        }
        return new QueryResult(columns, rows);
    }

    static final class QueryResult {
        final List<String> mColumns;
        final List<List<String>> mRows;

        QueryResult(List<String> columns, List<List<String>> rows) {
            mColumns = columns;
            mRows = rows;
        }

        int column(String name) throws IOException {
            int index = mColumns.indexOf(name);
            if (index == -1 || mColumns.lastIndexOf(name) != index) {
                throw new IOException("Missing or duplicate query column: " + name);
            }
            return index;
        }

        JSONObject toJson() throws JSONException {
            JSONArray values = new JSONArray();
            for (List<String> row : mRows) {
                values.put(new JSONArray(row));
            }
            return new JSONObject().put("columns", new JSONArray(mColumns)).put("rows", values);
        }
    }
}
