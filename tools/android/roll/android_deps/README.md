Android Deps Repository Generator
---------------------------------

Tool to generate a gradle-specified repository for Android and Java
dependencies.

### Usage

    fetch_all.py [--help]

This script creates a temporary build directory, where it will, for each
of the dependencies specified in `build.gradle`, take care of the following:

  - Download the library
  - Generate a README.chromium file
  - Download the LICENSE
  - Generate a GN target in BUILD.gn
  - Generate .info files for AAR libraries
  - Generate CIPD yaml files describing the packages
  - Generate a `deps` entry in DEPS.

It will then compare the build directory with your current workspace, and
print the differences (i.e. new/updated/deleted packages names).

### Adding a new library
Full steps to add a new third party library:

1. Add dependency to `build.gradle`

2. Run `fetch_all.py` to verify the new `build.gradle` file and that all
   dependencies could be downloaded properly. This does not modify your
   workspace. If not, fix your `build.gradle` file and try again.

3. Run `fetch_all.py --update-all` to update your current workspace with the
   changes. This will update, among other things, your top-level DEPS file,
   and print a series of commands to create new CIPD packages.
    - Before running this command, make sure to increment the `CIPD_SUFFIX`
      variable in `BuildConfigGenerator.groovy` in order to ensure that each
      tag in CIPD is unique.
      - One option to thoroughly test your change is to run
        `rm -rf third_party/android_deps/libs/[!O]*` before running with
        `--update-all`. This will ensure all your deps are fresh. The commands
        printed out in the following step will ensure you do not upload
        duplicate instances.

4. Run the commands printed at step 3 to create new and updated packages
   via cipd.
    - In order to do this, you will need write access. Ask anyone from this
      [list][owners_link] to add you to the group.
    - Just make sure to copy the commands exactly, as uploading an instance
      with the same package and tag as an existing instance currently in use
      can break all builds, and there is no easy way to delete an instance.
      - If this happens, file an infra-trooper bug immediately.

5. Create a commit & follow [`//docs/adding_to_third_party.md`][docs_link] for
   the review.
    - This is not necessary if you are only upgrading existing packages or
      adding packages from the same source and license (e.g. gms)

Note that if you're not satisfied with the results, you can use
`fetch_all.py --reset-workspace` to reset your workspace to its HEAD state,
including the original CIPD symlinks under
`third_party/android_deps/repository/`. This commands preserves local
`build.gradle` modifications.

If you are updating any of the gms dependencies, please ensure that the license
file that they use, explained in the [README.chromium][readme_chromium_link] is
up-to-date with the one on android's [website][android_sdk_link], last updated
date is at the bottom.

[owners_link]: http://go/android-deps-owners
[docs_link]: ../../../../docs/adding_to_third_party.md
[android_sdk_link]: https://developer.android.com/studio/terms
[readme_chromium_link]: ../../../../third_party/android_deps/README.chromium

### Implementation notes:
The script invokes a Gradle plugin to leverage its dependency resolution
features. An alternative way to implement it is to mix gradle to purely fetch
dependencies and their pom.xml files, and use Python to process and generate
the files. This approach was not as successful, as some information about the
dependencies does not seem to be available purely from the POM file, which
resulted in expecting dependencies that gradle considered unnecessary.
