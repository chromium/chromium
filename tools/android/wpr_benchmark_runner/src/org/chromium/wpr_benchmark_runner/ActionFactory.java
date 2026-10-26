// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import android.app.UiAutomation;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.build.NullUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.File;
import java.io.FileInputStream;
import java.math.BigDecimal;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

@NullMarked
final class ActionFactory implements AutoCloseable {
    private static final Pattern PLACEHOLDER = Pattern.compile("\\{([a-zA-Z_][a-zA-Z0-9_]*)\\}");
    private static final Pattern DURATION = Pattern.compile("(\\d+(?:\\.\\d+)?)\\s*(ms|s|m|h)?");

    private final UiAutomation mAutomation;
    private final RunArtifacts mArtifacts;
    private final Deadline mDeadline;
    private final CdpSocket mCdpSocket;
    private final TraceProcessor mTraceProcessor;
    private final JSONObject mBrowser;
    private final Map<String, Object> mResults = new LinkedHashMap<>();
    private final Map<String, Class<?>> mOutputs = new LinkedHashMap<>();

    ActionFactory(
            UiAutomation automation, RunArtifacts artifacts, JSONObject browser, Deadline deadline)
            throws JSONException {
        mAutomation = automation;
        mArtifacts = artifacts;
        mDeadline = deadline;
        mBrowser = browser;
        mCdpSocket =
                new CdpSocket(
                        automation,
                        text(mBrowser, "package"),
                        artifacts.cdpRelay(),
                        text(mBrowser, "devtools_socket"));
        mTraceProcessor = new TraceProcessor(artifacts);
    }

    Plan parse(JSONObject definition) throws Exception {
        List<Step> actions = new ArrayList<>();
        List<Step> afterPages = new ArrayList<>();
        List<Step> pages = parsePages(definition.getJSONArray("pages"));
        configureProbe(definition, actions, afterPages);
        List<Step> processing =
                parseActions(definition.getJSONArray("output_processing"), "output_processing");
        List<Step> resultFiles = new ArrayList<>();
        configureFileOutput(definition.optJSONObject("file_output"), afterPages, resultFiles);
        actions.addAll(pages);
        actions.addAll(afterPages);
        actions.addAll(processing);
        actions.addAll(resultFiles);
        return new Plan(actions, prepareReplay(definition));
    }

    private List<Step> parsePages(JSONArray definitions) throws Exception {
        List<Step> actions = new ArrayList<>();
        if (definitions.length() == 0) {
            throw new IllegalArgumentException("pages must not be empty");
        }
        for (int pageIndex = 0; pageIndex < definitions.length(); pageIndex++) {
            JSONObject page = definitions.getJSONObject(pageIndex);
            String pageName = text(page, "name");
            String pageLocation = "pages[" + pageIndex + "] (" + pageName + ")";
            JSONArray blocks = page.getJSONArray("blocks");
            for (int blockIndex = 0; blockIndex < blocks.length(); blockIndex++) {
                JSONObject block = blocks.getJSONObject(blockIndex);
                String blockName = text(block, "name");
                String location = pageLocation + ".blocks[" + blockIndex + "] (" + blockName + ")";
                if (blockName.equals("load")) {
                    actions.add(new Step(new Tap(mAutomation, mDeadline, 0, 0), location + ".tap"));
                }
                actions.addAll(parseActions(block.getJSONArray("actions"), location + ".actions"));
            }
        }
        return actions;
    }

    private List<Step> parseActions(JSONArray definitions, String prefix) throws Exception {
        List<Step> actions = new ArrayList<>();
        for (int index = 0; index < definitions.length(); index++) {
            String location = prefix + "[" + index + "]";
            try {
                actions.add(new Step(create(definitions.getJSONObject(index)), location));
            } catch (Exception error) {
                throw new IllegalArgumentException(location + ": " + error.getMessage(), error);
            }
        }
        return actions;
    }

