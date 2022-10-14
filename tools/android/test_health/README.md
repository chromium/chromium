# Android Test Health

## Overview

This directory contains helper modules and scripts for extracting test health
data from a Git repository. Test health data refers to, e.g., counts of disabled
or flaky tests.

## Get Test Health Script

The `get_test_health.py` script extracts Java test health from a Git repository.
The script defaults to the Chromium repository containing this script itself.
The test health data includes a listing of tests that are disabled (annotated as
`@DisabledTest`) or conditionally-disabled (`@DisableIf`).
The script exports the data in newline-delimited JSON
([JSON Lines](http://jsonlines.org)) format so that it can be easily
[ingested into BigQuery][bq-load-gcs-json].

[bq-load-gcs-json]:
  https://cloud.google.com/bigquery/docs/loading-data-cloud-storage-json

### Usage

```sh
usage: get_test_health.py [-h] -o OUTPUT_FILE [--git-dir GIT_DIR]
                          [--test-dir TEST_DIR]

Gather Java test health information for a Git repository and export it as
newline-delimited JSON.

optional arguments:
  -h, --help            show this help message and exit
  -o OUTPUT_FILE, --output-file OUTPUT_FILE
                        output file path for extracted test health data
  --git-dir GIT_DIR     root directory of the Git repository to read (defaults
                        to the Chromium repo)
  --test-dir TEST_DIR   subdirectory containing the tests of interest;
                        defaults to the root of the Git repo
```

## Helper Modules

### Java Test Utilities

The `java_test_utils` module contains utility functions to extract counts of
test cases annotated with `@DisabledTest` and `@DisableIf`, as well as the
Java package name, from the source code of Java test files.

### Other Modules

The remaining modules are primarily designed for internal use by the
`get_test_health.py` script:

*   `test_health_extractor` iterates over all Java tests in a repository
*   `test_health_exporter` writes test health data to newline-delimited JSON
    files
