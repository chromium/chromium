# WebLayer

WebLayer is a high level embedding API to support building a browser.

Unlike `src/content`, which is only concerned with a sandboxed multi-process web
platform, WebLayer includes modern browser features and Google integration.
It's the reusable version of Chrome, which might share some portions of the UI
and also its support for all the modern HTML5 and browser features (e.g. UI for
permissions, autofill, safe browsing etc...).

While it's built on top of `src/content`, the expectation is that the API will
hide the Content API.

## Design Goals
1. WebLayer should be powerful enough to build a modern browser. This means:
    * as fast and secure as Chrome
    * includes all the same web platform features as Chrome (including UIs and system integration code)
2. WebLayer embedders should get new web platform features for free (e.g. don't need to keep updating their UIs or code)
3. WebLayer hides subprocesses, so any code that needs to run in the renderer needs to be part of WebLayer. Script injection
is generally discouraged for security and performance reasons.

## Resources and Documentation

Mailing list: [weblayer-dev@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/weblayer-dev)

Bug tracker: [Internals>WebLayer](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=component%3AInternals%3EWebLayer)

## Directory Structure

`public` the C++ and Java public API. These are the only files an app should use

`shell` sample app

`test` test harnesses and test-only helper code

`tools` helper scripts

`app` internal code which runs at the beginning of each process

`browser` internal code which runs in the browser process

`common` internal code which runs in the browser and child processes

`renderer` internal code which runs in the renderer process

`utility` internal code which runs in the utility process

## Setting Up Your Build Environment

If you haven't done this already, you first need to set up an Android build. If
you are a Google employee, reach out to weblayer-team@google.com for internal
instructions. Otherwise follow the
[Android build instructions](https://source.chromium.org/chromium/chromium/src/+/main:docs/android_build_instructions.md).

## Building and Testing

To run the sample app:

```
    $ autoninja -C out/Default run_weblayer_shell
    $ out/Default/bin/run_weblayer_shell
```

To run instrumentation tests:

```
    $ autoninja -C out/Default weblayer_instrumentation_test_apk
    $ out/Default/bin/run_weblayer_instrumentation_test_apk
```

Note: this may not work on some versions of Android. If you see an error setting
the WebView provider when running instrumentation tests, try running the tests
using the WebLayer support APK which uses a different loading path:

```
    $ autoninja -C out/Default weblayer_support_instrumentation_test_apk
    $ out/Default/bin/run_weblayer_support_instrumentation_test_apk
```

The test script will build and install all necessary APKs.

## Running Skew Tests

Make sure you have the following gn arg:

```
system_webview_package_name = "com.google.android.webview"
```

The following tests the latest client library against M80:

```
    $ autoninja -C out/Default weblayer_instrumentation_test_versions_apk
    $ cipd install --root /tmp/M80 chromium/testing/weblayer-x86 m80
    $ out/Default/bin/run_weblayer_instrumentation_test_versions_apk \
       --test-runner-outdir out/Default \
       --client-outdir out/Default \
       --implementation-outdir /tmp/M80/out/Release
```

## Running WPT

To run WPT on android against weblayer do the following:

```
    $ WPT_TEST=badging/badge-success.https.html # test or directory you want to run, relative to wpt directory.
    $ autoninja -C out/Default weblayer_shell_wpt
    $ out/Default/bin/run_weblayer_shell_wpt \
      -t Default \
      --isolated-script-test-output out/Default/weblayer_out.json \
      --include $WPT_TEST \
      --ignore-browser-specific-expectations \
      --ignore-default-expectations \
      --avd-config tools/android/avd/proto/generic_android28.textpb
```

The script will write test artifacts to a directory named `layout-test-results` that is a sibling of `--isolated-script-test-output`.
You can view the results by serving them over HTTP:

```
    $ python3 -m http.server 8080 -d out/Default/layout-test-results
```

Then, navigate to `<host>:8080/results.html` in your browser.

To run against clank:

```
    $ WPT_TEST=badging/badge-success.https.html # test or directory you want to run, relative to wpt directory.
    $ autoninja -C out/Default chrome_public_wpt
    $ out/Default/bin/run_chrome_public_wpt \
      -t Default \
      --isolated-script-test-output out/Default/clank_out.json \
      --include $WPT_TEST \
      --ignore-browser-specific-expectations \
      --ignore-default-expectations \
      --avd-config tools/android/avd/proto/generic_android28.textpb
```

The `--ignore-browser-specific-expectations --ignore-default-expectations` flags will prevent
\*Expectations files from being loaded, which helps with error messages on test failures.
Once a test is fixed, rerun it without those flags to ensure the Expectations files are correct.

To run against linux with wptrunner (same runner we use on android, which runs normal chrome):

```
    $ WPT_TEST=badging/badge-success.https.html # test or directory you want to run, relative to wpt directory.
    $ autoninja -C out/Default wpt_tests_isolate
    $ out/Default/bin/run_wpt_tests_isolate \
      -t Default \
      --isolated-script-test-output out/Default/chrome_out.json \
      --include $WPT_TEST
````

To run against linux with `run_web_tests` (same runner we use on CI, which runs `content_shell`):

```
    $ WPT_TEST=badging/badge-success.https.html # test or directory you want to run, relative to wpt directory.
    $ autoninja -C out/Default blink_tests
    $ ./third_party/blink/tools/run_web_tests.py -t Default external/wpt/$WPT_TEST
```

Passing in `-vvvv` may be useful if you want to see loads of information about
test execution.

A list of known test failures is in [`WeblayerWPTExpectations`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/android/WeblayerWPTExpectations).
The values between the brackets at the end of each line list the expected
result types that test can have. For example, a test marked as "[ Failure ]" is
expected to fail, while a test marked as "[ Failure Pass ]" is expected to be
flaky.

Any failing tests should be removed from `WeblayerWPTExpectations` file once
fixed.

### Tips

While many WPT tests fail due to features not being implemented in WebLayer,
some may fail due to Features that aren't getting enabled or switches that
aren't getting passed to the test as they would be for Clank. If a test is
failing due to a missing Feature, check the test FieldTrial configuration
in [`fieldtrial_testing_config.json`](https://source.chromium.org/chromium/chromium/src/+/main:testing/variations/fieldtrial_testing_config.json).
A missing switch could have several causes, but the flags that get passed
to the test originate from [`third_party/wpt_tools/wpt/tools/wpt/run.py`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/wpt_tools/wpt/tools/wpt/run.py).


## Telemetry

Telemetry is run against WebLayer, currently on the bot
[`android-pixel2_weblayer-perf`](https://ci.chromium.org/p/chrome/builders/ci/android-pixel2_weblayer-perf).

Telemetry currently only runs on real hardware. Bug
[1067712](https://bugs.chromium.org/p/chromium/issues/detail?id=1067712) is for
adding support for emulators.

### Tricks:

To see the set of stories executed, click on a successful run, search for
`performance_weblayer_test_suite` and click on the `json.output`
link.

Googlers can submit jobs against your own patch using
[pinpoint](https://pinpoint-dot-chromeperf.appspot.com/). At the time of this
writing, logcat is *not* captured for successful runs
([1067024](https://bugs.chromium.org/p/chromium/issues/detail?id=1067024)).
Submitting a pinpoint run against a patch with a CHECK will generate
logcat. For such a run, the logcat is viewable by way of:

1. Click on Id next to `task` under `Test`.
2. Expand `+` (under `More Details`).
3. Click on link next to `Isolated Outputs`.
4. Click on `test_results.json`.
5. Replace `gs://` with `https://pantheon.corp.google.com/storage/browser`.