    private void configureProbe(
            JSONObject definition, List<Step> beforePages, List<Step> afterPages) throws Exception {
        JSONObject probe = definition.optJSONObject("probe");
        if (probe == null || !probe.optBoolean("enabled", false)) {
            return;
        }
        JSONObject traceConfig = probe.getJSONObject("trace_config");
        if (traceConfig.getJSONArray("includedCategories").length() == 0) {
            throw new IllegalArgumentException("Trace categories must not be empty");
        }
        beforePages.add(
                new Step(
                        new StartProbe(
                                mCdpSocket,
                                mDeadline,
                                traceConfig,
                                number(probe, "start_timeout_ms", 10_000, 1)),
                        "probe.start"));
        String output = text(definition.getJSONObject("trace_processor"), "output");
        declareOutput(output, RunArtifacts.TraceArtifact.class);
        afterPages.add(
                new Step(
                        new StopProbe(
                                mCdpSocket,
                                this,
                                mArtifacts,
                                mDeadline,
                                output,
                                number(probe, "stop_timeout_ms", 120_000, 1)),
                        "probe.stop"));
    }

    private void configureFileOutput(
            @Nullable JSONObject config, List<Step> traceFiles, List<Step> resultFiles)
            throws JSONException {
        if (config == null || !config.optBoolean("enabled", false)) {
            return;
        }
        JSONArray files = config.getJSONArray("files");
        for (int index = 0; index < files.length(); index++) {
            JSONObject file = files.getJSONObject(index);
            String input = text(file, "input");
            if (!mOutputs.containsKey(input)) {
                throw new IllegalArgumentException("File input has no producer: " + input);
            }
            String filename = text(file, "filename");
            java.nio.file.Path path = new File(filename).toPath();
            if (path.isAbsolute() || path.normalize().startsWith("..")) {
                throw new IllegalArgumentException("filename must stay within results-dir");
            }
            Step step =
                    new Step(
                            new DumpToFile(this, mArtifacts, mDeadline, input, filename),
                            "file_output.files[" + index + "]");
            if (mOutputs.get(input) == RunArtifacts.TraceArtifact.class) {
                traceFiles.add(step);
            } else {
                resultFiles.add(step);
            }
        }
    }

    private Action create(JSONObject action) throws Exception {
        String type = text(action, "action");
        if (action.has("duration") && !type.equals("wait")) {
            throw new IllegalArgumentException("duration is supported only for wait actions");
        }
        String input = optionalText(action, "input", "");
        String output = optionalText(action, "output", "");
        if (!input.isEmpty() && !mOutputs.containsKey(input)) {
            throw new IllegalArgumentException("Result has no earlier producer: " + input);
        }
        Class<?> outputType = Object.class;
        Action result;
        switch (type) {
            case "get":
                String target = optionalText(action, "target", "_self");
                String readyState = optionalText(action, "ready_state", "interactive");
                if (!target.equals("_self") && !target.equals("_new_tab")) {
                    throw new IllegalArgumentException("target must be _self or _new_tab");
                }
                if (!readyState.equals("any")
                        && !readyState.equals("interactive")
                        && !readyState.equals("complete")) {
                    throw new IllegalArgumentException("Invalid ready_state");
                }
                String url = text(action, "url");
                result =
                        new Get(
                                mCdpSocket,
                                mDeadline,
                                url,
                                target,
                                readyState,
                                optionalText(action, "url_contains", url),
                                durationMillis(action, "timeout", 10_000, 1));
                break;
            case "wait":
                result = new Wait(mDeadline, durationMillis(action, "duration", -1, 0));
                break;
            case "tap":
                JSONObject coordinates = action.getJSONObject("position");
                float x = coordinate(coordinates, "x");
                float y = coordinate(coordinates, "y");
                result = new Tap(mAutomation, mDeadline, x, y);
                break;
            case "js":
                result =
                        new Js(
                                mCdpSocket,
                                this,
                                mDeadline,
                                text(action, "script"),
                                output,
                                durationMillis(action, "timeout", CdpSocket.COMMAND_TIMEOUT_MS, 1));
                break;
            case "wait_for_condition":
                result =
                        new WaitForCondition(
                                mCdpSocket,
                                mDeadline,
                                text(action, "condition"),
                                optionalText(action, "failure_condition", ""),
                                durationMillis(action, "timeout", 10_000, 1),
                                number(action, "poll_interval_ms", 100, 1),
                                number(
                                        action,
                                        "command_timeout_ms",
                                        CdpSocket.COMMAND_TIMEOUT_MS,
                                        1));
                break;
            case "trace_processor":
            case "process_llp2_trace":
                if (input.isEmpty()) {
                    throw new IllegalArgumentException("input must be a nonempty string");
                }
                if (output.isEmpty()) {
                    throw new IllegalArgumentException("output must be a nonempty string");
                }
                if (mOutputs.get(input) != RunArtifacts.TraceArtifact.class) {
                    throw new IllegalArgumentException("Input must be a completed trace: " + input);
                }
                File query = file(text(action, "query_file"));
                long timeout = durationMillis(action, "timeout", 120_000, 1);
                if (type.equals("trace_processor")) {
                    outputType = TraceProcessor.QueryResult.class;
                    result =
                            new RunTraceProcessor(
                                    this,
                                    mTraceProcessor,
                                    mDeadline,
                                    input,
                                    output,
                                    query,
                                    timeout);
                } else {
                    String version = text(action, "version");
                    if (!version.matches("2(?:\\.[0-9]+)*")) {
                        throw new IllegalArgumentException(
                                "Unsupported LLP2 phone version: " + version);
                    }
                    outputType = JSONObject.class;
                    result =
                            new ProcessLlp2Trace(
                                    this,
                                    mTraceProcessor,
                                    mDeadline,
                                    input,
                                    output,
                                    query,
                                    timeout,
                                    version);
                }
                break;
            case "dump_to_syslog":
                result = new DumpToSyslog(this, mDeadline, text(action, "input"));
                break;
            default:
                throw new IllegalArgumentException("Unknown action: " + type);
        }
        if (!output.isEmpty()) {
            if (!type.equals("js")
                    && !type.equals("trace_processor")
                    && !type.equals("process_llp2_trace")) {
                throw new IllegalArgumentException(type + " does not produce a result");
            }
            declareOutput(output, outputType);
        }
        return result;
    }

