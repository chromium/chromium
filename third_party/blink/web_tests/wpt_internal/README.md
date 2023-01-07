This directory is the Chromium-internal (*) counterpart of external/wpt. All WPT
features (except `wpt lint`) are enabled in this directory, including wptserve.
This directory is mapped to wpt_internal/ on wptserve.

When including additional scripts from tests within this folder, the "root"
folder is "external/wpt" and paths should be relative to that. (E.g. "/resources
/testharness.js" will reference external/wpt/resources/testharness.js).

(*) "Internal" in the sense that tests are not synchronized with the WPT
upstream (https://github.com/web-platform-tests/wpt) or other browser vendors.

This directory is primarily intended for testing non-web-exposed and/or
Blink-specific behaviours (Blink internal testing APIs are allowed) with WPT
goodness (wptserve, testharness, reftest, etc.). Please try to use external/wpt
whenever possible.

Note: tests have to go into subdirectories. Files in the root level of
wpt_internal are not recognized as tests.
