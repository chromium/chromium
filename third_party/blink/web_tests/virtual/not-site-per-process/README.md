# virtual/not-site-per-process

## Summary

Layout tests use the default site isolation from the platform they are
run on.  For example, strict site isolation (aka site-per-process) is
used on desktop platforms.

Additionally, on platforms where strict site isolation is enabled,
layout tests opt into slightly stricter isolation and enabling isolating
same-site origins used by Web Platform Tests. This ensures that features
covered by WPT also provide sufficient coverage of how these feature
behave in presence of out-of-process iframes.

Tests under `virtual/not-site-per-process` (defined in
[VirtualTestSuites](../../VirtualTestSuites)) are run with
`--disable-site-isolation-trials` cmdline flag which turns off site
isolation.  This is needed to preserve test coverage provided by around
60 tests that fail when run with site isolation.

Instead of including `http/tests/devtools/isolated-code-cache` tests here, we
split into two virtual test suites
`virtual/not-split-http-cache-not-site-per-process` and
`virtual/split-http-cache-not-site-per-process`, which disable and enable HTTP
cache partitioning, respectively. This split is needed as a test checks whether
cross-origin resources were cached.

## Tests incompatible with WPT origin isolation

Tests that modify `document.domain` are incompatible with isolation of WPT
origins. The tests need to stay under `virtual/not-site-per-process` forever.
See `exclusive_tests` in `not-site-per-process` section in
[VirtualTestSuites](../../VirtualTestSuites).

## Tests that need further investigation and/or decisions

Remaining tests need further investigation as they may either 1) hide
previously unknown OOPIF-related bugs or 2) expose known OOPIF-related
differences in product behavior or 3) expose known OOPIF-support issues
in tests or in the test harness.  Over time, such tests should be
removed from `virtual/not-site-per-process`.  These tests are covered
by `web_tests/TestExpectations` file.
