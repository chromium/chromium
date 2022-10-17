# PendingBeacon Virtual Tests

This folder contains virtual test suites for the PendingBeacon feature.
The suite runs `web_tests/external/wpt/pending-beacon/` with `--enable-features=PendingBeaconAPI`.

To manually run the suites, use the following command:

To run all tests:

```bash
third_party/blink/tools/run_web_tests.py -t Default virtual/pending-beacon/
```

To run single test:

```bash
third_party/blink/tools/run_web_tests.py -t Default virtual/pending-beacon/external/wpt/pending-beacon/pending_beacon-basic.tentative.window.html
```
