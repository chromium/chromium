This document explains how to perform the project's maintenance tasks.

### Creating a new release

#### Artifacts

* a tag of the version number
* a new [crate version](https://crates.io/crates/flate2/versions)

#### Process

To generate all the artifacts above, one proceeds as follows:

1. `git checkout -b release-<next-version>` - move to a branch to prepare making changes to the repository. *Changes cannot be made to `main` as it is protected.*
2. Edit `Cargo.toml` to the next package version.
3. `gh pr create` to create a new PR for the current branch and **get it merged**.
4. `cargo publish` to create a new release on `crates.io`.
5. `git tag <next-version>` to remember the commit.
6. `git push --tags` to push the new tag.
7. Go to the newly created release page on GitHub and edit it by pressing the "Generate Release Notes" and the `@` button. Save the release.

