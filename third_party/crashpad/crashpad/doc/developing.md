<!--
Copyright 2015 The Crashpad Authors

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# Developing Crashpad

## Status

[Project status](status.md) information has moved to its own page.

## Introduction

Crashpad is a [Chromium project](https://www.chromium.org/Home). Most of its
development practices follow Chromium’s. In order to function on its own in
other projects, Crashpad uses
[mini_chromium](https://chromium.googlesource.com/chromium/mini_chromium/), a
small, self-contained library that provides many of Chromium’s useful low-level
base routines. [mini_chromium’s
README](https://chromium.googlesource.com/chromium/mini_chromium/+/main/README.md)
provides more detail.

## Prerequisites

To develop Crashpad, the following tools are necessary, and must be present in
the `$PATH` environment variable:

 * Appropriate development tools.
    * On macOS, install [Xcode](https://developer.apple.com/xcode/). The latest
      version is generally recommended.
    * On Windows, install [Visual Studio](https://www.visualstudio.com/) with
      C++ support and the Windows SDK. The latest version is generally
      recommended. Some tests also require the CDB debugger, installed with
      [Debugging Tools for
      Windows](https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/).
    * On Linux, obtain appropriate tools for C++ development through any
      appropriate means including the system’s package manager. On Debian and
      Debian-based distributions, the `build-essential`, `zlib1g-dev`, and any
      one of the `libcurl4-*-dev` packages such as `libcurl4-openssl-dev` should
      suffice.
 * Chromium’s
   [depot_tools](https://www.chromium.org/developers/how-tos/depottools).
 * [Git](https://git-scm.com/). This is provided by Xcode on macOS, by
   depot_tools on Windows, and through any appropriate means including the
   system’s package manager on Linux.
 * [Python](https://www.python.org/). This is provided by the operating system
   on macOS, by depot_tools on Windows, and through any appropriate means
   including the system’s package manager on Linux.

## Getting the Source Code

The main source code repository is a Git repository hosted at
https://chromium.googlesource.com/crashpad/crashpad. Although it is possible to
check out this repository directly with `git clone`, Crashpad’s dependencies are
managed by
[`gclient`](https://www.chromium.org/developers/how-tos/depottools#TOC-gclient)
instead of Git submodules, so to work on Crashpad, it is best to use `fetch` to
get the source code.

`fetch` and `gclient` are part of the
[depot_tools](https://www.chromium.org/developers/how-tos/depottools). There’s
no need to install them separately.

### Initial Checkout

```
$ mkdir ~/crashpad
$ cd ~/crashpad
$ fetch crashpad
```

`fetch crashpad` performs the initial `git clone` and `gclient sync`,
establishing a fully-functional local checkout.

### Subsequent Checkouts

```
$ cd ~/crashpad/crashpad
$ git pull -r
$ gclient sync
```

## Building

### Windows, Mac, Linux, Fuchsia

On Windows, Mac, Linux, and Fuchsia, Crashpad uses
[GN](https://gn.googlesource.com/gn) to generate
[Ninja](https://ninja-build.org/) build files. For example,

```
$ cd ~/crashpad/crashpad
$ gn gen out/Default
$ ninja -C out/Default
```

You can then use `gn args out/Default` or edit `out/Default/args.gn` to
configure the build, for example things like `is_debug=true` or
`target_cpu="x86"`.

GN and Ninja are part of the
[depot_tools](https://www.chromium.org/developers/how-tos/depottools). There’s
no need to install them separately.

#### Fuchsia

In order to instruct gclient to download the Fuchsia SDK, you need to add the
following to `~/crashpad/.gclient`:

```
target_os=["fuchsia"]
```

If you're using this tree to develop for multiple targets, you can also add
other entries to the the list (e.g. `target_os=["fuchsia", "mac"]`).

#### Optional Linux Configs

To pull and use Crashpad's version of clang and sysroot, make the following
changes.

Add the following to `~/crashpad/.gclient`.
```
"custom_vars": { "pull_linux_clang": True },
```
Add these args to `out/Default/args.gn`.
```
clang_path = "//third_party/linux/clang/linux-amd64"
target_sysroot = "//third_party/linux/sysroot"
```

### Android

This build relies on cross-compilation. It’s possible to develop Crashpad for
Android on any platform that the [Android NDK (Native Development
Kit)](https://developer.android.com/ndk/) runs on.

If it’s not already present on your system, [download the NDK package for your
system](https://developer.android.com/ndk/downloads/) and expand it to a
suitable location. These instructions assume that it’s been expanded to
`~/android-ndk-r21b`.

Note that Chrome uses Android API level 21 for both 64-bit platforms and
32-bit platforms. See Chrome’s
[`build/config/android/config.gni`](https://chromium.googlesource.com/chromium/src/+/main/build/config/android/config.gni)
which sets `android32_ndk_api_level` and `android64_ndk_api_level`.

Set these gn args
```
target_os = "android"
android_ndk_root = ~/android-ndk-r21b
android_api_level = 21
```

## Testing

Crashpad uses [Google Test](https://github.com/google/googletest/) as its
unit-testing framework, and some tests use [Google
Mock](https://github.com/google/googletest/tree/master/googlemock/) as well. Its
tests are currently split up into several test executables, each dedicated to
testing a different component. This may change in the future. After a successful
build, the test executables will be found at `out/Debug/crashpad_*_test`.

```
$ cd ~/crashpad/crashpad
$ out/Debug/crashpad_minidump_test
$ out/Debug/crashpad_util_test
```

A script is provided to run all of Crashpad’s tests. It accepts a single
argument, a path to the directory containing the test executables.

```
$ cd ~/crashpad/crashpad
$ python build/run_tests.py out/Debug
```

To run a subset of the tests, use the `--gtest_filter` flag, e.g., to run all
the tests for MinidumpStringWriter:

```
$ python build/run_tests.py out/Debug --gtest_filter MinidumpStringWriter\*
```

### Windows

On Windows, `end_to_end_test.py` requires the CDB debugger, installed with
[Debugging Tools for
Windows](https://docs.microsoft.com/windows-hardware/drivers/debugger/). This
can be installed either as part of the [Windows Driver
Kit](https://docs.microsoft.com/windows-hardware/drivers/download-the-wdk) or
the [Windows
SDK](https://developer.microsoft.com/windows/downloads/windows-10-sdk/). If the
Windows SDK has already been installed (possibly with Visual Studio) but
Debugging Tools for Windows is not present, it can be installed from Add or
remove programs→Windows Software Development Kit.

### Android

To test on Android, [ADB (Android Debug
Bridge)](https://developer.android.com/studio/command-line/adb.html) from the
[Android SDK](https://developer.android.com/sdk/) must be in the `PATH`. Note
that it is sufficient to install just the command-line tools from the Android
SDK. The entire Android Studio IDE is not necessary to obtain ADB.

When asked to test an Android build directory, `run_tests.py` will detect a
single connected Android device (including an emulator). If multiple devices are
connected, one may be chosen explicitly with the `ANDROID_DEVICE` environment
variable. `run_tests.py` will upload test executables and data to a temporary
location on the detected or selected device, run them, and clean up after itself
when done.

### Fuchsia

To test on Fuchsia, you need a connected device running Fuchsia. Run:

```
$ gn gen out/fuchsia --args 'target_os="fuchsia" target_cpu="x64" is_debug=true'
$ ninja -C out/fuchsia
$ python build/run_tests.py out/fuchsia
```

If you have multiple devices running, you will need to specify which device you
want using their hostname, for instance:

```
$ ZIRCON_NODENAME=scare-brook-skip-dried python build/run_tests.py out/fuchsia
```

## Contributing

Crashpad’s contribution process is very similar to [Chromium’s contribution
process](https://chromium.googlesource.com/chromium/src/+/main/docs/contributing.md).

### Code Review

A code review must be conducted for every change to Crashpad’s source code. Code
review is conducted on [Chromium’s
Gerrit](https://chromium-review.googlesource.com/) system, and all code reviews
must be sent to an appropriate reviewer, with a Cc sent to
[crashpad-dev](https://groups.google.com/a/chromium.org/group/crashpad-dev). The
[`codereview.settings`](https://chromium.googlesource.com/crashpad/crashpad/+/main/codereview.settings)
file specifies this environment to `git-cl`.

`git-cl` is part of the
[depot_tools](https://www.chromium.org/developers/how-tos/depottools). There’s
no need to install it separately.

```
$ cd ~/crashpad/crashpad
$ git checkout -b work_branch origin/main
…do some work…
$ git add …
$ git commit
$ git cl upload
```

Uploading a patch to Gerrit does not automatically request a review. You must
select a reviewer on the Gerrit review page after running `git cl upload`. This
action notifies your reviewer of the code review request. If you have lost track
of the review page, `git cl issue` will remind you of its URL. Alternatively,
you can request review when uploading to Gerrit by using `git cl upload
--send-mail`.

Git branches maintain their association with Gerrit reviews, so if you need to
make changes based on review feedback, you can do so on the correct Git branch,
committing your changes locally with `git commit`. You can then upload a new
patch set with `git cl upload` and let your reviewer know you’ve addressed the
feedback.

The most recently uploaded patch set on a review may be tested on a
[trybot](https://chromium.googlesource.com/chromium/src/+/main/docs/infra/trybot_usage.md)
by running `git cl try` or by clicking the “CQ Dry Run” button in Gerrit. These
set the “Commit-Queue: +1” label. This does not mean that the patch will be
committed, but the trybot and commit queue share infrastructure and a Gerrit
label. The patch will be tested on trybots in a variety of configurations.
Status information will be available on Gerrit. Trybot access is available to
Crashpad and Chromium committers.

### Landing Changes

After code review is complete and “Code-Review: +1” has been received from all
reviewers, the patch can be submitted to Crashpad’s [commit
queue](https://chromium.googlesource.com/chromium/src/+/main/docs/infra/cq.md)
by clicking the “Submit to CQ” button in Gerrit. This sets the “Commit-Queue:
+2” label, which tests the patch on trybots before landing it. Commit queue
access is available to Crashpad and Chromium committers.

Although the commit queue is recommended, if needed, project members can bypass
the commit queue and land patches without testing by using the “Submit” button
in Gerrit or by committing via `git cl land`:

```
$ cd ~/crashpad/crashpad
$ git checkout work_branch
$ git cl land
```

### External Contributions

Copyright holders must complete the [Individual Contributor License
Agreement](https://cla.developers.google.com/about/google-individual) or
[Corporate Contributor License
Agreement](https://cla.developers.google.com/about/google-corporate) as
appropriate before any submission can be accepted, and must be listed in the
[`AUTHORS`](https://chromium.googlesource.com/crashpad/crashpad/+/main/AUTHORS)
file. Contributors may be listed in the
[`CONTRIBUTORS`](https://chromium.googlesource.com/crashpad/crashpad/+/main/CONTRIBUTORS)
file.

## Buildbot

The [Crashpad Buildbot](https://ci.chromium.org/p/crashpad/g/main/console)
performs automated builds and tests of Crashpad. Before checking out or updating
the Crashpad source code, and after checking in a new change, it is prudent to
check the Buildbot to ensure that “the tree is green.”
