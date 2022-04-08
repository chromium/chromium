Standalone Support
====

This directory serves as a home for minimal polyfill of some common APIs within
Chromium, `//base` in particular. This allows the ipcz implementation to depend
on a small subset of the Chromium tree when building against Chromium, but to
still build and behave as expected when used as a standalone library.

Currently only a subset of logging macros, and stack trace symbolization are
polyfilled here. Internal ipcz code which includes `util/log.h` will get
Chromium's `base/logging.h` definitions when building as part of Chromium; when
building standalone, this will instead include `standalone/base/logging.h`.

Similarly, inclusion of `util/stack_trace.h` exposes the ipcz::StackTrace type.
For Chromium builds this is merely an alias for `base::debug::StackTrace` in
`base/debug/stack_trace.h`. In standalone builds, the implementation comes from
`standalone/base/` and is built around Abseil's stack tracing and symbolization.

Any additional polyfill in this directory should be added sparingly and with
careful consideration of maintenance costs, since the vast majority of the
Chromium tree does not expose stable APIs. Log macros, for example, have been
extremely consistent over the lifetime of the Chromium project and are likely to
to remain that way, so they're a reasonably good fit here.