    private void declareOutput(String output, Class<?> type) {
        if (mOutputs.putIfAbsent(output, type) != null) {
            throw new IllegalArgumentException("Duplicate output ID: " + output);
        }
    }

    private File file(String alias) {
        return NullUtil.assumeNonNull(mArtifacts.files().get(alias));
    }

    private ReplayConfig prepareReplay(JSONObject definition) throws Exception {
        Map<String, String> bindings = new LinkedHashMap<>();
        for (Map.Entry<String, File> binding : mArtifacts.files().entrySet()) {
            bindings.put(binding.getKey(), binding.getValue().getAbsolutePath());
        }
        JSONObject injections = definition.getJSONObject("_injection_bindings");
        Iterator<String> rulesAliases = injections.keys();
        while (rulesAliases.hasNext()) {
            String rulesAlias = rulesAliases.next();
            JSONArray rules;
            try (FileInputStream input = new FileInputStream(file(rulesAlias))) {
                rules = new JSONArray(RunArtifacts.readText(input));
            }
            JSONObject scripts = injections.getJSONObject(rulesAlias);
            for (int index = 0; index < rules.length(); index++) {
                JSONObject rule = rules.getJSONObject(index);
                if (!rule.has("InjectedScript")) {
                    continue;
                }
                String alias = scripts.getString(rule.getString("InjectedScript"));
                rule.put("InjectedScript", file(alias).getAbsolutePath());
            }
            File resolved = mArtifacts.temporaryFile("rules-", ".json");
            RunArtifacts.writeJson(resolved, rules);
            bindings.put(rulesAlias, resolved.getAbsolutePath());
        }
        bindings.put("http_port", String.valueOf(mBrowser.getInt("http_port")));
        bindings.put("https_port", String.valueOf(mBrowser.getInt("https_port")));
        JSONObject wpr = definition.getJSONObject("wpr");
        JSONArray argv = wpr.getJSONArray("command");
        List<String> command = new ArrayList<>();
        for (int index = 0; index < argv.length(); index++) {
            String argument = argv.getString(index);
            Matcher matcher = PLACEHOLDER.matcher(argument);
            StringBuilder expanded = new StringBuilder();
            int start = 0;
            while (matcher.find()) {
                expanded.append(argument, start, matcher.start());
                expanded.append(Objects.requireNonNull(bindings.get(matcher.group(1))));
                start = matcher.end();
            }
            expanded.append(argument, start, argument.length());
            command.add(expanded.toString());
        }
        return new ReplayConfig(
                command,
                text(mBrowser, "replay_host"),
                mBrowser.getInt("http_port"),
                mBrowser.getInt("https_port"),
                number(wpr, "ready_timeout_ms", 60_000, 1));
    }

