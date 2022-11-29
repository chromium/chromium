# WebEngine

WebEngine is an embedded web UI component offering a modern, secure set of web
embedding capabilities as part of Android.

WebEngine is based on the WebLayer project; a high level embedding API to
support building a browser.

Note: The WebLayer API is deprecated as of M108, and WebLayer is being merged
into the WebEngine project.

Unlike `src/content`, which is only concerned with a sandboxed multi-process web
platform, WebLayer includes modern browser features and Google integration.
It's the reusable version of Chrome, which might share some portions of the UI
and also its support for all the modern HTML5 and browser features (e.g. UI for
permissions, autofill, safe browsing etc...).

While it's built on top of `src/content`, the expectation is that the API will
hide the Content API.

WebEngine further abstracts away the full set of capabilities of WebLayer, with
a simple API surface focused on the following goals:

* Security - protecting the web content by adding a security barrier between the
  embedder/host and the web content
* Performance - improving the embedder/host's responsiveness by offloading the
  browser initialization
* Stability - decoupling browser crashes from the embedder/host crashes
* Modernness - receiving full web platform support for free, while hiding
  subprocesses and the Content API.

Most of these goals can be achieved via moving the _browser_ component to run
in a sandbox. While such as a sandbox does not yet exist within Android,
WebEngine is still being developed with the aim to eventually create a security
boundary between the embedder/host app and the browser. A non-sandboxed mode
with the same API surface is being developed for compatibility, and a sandboxed
mode with a limited makeshift sandbox is also being developed for testing.

Note: _weblayer_ is still referenced a lot in this directory, all references will
eventually be changed to _webengine_.

## Resources and Documentation

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

If you haven't done this already, you first need to set up an Android build.
[Android build instructions](https://source.chromium.org/chromium/chromium/src/+/main:docs/android_build_instructions.md).

## Building and Testing

To run the sample app:

```
    $ autoninja -C out/Default run_webengine_shell_local
    $ out/Default/bin/run_webengine_shell_local
```

To run the sample app with a browsing sandbox (limited capabilities):

```
    $ autoninja -C out/Default run_webengine_shell
    $ out/Default/bin/run_webengine_shell
```

To run instrumentation tests:

```
    $ autoninja -C out/Default webengine_support_instrumentation_test_apk
    $ out/Default/bin/run_webengine_support_instrumentation_test_apk
```

The scripts will build and install all necessary APKs.
