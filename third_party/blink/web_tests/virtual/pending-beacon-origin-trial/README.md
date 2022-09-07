# PendingBeacon Virtual Tests for Origin Trial

This folder contains virtual test suites to cover that the PendingBeacon feature should be enabled in a document with Origin Trial enabled.

The suite runs `http/tests/origin_trials/webexposed/pending-beacon-origin-trial-interfaces.html` with `--enable-features=PendingBeaconAPI:requires_origin_trial/true`.

To manually run the suites, use the following command:

```bash
third_party/blink/tools/run_web_tests.py -t Default virtual/pending-beacon-origin-trial/http/tests/origin_trials/webexposed/
```