    <T> T requireResult(String id, Class<T> type) {
        Object result = mResults.get(id);
        if (!type.isInstance(result)) {
            throw new IllegalStateException("Missing or incorrect result type for " + id);
        }
        return type.cast(result);
    }

    void putResult(String id, Object value) {
        mResults.put(id, value);
    }

    private static String text(JSONObject object, String name) throws JSONException {
        Object value = object.get(name);
        if (!(value instanceof String) || ((String) value).trim().isEmpty()) {
            throw new IllegalArgumentException(name + " must be a nonempty string");
        }
        return (String) value;
    }

    private static String optionalText(JSONObject object, String name, String fallback)
            throws JSONException {
        if (object.has(name)) {
            return text(object, name);
        }
        return fallback;
    }

    private static long durationMillis(JSONObject object, String name, long fallback, long minimum)
            throws JSONException {
        if (!object.has(name)) {
            if (fallback < minimum) {
                throw new IllegalArgumentException("Missing " + name);
            }
            return fallback;
        }
        Matcher matcher = DURATION.matcher(object.get(name).toString());
        if (!matcher.matches()) {
            throw new IllegalArgumentException("Invalid " + name + ": " + object.get(name));
        }
        long multiplier = 1000;
        @Nullable String unit = matcher.group(2);
        if ("ms".equals(unit)) {
            multiplier = 1;
        } else if ("m".equals(unit)) {
            multiplier = 60_000;
        } else if ("h".equals(unit)) {
            multiplier = 3_600_000;
        }
        long result =
                new BigDecimal(matcher.group(1))
                        .multiply(BigDecimal.valueOf(multiplier))
                        .longValueExact();
        if (result < minimum) {
            throw new IllegalArgumentException("Invalid " + name + ": " + result);
        }
        return result;
    }

    private static long number(JSONObject object, String name, long fallback, long minimum)
            throws JSONException {
        Object value = fallback;
        if (object.has(name)) {
            value = object.get(name);
        }
        if (!(value instanceof Integer) && !(value instanceof Long)) {
            throw new IllegalArgumentException(name + " must be an integer");
        }
        long result = ((Number) value).longValue();
        if (result < minimum) {
            throw new IllegalArgumentException("Invalid " + name + ": " + result);
        }
        return result;
    }

    private static float coordinate(JSONObject object, String name) throws JSONException {
        Object value = object.get(name);
        if (!(value instanceof Number)) {
            throw new IllegalArgumentException(name + " must be numeric");
        }
        float coordinate = ((Number) value).floatValue();
        if (!Float.isFinite(coordinate) || coordinate < 0) {
            throw new IllegalArgumentException("Invalid coordinate " + name);
        }
        return coordinate;
    }

    @Override
    public void close() {
        mCdpSocket.close();
    }

    static final class Plan {
        final List<Step> mActions;
        final ReplayConfig mReplay;

        Plan(List<Step> actions, ReplayConfig replay) {
            mActions = actions;
            mReplay = replay;
        }
    }

    static final class Step {
        final Action mAction;
        final String mLocation;

        Step(Action action, String location) {
            mAction = action;
            mLocation = location;
        }
    }

    static final class ReplayConfig {
        final List<String> mCommand;
        final String mHost;
        final int mHttpPort;
        final int mHttpsPort;
        final long mReadyTimeoutMillis;

        ReplayConfig(List<String> command, String host, int httpPort, int httpsPort, long timeout) {
            mCommand = command;
            mHost = host;
            mHttpPort = httpPort;
            mHttpsPort = httpsPort;
            mReadyTimeoutMillis = timeout;
        }
    }
}
