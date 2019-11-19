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

Tests under `virtual/not-site-per-process` are run with
`--disable-site-isolation-trials` cmdline flag which turns off site
isolation.  This is needed to preserve test coverage provided by around
60 tests that fail when run with site isolation.

Instead of including `http/tests/devtools/isolated-code-cache` tests here, we
split into two virtual test suites
`virtual/not-split-http-cache-not-site-per-process` and
`virtual/split-http-cache-not-site-per-process`, which disable and enable HTTP
cache partitioning, respectively. This split is needed as a test checks whether
cross-origin resources were cached.

When modifying the list of files that behave differently with and without
OOPIFs, please consider modifying all the locations below:
- LayoutTests/VirtualTestSuites (virtual/not-site-per-process suite)
- LayoutTests/virtual/not-site-per-process/README.md
- LayoutTests/TestExpectations and/or LayoutTests/NeverFixTests
  ("Site Isolation failures" section)


## Tests incompatible with WPT origin isolation

The following tests modify `document.domain` and are therefore incompatible with
isolation of WPT origins.  The tests need to stay under
`virtual/not-site-per-process` forever.  These tests are covered by
`LayoutTests/NeverFixTests` expectations file.

- external/wpt/html/browsers/origin/relaxing-the-same-origin-restriction
- external/wpt/FileAPI/url/multi-global-origin-serialization.sub.html
- external/wpt/dom/events/EventListener-incumbent-global-1.sub.html
- external/wpt/dom/events/EventListener-incumbent-global-2.sub.html
- external/wpt/html/browsers/history/the-location-interface/allow_prototype_cycle_through_location.sub.html
- external/wpt/html/browsers/history/the-location-interface/location-prototype-setting-same-origin-domain.sub.html
- external/wpt/html/browsers/origin/cross-origin-objects/cross-origin-objects-on-new-window.html
- external/wpt/html/browsers/the-windowproxy-exotic-object/windowproxy-prototype-setting-same-origin-domain.sub.html
- external/wpt/html/infrastructure/safe-passing-of-structured-data/shared-array-buffers/window-domain-success.sub.html
- external/wpt/html/infrastructure/safe-passing-of-structured-data/shared-array-buffers/window-similar-but-cross-origin-success.sub.html
- external/wpt/wasm/serialization/module/window-domain-success.sub.html
- external/wpt/wasm/serialization/module/window-similar-but-cross-origin-success.sub.html

## Tests that need further investigation and/or decisions

Remaining tests need further investigation as they may either 1) hide
previously unknown OOPIF-related bugs or 2) expose known OOPIF-related
differences in product behavior or 3) expose known OOPIF-support issues
in tests or in the test harness.  Over time, such tests should be
removed from `virtual/not-site-per-process`.  These tests are covered
by `LayoutTests/TestExpectations` file.
