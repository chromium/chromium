# Capture blocking details Virtual Tests

This folder contains virtual test suites for the capturing blocking details for bfcache feature.
The suite runs `web_tests/http/tests/inspector-protocol/bfcache/report-back-forward-cache-status-detils-captured.js` with `--enable-features=RegisterJSSourceLocationBlockingBFCache`.

To manually run the suites, use the following command:

To run tests:

```bash
third_party/blink/tools/run_web_tests.py -t Default virtual/capture-blocking-details/
```