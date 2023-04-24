# Attribution Reporting Tests

## Table of Contents

1.  [Overview](#overview)
2.  [General Guidelines](#general-guidelines)
3.  [Helper functions](#helper-functions)
4.  [Sample Test](#sample-test)
5.  [Running Tests](#running-tests)
6.  [Server Code](#server-code)

## Overview

The primary objective here is to be able to test the Attribution Reporting APIs.
This involves testing
[Event Level Reporting APIs](https://github.com/WICG/conversion-measurement-api/blob/main/EVENT.md)
and
[Aggregatable Reporting APIs](https://github.com/WICG/conversion-measurement-api/blob/main/AGGREGATE.md).
Please refer to the relevant documentation for API details.

## General Guidelines

1.  These tests are currently internal only. They must be added to
    `wpt_internal/attribution-reporting/` directory. Please note that if tests
    are added to a different directory, they could run in parallel, This could
    cause issues with shared stash. For this reason, **add all Attribution
    Reporting tests to this directory only**.
2.  The tests rely on using server-side stash for storing cross-request data.
    Each test is expected to clean up after itself. Specific helper methods are
    provided to achieve this (Described below).
3.  All tests must be
    [WPT JavaScript Tests](https://web-platform-tests.org/writing-tests/testharness.html)
4.  The test files should end with `.sub.https.html` to access information like
    `host`, etc. Please refer to the
    [Server Features](https://web-platform-tests.org/writing-tests/server-features.html#tests-involving-multiple-origins)
    section in the WPT docs for more details.

## Helper functions

A set of helper functions are available in `resources/helpers.js` for your
convenience. They can be included in tests by adding the following line to your
test. The examples are for Event-Levelt Reporting but the same can be applied
for Aggregatable Reporting.

```html
<script src="resources/helpers.js"></script>
```

### resetEventLevelReports and resetAggregatableReports

Removes any data from the stash. This is recommended to run at the beginning of
your test to ensure you have a fresh stash for your test. You can use this as

```javascript
// then syntax
resetEventLevelReports().then(callback...);

// await syntax
await resetEventLevelReports();
```

### registerAttributionSrc

This works to register a source or a trigger. You just need to pass the body of
the headers that you want to register.

```javascript
const sourceEvent = {...};
registerAttributionSrc({ source: sourceEvent });
```

The
[wptserve Pipes](https://web-platform-tests.org/writing-tests/server-pipes.html)
are used behind the scenes. Pipes are functions that may be used when serving
files to alter parts of the response.

### delay

A simple delay function that takes time in ms and waits for that long.

```javascript
// then syntax
delay(100).then(callback...);

// await syntax
await delay(100);
```

### pollEventLevelReports and pollAggregatableReports

Polls the server for Event-Level Reports or Aggregatable Reports respectively.
An interval is accepted which tells the function how much time (in ms) to wait
in between requests. The method keeps polling until at least 1 report is
returned by the server or the test times out. Please note that receiving reports
from the server is a destructive operation on the server-side. This would
essentially clear the server of all the reports.

### waitForSourceToBeRegistered

Waits for a previously initiated source registration `registerAttributionSrc` to
complete. Please note that a source can be "waited on" once.

```javascript
// then syntax
pollEventLevelReports(100).then(callback...);

// await syntax
await pollEventLevelReports(100);
```

## Sample test

Please refer to the `simple-event-level-report-test.sub.https.html` test. It is
a basic test that utilizes helpers for Event-Level reports.

## Running Tests

Attribution Reporting APIs add noise to the report content and delay to report
delivery. In order for the tests to run without this noise and delay, chrome
must run with command-line switch `--attribution-reporting-debug-mode`. For this reason,
all Attribution Reporting tests are virtual tests. You can run the tests by

```shell
# Build WPT
autoninja -C out/Default blink_tests

# Run all Attribution Reporting tests
third_party/blink/tools/run_web_tests.py -t Default virtual/attribution-reporting-debug-mode/wpt_internal/attribution-reporting

# Run a single test
third_party/blink/tools/run_web_tests.py -t Default virtual/attribution-reporting-debug-mode/wpt_internal/attribution-reporting/<test-name>.sub.https.html
```

## Server Code

All the server code lives in `external/wpt/.well-known/attribution-reporting/`
directory. This is due to the fact that the browser POSTs the reports to
`/.well-known/...` endpoint. The files of interest are

```shell
/.well-known/attribution-reporting/report-event-attribution
/.well-known/attribution-reporting/report-aggregate-attribution
```

These files don't have a `.py` extension but must be treated as python files.
They are registered as python scripts with the WPT server router. You can check
the
[tools/serve/serve.py](https://github.com/web-platform-tests/wpt/blob/master/tools/serve/serve.py#L573-L574)
file for details.

This code handles receiving the reports from the browser and the returning the
reports when requested. The server utilizes
[WPT Python Handlers](https://web-platform-tests.org/writing-tests/python-handlers/index.html)
to achieve this.
