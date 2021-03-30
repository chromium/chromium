# This suite runs tests with --enable-features=WebAppWindowControlsOverlay

We added a empty expected file so that when the virtual test suite runs, it'll use this empty expected
file for the test run instead of the normal expected test file here:

third_party/blink/web_tests/external/wpt/html/webappapis/system-state-and-capabilities/the-navigator-object/navigator-window-controls-overlay-expected.txt