# Tools for Analyzing Chrome's Binary Size

These tools currently focus on supporting Android. They somewhat work with
Linux builds. As for Windows, some great tools already exist and are documented
here:

 * https://www.chromium.org/developers/windows-binary-sizes

There is also a dedicated mailing-list for binary size discussions:

 * https://groups.google.com/a/chromium.org/forum/#!forum/binary-size

Bugs and feature requests are tracked in issues.chromium.org under:

 * [Tools > BinarySize](https://issues.chromium.org/issues?q=status:open%20componentid:1456763)

Per-Milestone Binary Size Breakdowns:

 * https://chrome-supersize.firebaseapp.com

Guide to dealing with chrome-perf size alerts:

 * [//docs/speed/apk_size_regressions.md](/docs/speed/apk_size_regressions.md)

[TOC]

## Binary Size Trybot (android-binary-size)

 * Introduced October 2018 as a mandatory CQ bot.
 * Documented [here](/docs/speed/binary_size/android_binary_size_trybot.md).

## Binary Size Gerrit Plugin

 * Introduced February 2020 to surface results from android-binary-size.
 * Documented [here](/docs/speed/binary_size/android_binary_size_trybot.md).

## resource_sizes.py

 * [//build/android/resource_sizes.py](https://cs.chromium.org/chromium/src/build/android/resource_sizes.py)
 * Able to run on an `.apk` without having the build directory available.
 * Reports the size metrics captured by our perf builders. Viewable at
   [chromeperf](https://chromeperf.appspot.com/report) under
   `Test suite="resource_sizes ($APK)"`.
 * Metrics reported by this tool are described in
   [//docs/speed/binary_size/metrics.md](/docs/speed/binary_size/metrics.md).

## SuperSize

Collects, archives, and analyzes Chrome's binary size on Android.
See [//tools/binary_size/libsupersize/README.md](/tools/binary_size/libsupersize/README.md).

## diagnose_bloat.py

Determines the cause of binary size bloat between two commits. Works for Android
and Linux (although Linux symbol diffs have issues, as noted below).

### How it Works

1. Builds multiple revisions using release GN args.
   * Default is to build just two revisions (before & after commit)
1. Measures all outputs using `resource_size.py` and `supersize`.
1. Saves & displays a breakdown of the difference in binary sizes.

### Example Usage

``` bash
# Build and diff trichrome_bundle HEAD^ and HEAD.
tools/binary_size/diagnose_bloat.py HEAD -v

# Build and diff trichrome_google_bundle HEAD^ and HEAD.
tools/binary_size/diagnose_bloat.py HEAD --enable-chrome-android-internal -v

# Build and diff trichrome_google_64_32_bundle HEAD^ and HEAD.
tools/binary_size/diagnose_bloat.py HEAD --enable-chrome-android-internal --arm64 -v

# Build and diff trichrome_bundle HEAD^ and HEAD without is_official_build.
tools/binary_size/diagnose_bloat.py HEAD --gn-args="is_official_build=false" -v

# Build and diff all contiguous revs in range BEFORE_REV..AFTER_REV for src/v8.
tools/binary_size/diagnose_bloat.py AFTER_REV --reference-rev BEFORE_REV --subrepo v8 --all -v

# Build and diff system_webview_apk HEAD^ and HEAD with arsc obfucstion disabled.
tools/binary_size/diagnose_bloat.py HEAD --target system_webview_apk --gn-args enable_arsc_obfuscation=false

# Display detailed usage info (there are many options).
tools/binary_size/diagnose_bloat.py -h
```

## Other Size Tools

### Bloaty McBloatface
 * https://github.com/google/bloaty
 * Our usage tracked in [crbug/698733](https://crbug.com/698733)
