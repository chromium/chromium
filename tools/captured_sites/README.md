# Captured Sites Automation Framework

## Overview

This directory contains helper scripts for recording, replaying, and running
Captured Sites interactive UI tests.  These tests use recorded network traffic
to provide a hermetic replay environment to validate user flows.

## Control Script

The `control.py` script is the main entry point for these tests.

### Usage
```sh
usage: Runs captured sites framework recording and tests.

  $ tools/captured_sites/control.py [command] [arguments]
Commands:
  chrome  Starts a Chrome instance with autofill hooks
  wpr     Starts a WPR server instance to record or replay
  run     Starts a test for a single site or "*" for all sites
  refresh Starts a test for a single site or "*" for all sites, and records new
          server prediction responses.

Use "control.py [command] -h" for more information about each command.'.
```

## Test suites

Test suites can be added for different validation flows.

Current test suites include:
* "Autofill" which validates address and credit card filling functionality.
* "Password" which validates password generation and filling functionality.

## Additional Information

See the google internal design doc for more details pertaining to this tool:
https://docs.google.com/document/d/1sDmQbQnJWDU_U73UNMbk5JSggumR3HCFb1iPtMgJBfU