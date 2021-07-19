# Fuzzing SQLite

"[ClusterFuzz](https://google.github.io/clusterfuzz/) is a scalable fuzzing
infrastructure which finds security and stabilty issues in software". Chromium
uses ClusterFuzz to find bugs in SQLite, among others. One can view SQLite
Fuzzing coverage [here](https://chromium-coverage.appspot.com/reports/709707_fuzzers_only/linux/chromium/src/third_party/sqlite/amalgamation/report.html),
with more detailed data [here](https://clusterfuzz.com/fuzzer-stats?fuzzer=libFuzzer_sqlite3_lpm_fuzzer).

Given access to a ClusterFuzz test case, this README will describe how one can
reproduce and help diagnose SQLite bugs found by ClusterFuzz.

Example bug: https://crbug.com/956851

# Simple automated repro

To verify that the bug still repros on the current main branch:
1. Open the relevant bug (ex. https://crbug.com/956851).
2. Open the ClusterFuzz "Detailed report" (ex. https://clusterfuzz.com/testcase?key=5756437473656832).
3. Click on the "REDO TASK" button.
4. Check on "Check if bug still reproduces", and click "Commit".
5. The bottom of the ClusterFuzz "Detailed report" from (2) should reflect that
the "Redo task(s): progression" task has started.
6. Wait for a few hours to a day, and this link should update to reflect that
the "Progression task finished.". If the bug has been fixed in main, then it
will automatically be closed. Otherwise, the bug still repro's, and the updated
stack trace will be displayed in the "Detailed report".

# Local repro setup

1. Run from your Chromium source directory.

# Local repro using ClusterFuzz testcase

SQLite authors and non-Chromium contributors may need more data in order to
reproduce SQLite bugs originating from Chromium fuzzers if:
* The fuzzer is not public (ex. LPM-based fuzzers, including fts_lpm), or
* The fuzzer's content is serialized in a custom manner, like via protobuf
  (ex. LPM-based fuzzers as well).

In these cases, you may need to reproduce the testcase manually on your local
environment, and also provide a SQL query that can reproduce the issue, by
deserializing the fuzzer reproduce testcase.

To reproduce the testcase:

1. `export FUZZER_NAME=sqlite3_fts3_lpm_fuzzer  # FUZZER_NAME is listed in the bug as the "Fuzz Target"`
2. Download the ClusterFuzz minimized testcase.
3. `export CLUSTERFUZZ_TESTCASE=./clusterfuzz-testcase-minimized-sqlite3_fts3_lpm_fuzzer-5756437473656832  # Set the ClusterFuzz testcase path to CLUSTERFUZZ_TESTCASE`
3. `gn args out/Fuzzer  # Set arguments to match those in the ClusterFuzz "Detailed report"'s "GN CONFIG (ARGS.GN)" section`
4. `autoninja -C out/Fuzzer/ ${FUZZER_NAME}  # Build the fuzzer target`
5. `./out/Fuzzer/${FUZZER_NAME} ${CLUSTERFUZZ_TESTCASE}  # Verify repro by running fuzzer (for memory leaks, try setting "ASAN_OPTIONS=detect_leaks=1")`

After this, to obtain a shareable SQLite query testcase:

1. `LPM_DUMP_NATIVE_INPUT=1 SQL_SKIP_QUERIES=AlterTable ./out/Fuzzer/${FUZZER_NAME} ${CLUSTERFUZZ_TESTCASE}  # Try using different args to get SQL statements that will repro the bug`
SQL_SKIP_QUERIES can help minimize the repro, LPM_DUMP_NATIVE_INPUT can dump a
SQLite query as output from a LPM fuzzer testcase, and DUMP_NATIVE_INPUT can
dump a SQLite query as output from a
[shadow_table_fuzzer](https://source.chromium.org/chromium/chromium/src/+/main:third_party/sqlite/fuzz/shadow_table_fuzzer.cc)
testcase.
2. Optionally, minimize the testcase further using the `-minimize_crash`
[flag](https://chromium.googlesource.com/chromium/src/+/main/testing/libfuzzer/reproducing.md#minimizing-a-crash-input-optional).
3. Optionally, take output from (2) into a repro.sql file for further testing.
To do so, either copy the SQL query in the output from (1) into a .sql file, or
run the final command in (2) with a `> repro.sql` at the end, and filter out
non-SQL content afterwards. Either way, ensure the testcase continues to repro
given filters placed in (2).

# Local repro using SQL commands

Please have a .sql file with SQL queries ready. We'll refer to this file as
repro.sql. This may optionally be generated in the previous section, when
getting a `DUMP_NATIVE_INPUT` from `./out/Fuzzer/${FUZZER_NAME}`.
1. `autoninja -C out/Fuzzer/ sqlite_shell  # Build the sqlite_shell`
2. `out/Fuzzer/sqlite_shell < repro.sql  # Try running this sql query in SQLite`

Optionally, test cases may be further minimized by deleting lines/sections in
repro.sql, until the crash no longer reproduces.
