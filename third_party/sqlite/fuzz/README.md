# Fuzzing sqlite

"[Clusterfuzz](https://google.github.io/clusterfuzz/) is a scalable fuzzing
infrastructure which finds security and stabilty issues in software". Chromium
uses Clusterfuzz to find bugs in sqlite, among others.

Given access to a clusterfuzz test case, this README will describe how one can
reproduce and help diagnose sqlite bugs found by clusterfuzz.

Example bug: https://crbug.com/956851

# Simple automated repro
TODO: Move to [here](https://google.github.io/clusterfuzz/using-clusterfuzz/)?
To verify that the bug still repros on the current master branch:
1. Open the relevant bug (ex. https://crbug.com/956851).
2. Open the clusterfuzz "Detailed report" (ex. https://clusterfuzz.com/testcase?key=5756437473656832).
3. Click on the "REDO TASK" button.
4. Check on "Check if bug still reproduces", and click "Commit".
5. The bottom of the clusterfuzz "Detailed report" from (2) should reflect that
the "Redo task(s): progression" task has started.
6. Wait for a few hours to a day, and this link should update to reflect that
the "Progression task finished.". If the bug has been fixed in master, then it
will automatically be closed. Otherwise, the bug still repro's, and the updated
stack trace will be displayed in the "Detailed report".

# Local repro context
1. Run from your Chromium source directory.

# Local repro using clusterfuzz testcase id
If the fuzzer that identified this bug is public (ex. dbfuzz2), reproduce
locally using the [Reproduce Tool](https://github.com/google/clusterfuzz-tools).
1. `export TESTCASE_ID=5756437473656832 # Set ${TESTCASE_ID}, where TESTCASE_ID is the ID at the end of the clusterfuzz link`
2. `/google/data/ro/teams/clusterfuzz-tools/releases/clusterfuzz reproduce --current --skip-deps ${TESTCASE_ID}`

# Local repro using clusterfuzz testcase
If the fuzzer is not public (ex. LPM-based fuzzers, including fts_lpm), or if
more data is needed, reproduce a bit more manually by first building the target.
To build the target, first set .gn args to match those in the clusterfuzz link,
then build and run the fuzzer.

1. `export FUZZER_NAME=sqlite3_fts3_lpm_fuzzer  # FUZZER_NAME is listed in the crbug as the "Fuzz target binary"`
2. Download the clusterfuzz minimized testcase.
3. `export CLUSTERFUZZ_TESTCASE=./clusterfuzz-testcase-minimized-sqlite3_fts3_lpm_fuzzer-5756437473656832  # Set the clusterfuzz testcase path to CLUSTERFUZZ_TESTCASE`
3. `gn args out/Fuzzer  # Set arguments to matches those in the clusterfuzz "Detailed report"'s "GN CONFIG (ARGS.GN)" section`
4. `autoninja -C out/Fuzzer/ ${FUZZER_NAME}  # Build the fuzzer target`
5. `./out/Fuzzer/${FUZZER_NAME} ${CLUSTERFUZZ_TESTCASE}  # Verify repro by running fuzzer (for memory leaks, try setting "ASAN_OPTIONS=detect_leaks=1")`
6. `LPM_DUMP_NATIVE_INPUT=1 SQL_SKIP_QUERIES=AlterTable ./out/Fuzzer/${FUZZER_NAME} ${CLUSTERFUZZ_TESTCASE}  # Try using different args to get SQL statements that will repro the bug. SQL_SKIP_QUERIES can help minimize the repro`
7. Optionally, minimize the testcase further using the `-minimize_crash`
[flag](https://chromium.googlesource.com/chromium/src/+/master/testing/libfuzzer/reproducing.md#minimizing-a-crash-input-optional).
8. Optionally, take output from (7) into a repro.sql file for further testing.
To do so, either copy the SQL query in the output from (6) into a .sql file, or
run the final command in (7) with a `> repro.sql` at the end, and filter out
non-sql content afterwards. Either way, ensure that the case continues to repro
given filters placed in (7).

# Local repro using SQL commands
Please have a .sql file with SQL queries ready. We'll refer to this file as
repro.sql.
1. `autoninja -C out/Fuzzer/ sqlite_shell  # Build the sqlite_shell`
2. `out/Fuzzer/sqlite_shell < repro.sql  # Try running this sql query in sqlite`