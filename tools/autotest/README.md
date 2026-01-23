# Autotest

The script eliminates the manual overhead of finding which build target contains a specific test.
Whether you provide a filename, a directory, or just a test case name, the script:

1. **Resolves** the input into concrete file paths.
2. **Identifies** the correct GN build targets.
3. **Compiles** the targets using AutoNinja.
4. **Executes** the tests with the appropriate filters.

---

## Usage & Examples

In the case when the Current Working Directory (CWD) is not the build directory or the environment
variable export CHROMIUM_OUTPUT_DIR is not set, the script requires a build directory specified
via `-C`.

### Common Commands

* **Run a specific file:**
`autotest.py base/pickle_unittest.cc`
* **Run all tests in a directory:**
`autotest.py -C out/Default base/strings/`
* **Search and run by test name:**
`autotest.py -C out/Default StringUtilTest.IsStringUTF8`
* **Run tests modified in your current Git branch:**
`autotest.py -C out/Default --run-changed`
* **Run a specific line (useful for editor integration):**
`autotest.py -C out/Default --line 42 base/pickle_unittest.cc`

To see other command line flags run with `--help`

---

## Pro Tips

* **Passthrough Arguments:** Any arguments not recognized by `autotest.py` are passed directly
to the underlying test runner.
* **Performance:** If you are working in a large repository within COG, it is recommended to
use `-r` if `cs` is available to avoid slow local disk I/O.