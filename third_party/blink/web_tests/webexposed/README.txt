The results in this directory serve to document Blink's complete API
as "visible" (exposed to scripts) during testing. These tests are run
with a default set of runtime flags enabled, including experimental
features and test flags.

web_tests/virtual/stable/webexposed/ runs the same tests without the
experimental and test flags enabled. It is testing what is exposed to
the stable channel. Changes there may require approval from blink API
OWNERS.

## Global Interface Listing Tests

Additions to Blink IDL files with new interfaces, methods and
attributes should result in changes to the expectations here. Such
changes should normally be made behind experimental flags (e.g.
[RuntimeEnabled=flag] in the IDL), and should not change the
virtual/stable expectations. To learn more about runtime flags, see:
third_party/blink/renderer/platform/RuntimeEnabledFeatures.md

To learn more about Blink IDL files, start with:
third_party/blink/renderer/bindings/README.md

NOTE: Changes to the inheritance of an existing interface (e.g.
changing an interface to derive from EventTarget) can't be guarded by
runtime flags, and will introduce virtual/stable changes.

The tests enumerate global interfaces, methods, getters/setters (how
most IDL attributes are exposed) and attributes (other IDL
attributes). They do not assert types or parameters. Individual
features typically have idlharness.js-based tests that live in
web_tests/external/wpt which more thoroughly exercise each API. See:
https://web-platform-tests.org/writing-tests/idlharness.html


The test files live in web_tests/webexposed:

* global-interface-listing.html - Window context
* global-interface-listing-dedicated-worker.html - Dedicated Worker context
* global-interface-listing-shared-worker.html - Shared Worker context

But also in web_tests/http/tests/serviceworker/webexposed:

* global-interface-listing-service-worker.html - Service Worker context

And also in web_tests/http/tests/worklet/webexposed:

* global-interface-listing-paint-worklet.html - Worklet contexts
