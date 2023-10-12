# Chromium SQLite.
This is the top folder for Chromium's [SQLite](https://www.sqlite.org/). The
actual SQLite source is not in this repository, but instead cloned into the
`src` directory from https://chromium.googlesource.com/chromium/deps/sqlite.

The directory structure is as follows. Files common to all third_party projects
(ex. BUILD.GN, OWNERS, LICENSE) are omitted.

* `src/`     The Chromium fork of SQLite (cloned via top level DEPS file).
* `scripts/` Scripts that generate the files in the amalgamations in src/.
* `sqlite.h` The header used by the rest of Chromium to include SQLite. This
             forwards to src/amalgamation/sqlite3.h
* `fuzz/`    Google OSS-Fuzz (ClusterFuzz) testing for Chromium's SQLite build.

## Amalgamations

[SQLite amalgamations](https://www.sqlite.org/amalgamation.html) are committed
to the SQLite Chromium repository (in `src`), but are created by a script that
lives in the Chromium repository. This is because the configuration variables
for building and amalgamation generation are shared.

There are two amalgamations:
* //third_party/sqlite/src/amalgamation is shipped, tested, and Fuzzed by
  Chromium.
* //third_party/sqlite/src/amalgamation_dev is not distributed or tested by
  Chromium. It is used for some developer tools (either only for local
  development, or only on trusted input).

## [//third_party/sqlite/src](https://source.chromium.org/chromium/chromium/src/+/main:third_party/sqlite/src/) repository.

CLs in this repository cannot be submitted through the commit queue (ex. CQ+2),
because there is no commit queue / try bot support for this repository. Please
use the "Submit" button (in Gerrit's 3-dot menu on the top right) to submit CLs
in this repository instead.

# Playbook

## Upgrade to a new SQLite release.

SQLite should be upgraded as soon as possible whenever a new version is
available. This is because new versions often contain security and stability
improvements, and frequent upgrades allow Chromium to have minimal cherry-pick
diffs when requesting investigation for SQLite bugs discovered by Chromium
Fuzzers. New versions may be viewed [here](https://www.sqlite.org/changes.html),
and bugs for these upgrades may look like [this example](https://crbug.com/1161048).

Historically, Chromium fuzzers often find issues within 2 weeks after upgrading
to new SQLite versions. Avoid upgrading SQLite within 1-2 weeks of a Chromium
[branch point](https://chromiumdash.appspot.com/schedule) to allow fuzzers time
to run. However, if the new SQLite release contains known security or stability
fixes, upgrade once available and monitor fuzzers more closely.

SQLite version upgrades tend to be extremely large changes
([example](https://crrev.com/c/2601105)), for which the diffs are not possible
to thoroughly review.

**Note** SQLite tags all releases `version-<release number>`, e.g.
`version-3.40.0`. The Chromium project prefixes all tags/branches with
"chromium-", e.g.  `chromium-version-3.40.0`.

1. Create new release branch

   Create the branch at
   [Gerrit/branches](https://chromium-review.googlesource.com/admin/repos/chromium/deps/sqlite,branches). The branch name should
   look like `chromium-version-3.40.0` and the initial revision will look something like `refs/tags/upstream/version-3.40.0`.

   Note: To create a release branch, you must be listed as a member in the
   [sqlite-owners Gerrit group](https://chromium-review.googlesource.com/admin/groups/3cb0e9e73693fd6377da67b63a28b815ef5c94cc,members)

2. Checkout the new Chromium release branch.

    Get the version from the [README.chromium](https://source.chromium.org/chromium/chromium/src/+/main:third_party/sqlite/README.chromium).

    ```sh
    cd third_party/sqlite/src  # from //chromium/src
    git fetch origin
    export VERSION=3.40.0
    git checkout -b chromium-version-$VERSION \
        --track origin/chromium-version-$VERSION
    ```

3. Generate and commit the SQLite amalgamations.

    ```sh
    ./../scripts/generate_amalgamation.py
    git add amalgamation amalgamation_dev
    git commit -m "Amalgamations for release $VERSION"
    ```

4. Run local tests.

    Follow steps in [Running Tests](#running-tests) below to execute all
    verifications and tests.

5. Upload the new release branch for review.

    ```sh
    git cl upload
    ```

6. Roll the Chromium DEPS file.

    Once review above has merged:

    1. Roll the `chromium/src/DEPS` file to reference that new commit hash.
        ```sh
        roll-dep src/third_party/sqlite/src --roll-to <git hash of merged CL>
        ```
    2. Update the version in //third_party/sqlite/README.chromium. Amend the
       commit created by roll-dep above.

## Cherry-pick unreleased commit from SQLite.

Sometimes **critical fixes** land in SQLite's master, but are not yet in a
release. This may occur when other SQLite embedders find critical security
or stability issues that SQLite authors then fix, but are often detected by
Chromium ClusterFuzz as well.

If you're triaging a ClusterFuzz bug, an internal playbook on how to triage
and fix ClusterFuzz bugs is available at
[go/sqlite-clusterfuzz-bug-process](https://goto.google.com/sqlite-clusterfuzz-bug-process).

If changes need to be brought into the current release branch, please do the
following:

1. Checkout the current release branch.

    Get the version from the [README.chromium](https://source.chromium.org/chromium/chromium/src/+/main:third_party/sqlite/README.chromium).

    ```sh
    export VERSION=3.40.0
    cd third_party/sqlite/src  # from //chromium/src
    git checkout -b chromium-version-$VERSION \
      --track origin/chromium-version-$VERSION
    ```

2. Cherry-pick the change

    Git _can_ be used to cherry pick upstream changes into a release branch but
    the sqlite_cherry_picker.py script is preferred. This script automates a
    few tasks such as:

    * Identifying the correct Git commit hash to use if given the
      Fossil commit hash. **note this is currently broken and a Git hash must be provided**
    * Automatically calculating Fossil manifest hashes.
    * Skipping conflicted binary files.
    * Generating the amalgamations.

    Cherry-pick the commit:

    ```sh
    ../scripts/sqlite_cherry_picker.py <full git commit hash>
    ```

    If there is a conflict that the script cannot resolve then, like
    `git cherry-pick`, the script will exit and leave you to resolve the
    conflicts. Once resolved run the script a second time:

    ```sh
    ../scripts/sqlite_cherry_picker.py --continue
    ```

    If you have access to the SQLite fossil commit hash, and would like to map
    this to the corresponding git hash, you can use GitHub search. As SQLite's
    git repository's commits include the fossil hash, you can search for the
    fossil hash, using the following query with the fossil commit hash appended
    ([example search](https://github.com/sqlite/sqlite/search?type=commits&q=8c432642572c8c4b7251f413def0725b3b8e9e7fe10230aa0aabe86b58e5902d)):
    https://github.com/sqlite/sqlite/search?type=commits&q=

    If the cherry-picking script is unable to cherry-pick a commit, like in
    https://crbug.com/1162100, manually apply the change from a SQLite or git,
    in //third_party/sqlite/src's files modified in the SQLite tracker, like at
    https://sqlite.org/src/info/a0bf931bd712037e. From there, run
    `../scripts/generate_amalgamation.py` to propagate these changes over to
    the amalgamation files. sqlite_cherry_picker.py should generally be
    preferred, as it updates hashes and simplifies tracking.

3. Run local tests.

    Follow steps in [Running Tests](#running-tests) below to execute all
    verifications and tests.

4. Upload cherry-picked change (with amalgamations) for review.

  If the relevant bug is a security bug, make sure that the reviewers are cc'ed.
  Otherwise, they may not know what/why they're reviewing.

    ```sh
    git cl upload
    ```

5. Update the Chromium DEPS file.

    Once review above has merged, roll the `chromium/src/DEPS` file to
    reference that new commit hash.

    ```sh
    roll-dep src/third_party/sqlite/src --roll-to <git hash of merged CL>
    ```

## Running Tests

Build all desktop targets:

Check that `extract_sqlite_api.py` added "chrome_" to all exported symbols.
Only "_fini" and "_init" should be unprefixed, but are conditionally
exported by the linker and may be omitted.

```sh
autoninja -C out/Default
nm -B out/Default/libchromium_sqlite3.so | cut -c 18- | sort | grep '^T'
```

### Running unit tests

```sh
out/Default/sql_unittests
```

### Running web tests

```sh
third_party/blink/tools/run_web_tests.py -t Default storage/websql/
```

### Running SQLite's TCL test suite within the Chromium checkout.

This is one of the [SQLite test suites](https://www.sqlite.org/testing.html).
They take approximately 3 minutes to build and run on a fast workstation.

**Note**: Tests currently fail both locally and on Chromium release branches.
They fail on release branches because some tests rely on SQLite databases
(binary files) which are committed to the source and are likely not merged down
when cherry picked. It is safe to ignore these errors which should be
reasonably easy to identify based on the cherry picked upstream changes. Until
these tests are fixed, it is safe to ignore these tests when running SQLite test
suites.

```sh
cd third_party/sqlite  # from //chromium/src
./scripts/generate_amalgamation.py --testing
make --directory=src test | tee /tmp/test.log
```

Show tests with errors:

```sh
egrep 'errors out of' /tmp/test.log
```

Show broken tests:

```sh
egrep 'Failures on these tests:' /tmp/test.log
```

Broken tests will also show lines ending in "..." instead of "... Ok".

When done, clean up the SQLite repository:

```sh
cd src
make clean
git clean -i  # and delete everything
rm -rf testdir
git checkout amalgamation/sqlite3.c
git checkout amalgamation_dev/sqlite3.c
```
