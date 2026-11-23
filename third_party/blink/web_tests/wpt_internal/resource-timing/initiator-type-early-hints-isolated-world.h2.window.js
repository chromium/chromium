// META: script=/common/utils.js

// Run the test in a popup, which reports its subtests back to this window, so
// that this window isn't navigated away (which would race with the harness).
const url = new URL('resources/initiator-type-early-hints-isolated-world.h2.py',
                    window.location);
// Make the preloaded URLs unique, so that they can't have been loaded before.
url.searchParams.set('token', token());
const popup = window.open(url);
add_completion_callback(() => popup.close());
fetch_tests_from_window(popup);
