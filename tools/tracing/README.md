# Chrome Tracing Tools - Guide

The tools/tracing directory contains scripts for both recording and symbolizing
traces in proto file format. This doc outlines different use cases for the tool.
Each use case shows what arguments are most important for the script to work.
Run `tools/tracing/profile_chrome_startup --help` or
`tools/tracing/symbolize_trace --help` for more details on more command-line
flags.

[TOC]

## What's supported?

Platform | Trace Recording                 | Trace Symbolization
-------- | ------------------------------- | -------------------
Android  | ✔️                              | ✔️
Linux    | ❌ (use https://ui.perfetto.dev) | ✔️
MacOS    | ❌ (use https://ui.perfetto.dev) | ✔️
Windows  | ❌                               | ❌

## **Profiling Chrome**

**For local builds:** In order for this tool to work, make sure that a build
directory for Chrome exists and that Chrome is built with the gn arg
‘symbol_level’ >= 1.

### **Android:**

#### Setting up to collect profiles:

Supported architectures for heap profiling:

*   **Local builds**: arm32, arm64, x86-64

    *   Set 'symbol_level' to 1 or 2 in gn args. Symbols are used to add
        function names to CPU and heap profiles. Level 1 only includes public
        functions. Level 2 includes all function names but takes longer to
        build.

    *   **Emulator**: Use x86-64 architecture. x86 architecture is not
        supported.

    *   **Arm64**: If you are building for arm64 and using the primary apk
        (chrome.apk or monochrome.apk), then profiling works with all the
        builds.

    *   **Arm64-secondary**: If you are using builds from secondary arch folder
        (clang_arm32), then make sure the arm32 gn args defined below are
        included.

    *   Prefer to use arm64/x86-64 primary since arm32 support is less stable.

    *   For **arm32**, set these gn args:

        *   Unofficial: `enable_profiling=true`, `arm_use_thumb=false`,
            `is_component_build=false` and `symbol_level=1`.
        *   Official: for arm32 you can use `is_official_build=true`,
            `symbol_level=1`.

*   **Official builds**: arm32, arm64, x86-64.

    *   Official builds need to be installed from the play store.
    *   For arm32, only canary and dev channels are supported.

Note: x86 architecture is not supported for this.

Supported architectures for CPU profiling:

*   CPU profiling not supported on emulators.
*   Official builds: arm32 and arm64.

    *   Official builds need to be downloaded from the play store.
    *   For arm32, only canary and dev channels are supported.

*   Local builds: arm32 and arm64

    *   Include gn arg: `symbol_level=1` so that function names are included in
        symbol files. For arm32, include the following gn args:
        `is_official_build=true`, `symbol_level=1`.

Setup steps:

1.  Build Chrome using x86-64 (with gn arg: `target_os="x64"` for local builds)
    or other supported architectures.
1.  Skip this step if heap profiling is not needed. Setup Chrome command line
    for enabling heap profiling before starting Chrome:

    *   Run:

    ```
    build/android/adb_chrome_public_command_line \
      --memlog-sampling-rate=1000000 --memlog=browser \
      --use-heap-profiling-proto-writer
    ```

    *   Alternatively you can enable heap profiling from
        chrome://memory-internals.
    *   For CPU sampling profiling, no Chrome command line setup is needed.

1.  Make sure that the Chrome browser installed has permission to access device
    storage. Chrome needs permission to write traces to disk (outside data dir).

    *   On your device/emulator with Chrome: Setting > Apps > App Info, then
        click the Chrome app you want to profile. Click on App Permissions and
        allow access to device storage.

1.  Build dump_syms by running: ninja -C out/build_dir dump_syms. This is needed
    for official builds too. You need to setup a local directory with the same
    `target_os` as the profiled device and set `is_debug=false` in gn args.

#### Collecting profiles:

##### Using Perfetto UI (https://ui.perfetto.dev):

Heap profiling:

*   Enable memory-infra (high-overhead) for memory in Chrome categories and
    collect traces. this would capture a profile every 10 seconds.

CPU profiling:

*   For CPU profiling, enable cpu_profiler (high overhead) in Chrome categories
    and collect traces.

Note: Download and symbolize this profile using symbolization steps below.

##### For local builds:

1.  Find the browser you want to profile. The `--browser` flag will be used to
    select the browser.

    *   Examples for local builds: `--browser=build` (for clankium),
        `--browser=chromium` (for chromium), `--browser=beta`,
        `--browser=stable`. `--help` will give a list of all possible browser
        options.

1.  Run profile_chrome_startup:

    ```
    tools/tracing/profile_chrome_startup \
      --local_build_dir=LOCAL_BUILD_DIR --browser=BROWSER \
      [--enable_profiler={memory,cpu}]
    ```

    Example heap profiling:

    ```
    tools/tracing/profile_chrome_startup \
      --local_build_dir=out/Release --browser=build --enable_profiler=memory
    ```

    *   When collecting heap profiles, samples are collected every 10 seconds.
        Extend the time for heap profiling by setting the `--time` flag greater
        than 10 seconds to actually collect heap samples. For example,
        specifying `--time=300` will ensure heap samples are taken every 10
        seconds for five minutes.

##### For official builds:

1.  Find the browser you want to profile. The `--browser` flag will be used to
    select the browser.

    *   Examples for official builds: `--browser=beta`, `--browser=canary`,
        `--browser=stable`. `--help` will give a list of all possible browser
        options.

1.  Run profile_chrome_startup:

    ```
    tools/tracing/profile_chrome_startup \
      --dump_syms=DUMP_SYMS --browser=BROWSER [--enable_profiler={memory,cpu}]
    ```

    Example heap profiling:

    ```
      tools/tracing/profile_chrome_startup \
        --dump_syms=out/Release/dump_syms \
        local_build_dir=out/Release --browser=stable --enable_profiler=memory
    ```

    *   When collecting heap profiles, samples are collected every 10 seconds.
        Extend the time for heap profiling by setting the `--time` flag greater
        than 10 seconds to actually collect heap samples. For example,
        specifying `--time=300` will ensure heap samples are taken every 10
        seconds for five minutes.

**Notes:**

*   To specify the kind of profile to collect, the `--enable_profiler` flag is
    needed. You can specify either `cpu` or `memory` or a comma-separated list
    containing both as arguments.

*   To enable/disable specific Chrome categories while recording a trace, use
    the `--chrome_categories flag`.

*   To view a symbolized trace automatically after symbolization, you can pass
    the `--view` flag to automatically open the symbolized trace in
    https://ui.perfetto.dev.

*   For steps to save time on multiple runs, refer to **Caching Symbols for
    Multiple Traces** section for additional flags to add to the command line.

*   In case dump_syms is not found by the script, specify the `--dump_syms`
    flag.

*   It is not needed, but if you want to specify the directory to hold breakpad
    files, use the `--breakpad_output_dir` flag.

*   If symbolization is not needed, the `--skip_symbolize` flag specifies that a
    trace should skip symbolization after collection. If specified omit the
    `--dump_syms` and `--local_build_dir` flags since symbolization is not
    needed.

### **For Mac and Linux:**

#### Collecting profiles:

##### Using Perfetto UI (https://ui.perfetto.dev):

Heap profiling:

*   Enable memory-infra (high-overhead) for memory in Chrome categories and
    collect traces. this would capture a profile every 10 seconds.

CPU profiling:

*   For CPU profiling, enable cpu_profiler (high overhead) in Chrome categories
    and collect traces.

Note: Download and symbolize this profile using symbolization steps below.

### **For Windows:**

You can collect a trace using https://ui.perfetto.dev, but symbolizing on
Windows is not currently supported with this script.

## **Symbolizing traces (Only works for proto traces)**

### **Android, Linux, Mac:**

#### For local builds:

##### Symbolization setup:

1.  Build dump_syms by running: ninja -C out/build_dir dump_syms.
1.  Find the trace you want to symbolize.

##### Symbolizing:

1.  Run symbolize_trace:

```
tools/tracing/symbolize_trace [trace_file] --local_build_dir=LOCAL_BUILD_DIR
```

#### For official builds:

1.  Skip this step for Mac and Linux. Build dump_syms by running: ninja -C
    out/build_dir dump_syms.
1.  Run symbolize_trace:

```
tools/tracing/symbolize_trace [trace_file]
```

*   Include `--dump_syms=DUMP_SYMS` for Android. Mac and Linux official builds
    do not require dump_syms, but for Android traces, a path to dump_syms should
    be found.

**Notes**

*   To view a symbolized trace automiatically after symbolization, you can pass
    the `--view` flag to automatically open the symbolized trace in
    https://ui.perfetto.dev.

*   For steps to save time on multiple runs, refer to **Caching Symbols for
    Multiple Traces** section for additional flags to add to the command line.

### **Windows and Chrome OS:** Not yet supported.

## Caching Symbols for Multiple Traces:

*   To cache symbols for symbolizing different traces from the same build of
    Chrome include the --breakpad_output_dir flag (this will work for both
    `profile_chrome_startup` and `symbolize_trace` scripts). If
    `--breakpad_output_dir` is specified in the first run, the flag can be
    replaced with `--local_breakpad_dir` to save time in subsequent runs, by
    using the breakpad symbols that have been stored. Ex:

    For `tools/tracing/profile_chrome_startup`:

    *   1st run: `tools/tracing/profile_chrome_startup
        --breakpad_output_dir=/tmp`

    *   Future runs: `tools/tracing/profile_chrome_startup
        --local_breakpad_dir=/tmp`

    For `tools/tracing/symbolize_trace`:

    *   1st run: `tools/tracing/symbolize_trace [trace_file1]
        --breakpad_output_dir=/tmp`

    *   Future runs: `tools/tracing/symbolize_trace [trace_file2]
        --local_breakpad_dir=/tmp`

## Troubleshooting:

*   If `tools/tracing/profile_chrome_startup` gives `Error : Activity Class` or
    the desired Chrome browser does not open:

    *   Check that the browser given for the `--browser` flag matches the build
        of the browser you want to profile.

*   If heap profiling gives and empty trace:

    *   Make sure 'memory' is included as an argument for the
        `--enable_profiler` flag.
    *   Make sure that the `--time` flag is set to a value greater than 10.
    *   Make sure Chrome is able to read the command line (give storage
        permission, as outlined in steps to profile Chrome) and that Chrome is
        restarted after setting the command line for memlog.

*   If symbolization fails because dump_syms cannot be found:

    *   Make sure that the dump_syms binary you built is given for the
        `--dump_syms` or that the binary can be found in the directory given for
        `--local_build_dir`

*   If an error shows that no symbolizer is found:

    *   Try building the trace_to_text tool in perfetto, using instructions
        found here: https://perfetto.dev/docs/contributing/build-instructions.
        You can try the script again, specifying the `--symbolizer` flag, or you
        can use the trace_to_text tool directly by running:
        `path/to/trace_to_text [trace_file]`

See the google internal design doc for more details pertaining to this tool:
https://docs.google.com/document/d/1BJPbcl5SPjOvuRuP1JSFAUPK3ZWNIS7j1h94rPHRzVE
