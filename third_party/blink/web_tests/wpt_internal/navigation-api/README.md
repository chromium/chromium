This directory contains tests of the Navigation API that require
window.internals or that verify behavior where we are not spec-compliant.

Currently, the directory contains a near-complete copy of the WPT navigation-api
suite. This tests the behavior of the deprecated transitionWhile() API, which is
scheduled for removal in M108.

The tests that are not part of the transitionWhile() regression suite are:
* navigate-from-initial-about-blank-gc.html
* sandboxing-back-parent-never-settles.html
* sandboxing-back-sibling-never-settles.html
