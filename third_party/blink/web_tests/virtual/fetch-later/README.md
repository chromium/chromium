# FetchLater Virtual Tests

This folder contains virtual test suites for the fetchLater API.
The suite runs `web_tests/external/wpt/fetch/fetch-later/` with `--enable-features=FetchLaterAPI`.

To manually run the suites, use the following command:

To run all tests:

```bash
third_party/blink/tools/run_web_tests.py -t Default virtual/fetch-later/
```

To run single test:

```bash
third_party/blink/tools/run_web_tests.py -t Default virtual/fetch-later/external/wpt/fetch/fetch-later/basic.tentative.https.window.html
```
