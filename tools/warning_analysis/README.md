This folder contains scripts for discovering and cataloguing instances of
compiler warnings in Chromium builds.

### Background
There are currently many clang warnings which we suppress when building
Chromium, but intend to one day re-enable (try searching for `-Wno` in
[build/config/compiler.BUILD.gn](https://source.chromium.org/chromium/chromium/src/+/main:build/config/compiler/BUILD.gn)
for some examples). Unfortunately, actually turning these warnings back on is a
daunting task due to the sheer variety of platforms on which chromium needs to
build. Each of these compiles a different subset of the codebase, meaning that
certain warnings may only show up when building on a single, obscure system.

The Chromium CQ lets us check for this by building on many different systems
simultaneously. If we disable the `-Werror` flag, the build logs for each system
will contain a list of all the warnings that occurred during the build. However,
the CQ is quite large. It's not feasible to manually inspect the logs for every
single build to ensure all possible warning sites are caught.

The scripts in this directory serve to automate that process. `pull_logs.py`
uses the bitbucket cli tool `bb` (which must be in the PATH) to pull the build
logs for all trybots from a given CQ run, then `collect_warnings.py` can process
them to determine all the places in the codebase where a warning occurs. With
that information, it's feasible to begin fixing the occurrences.

### Intended use
The scripts are flexible, but they are intended for use as part of the following
workflow:
1. Enable the warning of interest by commenting out the relevant line in the
   BUILD file.
2. Disable `-Werror` by setting `treat_warnings_as_errors` to `false` in
   [build/config/compiler/compiler.gni](https://source.chromium.org/chromium/chromium/src/+/main:build/config/compiler/compiler.gni;l=50;drc=e1c0ef1369e527d7027782d1285df483e29a200a).
3. Create a CL with the above changes, and start a CQ Dry Run.
4. When the dry run finishes, run `pull_logs.py` with the CL and patchset number
   the CQ ran on.
5. When the logs have been downloaded, run `collect_warnings.py` with the
   warning flag you're interested in.

### Notable script options
The section describes particularly notable behavior. See the scripts themselves
for a full list of options. Almost all options have an equivalent single-letter
form.

`pull_logs.py`
* The script pulls the build logs for a single trybot step (by default, the
compilation step). The same step can have different names on different builders,
so the `--step` argument may be specified multiple times. If so, the scripts
tries to pull logs from each step name in order until one succeeds.
* By default, the logs are written to a temporary folder whose name is printed
to stdout. You can specify the destination with the `--log-dir` argument.
* The resulting build logs can be extremely large, but most of the lines are
uninteresting boilerplate. The `--filter` flag may be used to automatically
remove uninteresting lines before saving the log to disk. The definition of
"interesting" is controlled by a lambda defined in the script file; by default,
all lines beginning with `[` are pruned. Feel free to edit the lambda locally
if you want a different filter.
* If the `--delete-logs` flag is passed, all `.txt` files in the log
directory will be deleted before the new ones are downloaded. Not using this
argument can result in having stale files in the log directory, preventing
accurate collection of warnings. The argument is disabled by default to prevent
unintentional deletion of data.

`collect_warnings.py`
* The `--warning` flag is required to determine which lines correspond to the
warning of interest. The value of the flag should be the ending text on the line
where the compiler announces the warning, e.g.
`--warning [-Wthread-safety-reference-return]`.
A convenient shortcut to collect all warnings in the file (frequently there will
only be one type of warning anyway) is to simply pass `--warning ]`.
* By default, the collected info is written to a temporary file whose name is
printed to stdout. You can specify your own output file using the `--output`
argument. If the value is `stdout` or `-`, the text will be printed to
stdout instead of a file.
* The scripts has two "modes": by default, it outputs a `.json` file with
detailed information about each warning emitted. If the `--summarize` flag is
passed, it will instead output a more human-readable `.txt` file which simply
lists every warning site, along with a count of number of files and warnings
emitted total.
* The detailed information consists of a list of warnings for each source file.
Each warning entry contains the line and character number, as well as the line
of code which triggered the warning. Finally, it contains a list of builders on
which the warning occurred.

### Example invocations
Pull logs for cl 1234567, patchset 8. Logs will be stored in the default
location
(`tools/warning_analysis/build_logs`).

`python3 tools/warning_analysis/pull_logs.py -cl 12345678 -p 8`

Pull logs for cl 1234567, patchset 8. Delete any old logs, filter out
uninteresting  lines from the new logs, and print progress to the console as the
script is running.

`python3 tools/warning_analysis/pull_logs.py -cl 12345678 -p 8 -d --filter -v`

Collect instances of clang's C++ extension warning from the default
log location, and store a summary in `thread_safety.txt`.

`python3 tools/warning_analysis/collect_warnings.py -w [-Wvla-cxx-extension] -o cxx_extension --summarize`

Collect instances of any warning, and store the detailed output in `out.json`.

`python3 tools/warning_analysis/collect_warnings.py -w ]`
