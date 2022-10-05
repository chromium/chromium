# Virtual Tests for BackForwardCache NotRestoredReasons

This folder contains virtual test suites to cover NotRestoredReasons feature.

The suite runs `external/wpt/performance-timeline/not-restored-reasons/` with `--enable-features=BackForwardCacheSendNotRestoredReasons`.

To manually run the suites, use the following command:

```bash
third_party/blink/tools/run_web_tests.py -t Default virtual/not-restored-reasons/external/wpt/performance-timeline/not-restored-reasons/
```