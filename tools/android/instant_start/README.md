# Benchmarking and analyzing scripts for Instant Start

## Introduction

In order to understand the performance implication of a CL, we can do a local
benchmark to compare the before/after metrics.

## Usage

Build two APKs for before and after a CL, on gn target `monochrome_apk`, and
make sure they are on different Chrome channels, like Canary (gn args
`android_channel = "canary"`) and default (unspecified) because they will be
installed side by side. Depending on your preferred workflow, you can use two
separate workspaces like `~/code/clankium/src` and `~/code/clankium2/src`, or
use the same workspace but two different output directories like `./out/Release`
and `./out/Release2`, or simply use the same output directory but rename the APK
like `out/Release/bin/monochrome_before_apk` and
`out/Release/bin/monochrome_after_apk`.

On the device, uninstall Chrome of these two channels to make sure the
environment is clean. Otherwise, chrome://flags changes and Finch trials could
introduce undesirable differences. You can use the `--reinstall` option to
automate this.  When running benchmark.py, first-run experience (FRE) would be
skipped, but you'll need to manually create one tab, make sure Feed is loaded,
and swipe away the login prompt in the dry-run step. Follow the instructions of
the script.

The command line looks like this:

```bash
./tools/android/instant_start/benchmark.py --control-apk out/Release/bin/monochrome_before_apk --experiment-apk out/Release/bin/monochrome_after_apk -v --repeat 100 --reinstall
```

The metrics are persisted to `runs.pickle` by default, and the filename can be
specified by `--data-output` option. This can later be analyzed like this:

```bash
./tools/android/instant_start/analyze.py runs.pickle
```

The output looks like:

```
Reading runs-pixel3xl.pickle with {'model': 'Pixel 3 XL', 'start_time': datetime.datetime(2020, 9, 19, 14, 55, 34, 596731)}
100 samples on Pixel 3 XL
                               Median  Diff with control   p-value
FirstDrawCompletedTime        155.0ms   -13.5ms (-8.71%)  0.000000
SingleTabTitleAvailableTime   117.0ms  -13.0ms (-11.11%)  0.000000
FeedStreamCreatedTime         356.0ms   -35.5ms (-9.97%)  0.000001
FeedsLoadingPlaceholderShown   94.5ms    -2.0ms (-2.12%)  0.007312
FeedContentFirstLoadedTime    924.0ms    -6.5ms (-0.70%)  0.034100
```
