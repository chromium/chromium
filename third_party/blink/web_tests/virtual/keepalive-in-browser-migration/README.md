# Fetch Keepalive Infra Migration Virtual Tests

This folder contains virtual test suites for the keepalive infra migration.
The suite runs the tests in `third_party/blink/web_tests/VirtualTestSuites`
with the argument `--enable-features=KeepAliveInBrowserMigration`:

To manually run the virtual test suites, use the following command:

1. To run all tests:

```bash
third_party/blink/tools/run_web_tests.py -t Default virtual/keepalive-in-browser-migration
```

2. To run a single test:

```bash
third_party/blink/tools/run_web_tests.py -t Default virtual/keepalive-in-browser-migration/external/wpt/fetch/api/basic/keepalive.html
```
