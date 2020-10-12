This is the top folder for Chromium's SQLite. The actual SQLite source is not
in this repository, but instead cloned into the `src` directory from
https://chromium.googlesource.com/chromium/deps/sqlite.

The directory structure is as follows. Files common to all third_party projects
(BUILD.GN, OWNERS, LICENSE) are omitted.

* `src/`     The Chromium fork of SQLite (cloned via top level DEPS file).
* `scripts/` Scripts that generate the files in the amalgamations in src/.
* `sqlite.h` The header used by the rest of Chromium to include SQLite. This
             forwards to src/amalgamation/sqlite3.h
* `fuzz/`    Google OSS-Fuzz (ClusterFuzz) testing for Chromium's SQLite
             build This directory contains:

The SQLite amalgamation is committed to the SQLite Chromium repository (in
`src`), but it is created by a script that lives in the Chromium repository.
This is because the configuration variables for building and amalgamation
generation are shared.

There are two amalgamations. The one in //third_party/sqlite/src/amalgamation
is used by Chromium. A second, located at
//third_party/sqlite/src/amalgamation_dev is used for some Chromium developer
tools and is not distributed.

# Upgrade to a new SQLite release.

**Note** SQLite tags all releases `version-<release number>`, e.g.
`version-3.33.0`. The Chromium project prefixes all tags/branches with
"chromium-", e.g.  `chromium-version-3.33.0`.

1. Create new release branch

   Use the SQLite commit ID when creating a branch. For example
   "562fd18b9dc27216191c0a6477bba9b175f7f0d2" corresponds to the
   3.33.0 release. The commit is used, instead of the tag name, because
   we do not mirror the SQLite tags along with the commits. The correct
   commit ID can be found at
   [sqlite/releases](https://github.com/sqlite/sqlite/releases).

   Create the branch at
   [Gerrit/branches](https://chromium-review.googlesource.com/admin/repos/chromium/deps/sqlite,branches).

2. Checkout the new Chromium release branch.

    ```sh
    cd //third_party/sqlite/src
    git fetch origin
    export VERSION=3.33.0
    git checkout -b chromium-version-$VERSION \
        --track origin/chromum-version-$VERSION
    ```

3. Generate and commit the SQLite amalgamations.

    ```sh
    ../scripts/generate_amalgamation.py
    git add amalgamation
    git add amalgamation_dev
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

    1. Roll the `chromium/src/DEPS` file to reference that new commit ID.
        ```sh
        roll-dep src/third_party/sqlite/src --roll-to <git hash of merged CL>
        ```
    2. Update the version in //third_party/sqlite/README.chromium. Append the
       commit created by roll-dep above.

# Cherry-pick unreleased commit from SQLite.

Sometimes **critical fixes** land in SQLite's master, but are not yet in a
release. If these need to be brought into the current release branch do the
following:

1. Checkout the current release branch.

    ```sh
    export VERSION=3.33.0
    cd //third_party/sqlite/src
    git checkout -b chromium-version-$VERSION \
      --track origin/chromium-version-$VERSION
    ```

2. Cherry-pick the change

    Git _can_ be used to cherry pick upstream changes into a release branch but
    the sqlite_cherry_picker.py script is preferred. This script automates a
    few tasks such as:

    * Identifying the correct Git commit ID to use if given the
      Fossil commit ID.
    * Automatically calculating Fossil manifest hashes.
    * Skipping conflicted binary files.
    * Generating the amalgamations.

    Cherry-pick the commit:

    ```sh
    ../scripts/sqlite_cherry_picker.py <full git or fossil commit id>
    ```

    If there is a conflict that the script cannot resolve then, like
    git-cherry-pick, the script will exit and leave you to resolve the
    conflicts. Once resolved run the script a second time:

    ```sh
    ../scripts/sqlite_cherry_picker.py --continue
    ```

3. Run local tests.

    Follow steps in [Running Tests](#running-tests) below to execute all
    verifications and tests.

4. Upload cherry-picked change (with amalgamations) for review.

    ```sh
    git cl upload
    ```

5. Update the Chromium DEPS file.

    Once review above has merged, roll the `chromium/src/DEPS` file to
    reference that new commit ID.

    ```sh
    roll-dep src/third_party/sqlite/src --roll-to <git hash of merged CL>
    ```

# Running Tests

Build all desktop targets:

Check that `extract_sqlite_api.py` added "chrome_" to all exported symbols.
Only "_fini" and "_init" should be unprefixed, but are conditionally
exported by the linker and may be omitted.

```sh
autoninja -C out/Default
nm -B out/Default/libchromium_sqlite3.so | cut -c 18- | sort | grep '^T'
```

## Running unit tests

```sh
out/Default/sql_unittests
```

## Running web tests

```sh
third_party/blink/tools/run_web_tests.py -t Default storage/websql/
```

## Running SQLite's TCL test suite within the Chromium checkout.

This is one of the [SQLite test suites](https://www.sqlite.org/testing.html).
They take approximately 3 minutes to build and run on a fast workstation.

```sh
cd //third_party/sqlite
./scripts/generate_amalgamation.py --testing
make --directory=src test | tee /tmp/test.log
```

**Note**: Tests may fail on Chromium release branches. This is because some
tests rely on SQLite databases (binary files) which are committed to the
source and are likely not merged down when cherry picked. It is safe to
ignore these errors which should be reasonably easy to identify based on the
cherry picked upstream changes.

Show error'ed tests:

```sh
egrep 'errors out of' /tmp/test.log
```

Show broken tests:

```sh
egrep 'Failures on these tests:' /tmp/test.log
```

Broken tests will also show lines ending in "..." instead of "... Ok".

When done cleanup the SQLite repository:

```sh
cd src
make clean
git clean -i  # and delete everything
rm -rf testdir
git checkout amalgamation/sqlite3.c
git checkout amalgamation_dev/sqlite3.c
```
