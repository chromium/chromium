# Updating Rust crates used by Chromium

This document describes how Chromium updates crates.io Rust crates that Chromium
depends on.

## Staffing

We have a
[weekly rotation](https://goto.google.com/chromium-crates-update-rotation) of
Google engineers responsible for creating and landing CLs that update Rust
crates.

Google engineers can join the rotation by emailing
[chrome-safe-coding@google.com](mailto:chrome-safe-coding@google.com).

## Initial setup

The "Rust: periodic update of 3rd-party crates" rotation requires access to an
up-to-date Chromium repo.  One way to start a shift is to run `git fetch`,
`git checkout origin/main`, and `gclient sync` (but other workflows should also
work - e.g. ones based on `git-new-workdir`).

## Checking the state of the world

Before creating a CL stack, check for open CLs with the [`cratesio-autoupdate`
tag](https://chromium-review.googlesource.com/q/hashtag:%22cratesio-autoupdate%22+(status:open%20OR%20status:merged)).
Such CLs tend to conflict, so coordinate with owners of any open CLs.

You may also check a doc with notes from previous rotations, where we may note
known issues and their workarounds.  See (Google-internal, sorry):
https://docs.google.com/document/d/1S7gsrJFsgoU5CH0K7-X_gL55zIIgd6UsFpCGrJqjdAg/edit?usp=sharing

## Automated step: `create_update_cl.py`

The first actual step of the rotation is running `create_update_cl.py`. You must
invoke it from within the `src/` directory of a Chromium repository checkout,
and it depends on `depot_tools` and `git` being present in the `PATH`.

```sh
$ cd ~/chromium/src  # or wherever you have your checkout
$ tools/crates/create_update_cl.py auto
```

In `auto` mode, it runs `gnrt update` to discover crate updates and then for
each update creates a new local git branch (and a Gerrit CL unless invoked with
`--no-upload`). Each branch contains an update created by `gnrt update <old
crate id>`, `gnrt vendor`, and `gnrt gen`. Depending on how many crates are
updated, the script may need 10-15 minutes to run.

The script should Just Work in most cases, but sometimes it may fail when
dealing with a specific crate update.  See [Recovering from script
failures](#recovering-from-script-failures) below for what to do when that
happens.

Before the auto-generated CLs can be landed, you will need to get an LGTM from
`//third_party/rust/OWNERS`.  A review checklist can be found at
`//third_party/rust/OWNERS-review-checklist.md`. If you add
chrome-third-party-rust-reviews@google.com to the "Reviewers" line, an
OWNER will be automatically assigned.

## New transitive dependencies

Notes from `//third_party/rust/OWNERS-review-checklist.md` apply:

* The dependency will need to go through security review.
* An FYI email should be sent to
  [chrome-atls-discuss@google.com](mailto:chrome-atls-discuss@google.com)
  in order to record the addition.

### Optional: Adding the transitive dependency in its own CL

If the new crate is non-trivial, it's possible to split the
additional crate into its own CL, however then it will default to global
visibility and allowing non-test use.
* `gnrt add` and `gnrt vendor` can add the dependency to a fresh checkout.
* Mark the crate as being for third-party code only by setting
  `allow_first_party_usage` to `false` for the crate in
  `third_party/rust/chromium_crates_io/gnrt_config.toml`.
* If the crates making use of the transitive dependency are only allowed
  in tests, then set `group = 'test'` for the crate in
  `third_party/rust/chromium_crates_io/gnrt_config.toml`. This reduces
  the level of security review required for the library.
* `gnrt gen` will then generate the GN rules.
* Rebase the roll CL on top of the changes to make sure the choices made above
  are correct. `gn gen` will fail in CQ if the crate was placed in the `'test'`
  group but needs to be visible outside of tests.

## Potential additional steps

* The `create_update_cl.py` script may stop early if it detects that `gnrt
  vendor` or `gnrt gen` have reported any warnings or errors (e.g. a "License
  file not found for crate foo" warning).  In this case, manual intervention is
  needed to finish the update CL.  It's probably best to finish and land the CLs
  created so far before trying to restart the script in order to create the
  remaining CLs.

## Landing the CL

Other than the above, the CL can go through the normal, plain-vanilla, manual
review and landing process.

1. `git cl upload`
1. Get a review from one of `//third_party/rust/OWNERS`
1. Land the CL using CQ+2

## Checking for new major versions

Note that `create_update_cl.py auto` will by default only handle minor version
updates (e.g.  123.1 => 123.2, or 0.123.1 => 0.123.2).  Major version changes
(e.g. 1.0 => 2.0, which may include breaking API changes and other breaking
changes) need to be handled separately - this section describes what to do.

### Detecting available major version updates (and doing the updates)

As part of the rotation, please do the following:

1. Check for new major versions of
   _direct_ Chromium dependencies (i.e. dependencies directly listed in
  `third_party/rust/chromium_crates_io/Cargo.toml`).  To discover direct _and_
  transitive dependencies with a new major version, you can use the command below
  (running it in the final update CL branch - after all the minor version
  updates):

    ```sh
    $ tools/crates/run_gnrt.py update -- --verbose --dry-run
    ...
       Unchanged serde_json_lenient v0.1.8 (latest: v0.2.0)
       Unchanged syn v1.0.109 (latest: v2.0.53)
    ...
    ```

2. For each detected available major version update, use the script to
   put together a CL that updates the crate (see the "Major version update:
   Workflow A: Single update CL" section below) and then kick of CQ dry run.
   Example invocation:

    ```
    $ tools/crates/create_update_cl.py auto -- font-types read-fonts skrifa --breaking
    Checking out the `origin/main` branch...
    ...
    Creating a major version update CL...
      Running `gnrt update -- font-types read-fonts skrifa --breaking` ...
      ...
      Issue number: 6990318 (https://chromium-review.googlesource.com/6990318)
    ```

3. Depending on whether the CQ dry run succeeds
   (or it's relatively easy to fix errors and make it succeed):

    * If CQ succeeds then get a review from the crate owner (look
      for `//third_party/rust/some_crate_name/OWNERS`) and land the CL.
      Getting a review from the crate owner is important, because sometimes a
      major version bump indicates a breaking behavior change (and CQ may pass
      if there are no breaking API changes and Chromium test coverage doesn't
      exercise the breaking behavior change).
    * Otherwise, if fixing CQ dry run is not straightforward, then please
      open a bug and assign it to the crate owner (asking them to drive the
      update).  For searchability use a bug title like:
      "Rust crate major version update: `some_crate_name`: 123.x => 124.x"

Other notes to help with this part of the rotation:

* Major version updates of sets of interdependent crates may need to be
  atomically (i.e. in a single CL).  For example the 3 Fontations crates
  (`font-types`, `read-fonts`, `skrifa`) need to be updated together.

### Major version update: Workflow A: Single update CL

If updating to a new major version doesn't require lots of Chromium changes,
then it may be possible to land the update in a single CL.  This is typically
possible when the APIs affected by the major version's breaking change either
weren't used by Chromium, or were used only in a handful of places.

**Warning**: Sometimes a new major version may be API compatible, but may
introduce breaking changes in the _behavior_ of the existing APIs.

To update:

1. `tools/crates/create_update_cl.py auto -- some_crate_name --breaking`
1. Follow the manual steps from the minor version update rotation for
   review, landing, etc.

### Major version update: Workflow B: Incremental transition

When lots of first-party code depends on the old major version, then the
transition to the new major version may need to be done incrementally.  In this
case the transition can be split into the following steps:

1. Open a new bug to track the transition
    - TODO: Figure out how to tag/format the bug to make it easy to discover
      in future rotations
1. Land the new major version, so that the old and the new versions coexist.
   To do this follow the process for importing a new crate as described in
   [`docs/rust.md`](../../docs/rust.md#importing-a-crate-from-crates_io)
   (i.e. edit `Cargo.toml` to add the new version, run `gnrt vendor`, and so
   forth).
1. Incrementally transition first-party code to the new major version
1. Remove the old major version.  To do this follow a similar process as above
   (i.e. edit `Cargo.toml` to remove the old version, run `gnrt vendor`, and so
   forth).  Any leftover files in `//third_party/rust/<crate>/<old epoch>`
   should also be removed.

Note that the following `Cargo.toml` syntax allows two versions of a crate to
coexist:

```toml
[dependencies.serde_json_lenient_old_epoch]
package = "serde_json_lenient"
version = "0.1"

[dependencies.serde_json_lenient]
version = "0.2"
```

## Other ways to use `create_update_cl.py`

### `auto` mode

Extra arguments passed to `create_update_cl.py auto` end up being passed to
`cargo update`.  For a complete list of available options, see
[Cargo documentation here](https://doc.rust-lang.org/cargo/commands/cargo-update.html#update-options)),
but the most common scenarios are covered in the sections below.

#### Updating all crates during the weekly rotation

`tools/crates/create_update_cl.py auto` with no extra arguments will attempt to
discover **minor** version updates for **all** crates that Chromium depends on
and for their transitive dependencies.

#### Updating the minor version of a single crate

`tools/crates/create_update_cl.py auto -- some_crate_name` can be used to
trigger a **minor** version update of a single crate.

#### Updating the major version of a single crate

`tools/crates/create_update_cl.py auto -- some_crate_name --breaking` can be
used to trigger a **major** version update of a single crate

### `manual` mode

For maximal control, the script can be used in `manual` mode:

1. Prepare `Cargo.toml` change:
    1. `git checkout origin/main`
    1. `git checkout -b manual-update-of-foo`
    1. Edit `third_party/rust/chromium_crates_io/Cargo.toml` to change the crate
       version of the crate (or crates) you want to update.
       **Important**: Do not edit `Cargo.lock` (e.g. don't run `gnrt vendor`
       etc.).
    1. `git add third_party/rust/chromium_crates_io/Cargo.toml`
    1. `git commit -m "Manual edit of Cargo.toml"`
    1. `git cl upload -m "Manual edit of Cargo.toml" --bypass-hooks --skip-title --force`
1. Run the helper script as follows:
   `tools/crates/create_update_cl.py manual
   --title "Roll foo crate to new version X"`
    - This will run `gnrt vendor` to discover and execute updates that were
      requested by the manual edits of `Cargo.toml` in the previous steps.
    - This will automatically add more details to the CL description
    - To make the review easier, one of the patchsets covers just the path
      changes.  For example - see [the delta here](https://crrev.com/c/5445719/2..7).

<a id="recovering-from-script-failures"></a>
## Recovering from script failures

Sometimes the `create_update_cl.py` script will fail when dealing with
a specific crate update.  The general workflow in this case is to
1) fix the issue in a separate CL, and 2) restart the tool from the middle
by using `--upstream-branch` that points to the last successful update branch
(or to the fix CL) rather than defaulting to `origin/main`.

Examples of a few specific situations that may lead to script failure:

* An update brought in a new crate, but `gnrt` didn't recognize new crate's
  license kind or license file.  In that case a prerequisite CL needs to be
  landed first, teaching `gnrt` about the new license kinds/files
  ([in readme.rs](https://source.chromium.org/chromium/chromium/src/+/main:tools/crates/gnrt/lib/readme.rs;l=264-290;drc=c838bc6c6317d4c1ead1f7f0c615af353482f2b3)).
  You can see [an example CL with such a fix](https://crrev.com/c/6219211).
* Patches from `//third_party/rust/chromium_crates_io/patches/` no longer
  apply cleanly to the new version of a crate.  In that case the crate update CL
  needs to 1) first update the patches, and then 2) update the crate as usual.
  This is not very well supported by the script... But something like this
  should work:
    - Checkout a new branch:
        ```sh
        $ git checkout rust-crates-update--last-successful-update
        $ git checkout -b fix-patches-for-foo
        $ git branch --set-upstream-to=rust-crates-update--last-successful-update
        ```
    - Fix the patches and upload as a temporary / throw-away CL
      (this CL can't be landed on its own - it needs to be combined
      with the actual update CL):
        ```sh
        $ # Get the updated crate code
        $ ./tools/crates/run_gnrt.py update foo
        $ ./tools/crates/run_gnrt.py vendor --no-patches foo
        $ git commit -a -m ...
        $ # Manually apply patches, creating separate commits for patch file updates
        $ git rebase -i # drop everything but the patch file commits
        $ git cl upload
        ```
    - Restart the script (the CL created by the script can't be landed
      as-is / on its own - it needs to be combined with the fixed patches
      in the step below) with `--upstream-branch` parameter:
        ```sh
        $ tools/crates/create_update_cl.py auto \
            --upstream-branch=fix-patches-for-foo \
            -- name-of-failed-crate
        ```
    - Combine the branches:
        ```sh
        $ git map-branches -v # to orient yourself
        $ git checkout rust-crates-update--new-successful-update
        $ git branch --set-upstream-to=rust-crates-update--last-successful-update
        $ git cl upload -m Rebasing... # --bypass-hooks as needed
        ```
* `//third_party/rust/chromium_crates_io/gnrt_config.toml` needs to be updated
  to work with a new crate version.  The same workflow should work as for fixing
  `//third_party/rust/chromium_crates_io/patches/` (see the item above).
